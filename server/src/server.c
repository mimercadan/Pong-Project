#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "logger.h"

#define MAX_CLIENTS 64
#define BUF_SIZE    1024

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

/* Un hilo por cliente */
static void *client_thread(void *arg) {
    client_t *c = arg;
    unsigned char buf[BUF_SIZE];
    ssize_t n;

    log_msg("INFO", "Cliente #%d conectado desde %s:%d", c->id, c->ip, c->port);

    while ((n = recv(c->fd, buf, sizeof buf, 0)) > 0) {
        log_msg("RECV", "Cliente #%d -> %zd bytes", c->id, n);

        /* TODO: aquí va el parseo del protocolo (lo entrega la persona 3)
           y el llamado a la lógica del juego (persona 2).
           Por ahora hace eco para poder probar. */
        send(c->fd, buf, n, 0);
        log_msg("SEND", "Cliente #%d <- %zd bytes", c->id, n);
    }

    if (n == 0) log_msg("INFO",  "Cliente #%d se desconectó", c->id);
    else        log_msg("ERROR", "Cliente #%d error en recv", c->id);

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
