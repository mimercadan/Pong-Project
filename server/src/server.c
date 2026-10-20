#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "logger.h"
#include "net.h"
#include "engine.h"
#include "protocol.h"

/* Un motor compartido por los hilos del servidor. */
static engine_t engine;

#define MAX_CLIENTS 64

typedef struct {
    int  fd;
    int  id;
    char ip[INET_ADDRSTRLEN];
    int  port;
} client_t;

/* Lista global de clientes conectados, protegida con mutex */
static client_t *clients[MAX_CLIENTS];
static pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
static int next_id = 1;

static int add_client(client_t *c) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) {
            c->id = next_id++;
            clients[i] = c;
            pthread_mutex_unlock(&clients_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return -1; /* servidor lleno */
}

static void remove_client(client_t *c) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] == c) { clients[i] = NULL; break; }
    pthread_mutex_unlock(&clients_lock);
}

/* Un hilo por cliente: primera etapa de integración del protocolo. */
static void *client_thread(void *arg) {
    client_t *c = arg;
    msg_header_t h;

    uint8_t payload[MAX_PAYLOAD];
    uint8_t response[MAX_PAYLOAD];
    int r;

    log_msg("INFO", "Cliente #%d conectado desde %s:%d",
            c->id, c->ip, c->port);

    struct timeval tv = {
        .tv_sec = 30,
        .tv_usec = 0
    };

    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    while ((r = recv_message(c->fd, &h, payload)) == 1) {
        log_msg("RECV", "Cliente #%d -> %s len=%u",
                c->id, proto_type_name(h.type), h.length);

        int len;
        uint8_t response_type;

        if (h.type == MSG_REGISTER) {
            char nickname[PLAYER_NICKNAME_MAX + 1];
            char email[PLAYER_EMAIL_MAX + 1];

            if (proto_parse_register(
                payload, h.length, nickname, email
            ) != 0) {
                response_type = MSG_ERROR;
                len = proto_build_error(
                    response, PROTO_ERR_BAD_MESSAGE
                );
            } else {
                player_result_t result = engine_register(
                    &engine, c->id, nickname, email
                );

                response_type = MSG_REGISTER_RESULT;

                len = proto_build_register_result(
                    response, result, c->id
                );
            }
        } else if (h.type == MSG_PING && h.length == 0) {
            /* Mantener la conexión sin responder al PING. */
            continue;
        } else if (h.type == MSG_QUIT && h.length == 0) {
            break;
        } else {
            /*
             * JOIN y MOVE se conectarán en la siguiente etapa.
             * Por ahora se rechazan los mensajes no implementados.
             */
            response_type = MSG_ERROR;
            len = proto_build_error(
                response, PROTO_ERR_BAD_MESSAGE
            );
        }

        if (len < 0 || send_message(
            c->fd,
            response_type,
            response,
            (uint16_t)len
        ) != 0) {
            break;
        }

        log_msg("SEND", "Cliente #%d <- %s len=%d",
                c->id, proto_type_name(response_type), len);
    }

    /* Retirar el perfil si la conexión llegó a registrarse. */
    game_t result;
    engine_disconnect(&engine, c->id, &result);

    log_msg("INFO", "Cliente #%d: conexión terminada", c->id);

    remove_client(c);
    close(c->fd);
    free(c);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <PORT> <LogFile>\n", argv[0]);
        return 1;
    }

    if (engine_init(&engine) != 0) {
    fprintf(stderr, "No se pudo inicializar el motor\n");
    return 1;
 }
    int port = atoi(argv[1]);

    if (logger_init(argv[2]) < 0) {
        perror("No se pudo abrir el log");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN); /* que no se caiga si un cliente cierra de golpe */

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 16) < 0) { perror("listen"); return 1; }

    log_msg("INFO", "PongServer escuchando en el puerto %d", port);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof caddr;
        int fd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (fd < 0) { log_msg("ERROR", "accept falló"); continue; }

        client_t *c = calloc(1, sizeof *c);
        c->fd = fd;
        inet_ntop(AF_INET, &caddr.sin_addr, c->ip, sizeof c->ip);
        c->port = ntohs(caddr.sin_port);

        if (add_client(c) < 0) {
            log_msg("WARN", "Servidor lleno, rechazando %s", c->ip);
            close(fd); free(c); continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, c);
        pthread_detach(tid);
    }

    logger_close();
    return 0;
}
