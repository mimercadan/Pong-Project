/*
 * PongServer: servidor del juego Pong multijugador.
 *
 * Uso: ./server <PORT> <LogFile>
 *
 * Cómo está organizado:
 *  - main() abre el socket TCP y acepta clientes.
 *  - Cada cliente tiene su propio hilo (client_thread) que lee sus
 *    mensajes y responde según el tipo (REGISTER, JOIN, MOVE, ...).
 *  - Cada partida tiene otro hilo (match_loop.c) que mueve la bola
 *    ~60 veces por segundo y llama a on_match_event para enviar el
 *    estado a los dos jugadores.
 *  - Toda la lógica del juego vive en engine.c; aquí solo se conecta
 *    esa lógica con la red.
 */
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
#include "match_loop.h"
#include "protocol.h"

#define MAX_CLIENTS 64
#define IDLE_TIMEOUT_SECONDS 30
#define SEND_TIMEOUT_SECONDS 2
#define STATE_LOG_EVERY 60      /* registrar un STATE por segundo por partida */

/* Un solo motor de juego compartido por todos los hilos. */
static engine_t engine;

typedef struct {
    int  fd;
    int  id;                    /* también es el player_id del jugador */
    char ip[INET_ADDRSTRLEN];
    int  port;
    pthread_mutex_t send_lock;  /* evita que dos hilos escriban a la vez */
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

/* ------------------------------------------------------------------ */
/*  Envío de mensajes                                                  */
/* ------------------------------------------------------------------ */

/*
 * Enviar un mensaje a un cliente.
 * Al mismo socket pueden escribir dos hilos: el del propio cliente
 * (respuestas) y el de su partida (STATE). El send_lock evita que
 * los bytes de los dos mensajes se mezclen.
 */
static int send_to_client(client_t *c, uint8_t type,
                          const uint8_t *payload, int len) {
    if (len < 0) return -1;

    pthread_mutex_lock(&c->send_lock);
    int result = send_message(c->fd, type, payload, (uint16_t)len);
    pthread_mutex_unlock(&c->send_lock);

    /* STATE sale ~60 veces por segundo: se resume en on_match_event. */
    if (type != MSG_STATE) {
        if (result == 0)
            log_msg("SEND", "Cliente #%d <- %s len=%d", c->id, proto_type_name(type), len);
        else
            log_msg("WARN", "Cliente #%d: no se pudo enviar %s", c->id, proto_type_name(type));
    }
    return result;
}

/*
 * Enviar un mensaje a un jugador usando solo su id.
 * Se mantiene clients_lock mientras se envía para que el cliente no
 * se borre a mitad del envío. Si ya se desconectó, no se hace nada.
 */
static void send_to_player(int player_id, uint8_t type,
                           const uint8_t *payload, int len) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->id == player_id) {
            send_to_client(clients[i], type, payload, len);
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

static void send_error(client_t *c, uint8_t code) {
    uint8_t payload[MAX_PAYLOAD];
    send_to_client(c, MSG_ERROR, payload, proto_build_error(payload, code));
}

/* Enviar GAME_OVER a los dos jugadores de una partida. */
static void send_game_over(const game_t *game, uint8_t reason) {
    uint8_t payload[MAX_PAYLOAD];
    int len = proto_build_game_over(payload, game, reason);

    send_to_player(game->player_left_id, MSG_GAME_OVER, payload, len);
    send_to_player(game->player_right_id, MSG_GAME_OVER, payload, len);

    log_msg("INFO", "Partida #%d terminada %d-%d, ganador jugador #%d (%s)",
            game->id, game->score_left, game->score_right, game->winner_id,
            reason == PROTO_END_SCORE ? "por puntos" : "por abandono");
}

/*
 * La llama el hilo de cada partida (match_loop.c) después de cada paso.
 *  - Siempre: envía STATE a los dos jugadores.
 *  - Si alguien anotó: lo registra como evento.
 *  - Si la partida terminó por puntos: envía GAME_OVER.
 */
static void on_match_event(const game_t *game, match_event_t event,
                           unsigned long tick) {
    uint8_t payload[MAX_PAYLOAD];
    int len = proto_build_state(payload, game);

    send_to_player(game->player_left_id, MSG_STATE, payload, len);
    send_to_player(game->player_right_id, MSG_STATE, payload, len);

    /* Registrar un STATE por segundo (no 60) para que el log se pueda leer. */
    if (tick % STATE_LOG_EVERY == 1 || event != MATCH_EVENT_TICK) {
        log_msg("SEND", "Partida #%d -> STATE a #%d y #%d: bola=(%.0f,%.0f) "
                "paletas=(%.0f,%.0f) marcador %d-%d",
                game->id, game->player_left_id, game->player_right_id,
                game->ball_x, game->ball_y,
                game->paddle_left_y, game->paddle_right_y,
                game->score_left, game->score_right);
    }

    if (event == MATCH_EVENT_POINT) {
        log_msg("INFO", "Partida #%d: punto, marcador %d-%d",
                game->id, game->score_left, game->score_right);
    }

    if (event == MATCH_EVENT_OVER) {
        send_game_over(game, PROTO_END_SCORE);
    }
}

/* ------------------------------------------------------------------ */
/*  Atender cada tipo de mensaje                                       */
/* ------------------------------------------------------------------ */

/* Saber si esta conexión ya se registró como jugador. */
static int is_registered(client_t *c) {
    player_t player;
    return engine_get_player(&engine, c->id, &player) == PLAYER_OK;
}

/* REGISTER: guardar nickname y correo; el id del jugador es el de la conexión. */
static void handle_register(client_t *c, const uint8_t *payload, uint16_t len) {
    char nickname[PLAYER_NICKNAME_MAX + 1];
    char email[PLAYER_EMAIL_MAX + 1];

    if (proto_parse_register(payload, len, nickname, email) != 0) {
        send_error(c, PROTO_ERR_BAD_MESSAGE);
        return;
    }

    player_result_t result = engine_register(&engine, c->id, nickname, email);

    if (result == PLAYER_OK)
        log_msg("INFO", "Cliente #%d registrado como \"%s\" (%s)", c->id, nickname, email);

    uint8_t response[MAX_PAYLOAD];
    send_to_client(c, MSG_REGISTER_RESULT, response,
                   proto_build_register_result(response, result, c->id));
}

/* Avisar a los dos jugadores que empezó su partida y arrancar su hilo. */
static void start_match(int match_id) {
    game_t game;
    player_t left, right;

    if (engine_get_match(&engine, match_id, &game) != 0 ||
        engine_get_player(&engine, game.player_left_id, &left) != PLAYER_OK ||
        engine_get_player(&engine, game.player_right_id, &right) != PLAYER_OK) {
        log_msg("WARN", "Partida #%d: un jugador se fue antes de empezar", match_id);
        return;
    }

    /* Cada jugador recibe su lado y el nickname de su rival. */
    uint8_t payload[MAX_PAYLOAD];
    int len;

    len = proto_build_match_start(payload, match_id, PROTO_SIDE_LEFT, right.nickname);
    send_to_player(left.id, MSG_MATCH_START, payload, len);

    len = proto_build_match_start(payload, match_id, PROTO_SIDE_RIGHT, left.nickname);
    send_to_player(right.id, MSG_MATCH_START, payload, len);

    /* MATCH_START va primero; después empiezan a llegar los STATE. */
    if (match_loop_run_detached(&engine, match_id, on_match_event) != 0) {
        log_msg("ERROR", "Partida #%d: no se pudo crear su hilo", match_id);
        return;
    }

    log_msg("INFO", "Partida #%d iniciada: \"%s\" (#%d) vs \"%s\" (#%d)",
            match_id, left.nickname, left.id, right.nickname, right.id);
}

/* JOIN: entrar a la cola. Si ya había alguien esperando, empieza la partida. */
static void handle_join(client_t *c) {
    if (!is_registered(c)) {
        send_error(c, PROTO_ERR_NOT_REGISTERED);
        return;
    }

    int match_id = 0;
    lobby_result_t result = engine_join(&engine, c->id, &match_id);

    switch (result) {
    case LOBBY_WAITING:
        log_msg("INFO", "Cliente #%d esperando rival", c->id);
        send_to_client(c, MSG_WAITING, NULL, 0);
        break;
    case LOBBY_MATCH_STARTED:
        start_match(match_id);
        break;
    case LOBBY_ERROR_FULL:
        send_error(c, PROTO_ERR_SERVER_FULL);
        break;
    default:
        /* Ya estaba esperando o jugando. */
        send_error(c, PROTO_ERR_INVALID_STATE);
        break;
    }
}

/*
 * MOVE: guardar hacia dónde quiere mover su paleta.
 * No se responde nada: el jugador ve el resultado en el siguiente STATE.
 */
static void handle_move(client_t *c, const uint8_t *payload, uint16_t len) {
    move_direction_t direction;

    if (proto_parse_move(payload, len, &direction) != 0) {
        log_msg("RECV", "Cliente #%d -> MOVE inválido", c->id);
        send_error(c, PROTO_ERR_BAD_MESSAGE);
        return;
    }

    log_msg("RECV", "Cliente #%d -> MOVE %s", c->id,
            direction == MOVE_UP ? "arriba" :
            direction == MOVE_DOWN ? "abajo" : "detener");

    if (engine_set_input(&engine, c->id, direction) != 0) {
        send_error(c, is_registered(c) ? PROTO_ERR_INVALID_STATE
                                       : PROTO_ERR_NOT_REGISTERED);
    }
}

/*
 * El cliente se fue (QUIT, cerró o timeout).
 * Si estaba jugando, pierde la partida y el rival recibe GAME_OVER.
 */
static void handle_disconnect(client_t *c) {
    game_t result;
    int match_finished = engine_disconnect(&engine, c->id, &result);

    if (match_finished == 1) {
        /* Si la partida ya iba a terminar por puntos, se respeta ese motivo. */
        int by_score = result.score_left >= WINNING_SCORE ||
                       result.score_right >= WINNING_SCORE;

        send_game_over(&result, by_score ? PROTO_END_SCORE : PROTO_END_FORFEIT);
    }
}

/* ------------------------------------------------------------------ */
/*  Hilo de cada cliente                                               */
/* ------------------------------------------------------------------ */

static void *client_thread(void *arg) {
    client_t *c = arg;
    msg_header_t h;
    uint8_t payload[MAX_PAYLOAD];
    int r;

    log_msg("INFO", "Cliente #%d conectado desde %s:%d", c->id, c->ip, c->port);

    /* Si en 30 s no llega nada (ni un PING), se cierra la conexión. */
    struct timeval recv_timeout = { .tv_sec = IDLE_TIMEOUT_SECONDS, .tv_usec = 0 };
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof recv_timeout);

    /* Un cliente que no lee no puede bloquear al servidor más de 2 s. */
    struct timeval send_timeout = { .tv_sec = SEND_TIMEOUT_SECONDS, .tv_usec = 0 };
    setsockopt(c->fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof send_timeout);

    while ((r = recv_message(c->fd, &h, payload)) == 1) {
        /* MOVE se registra en handle_move con la dirección. */
        if (h.type != MSG_MOVE)
            log_msg("RECV", "Cliente #%d -> %s len=%u", c->id, proto_type_name(h.type), h.length);

        if (h.type == MSG_QUIT && h.length == 0) {
            log_msg("INFO", "Cliente #%d envió QUIT", c->id);
            break;
        }

        switch (h.type) {
        case MSG_REGISTER:
            handle_register(c, payload, h.length);
            break;
        case MSG_JOIN:
            if (h.length == 0) handle_join(c);
            else send_error(c, PROTO_ERR_BAD_MESSAGE);
            break;
        case MSG_MOVE:
            handle_move(c, payload, h.length);
            break;
        case MSG_PING:
            /* Solo mantiene viva la conexión; no se responde. */
            break;
        default:
            send_error(c, PROTO_ERR_BAD_MESSAGE);
            break;
        }
    }

    /* r == 1 significa que salimos por QUIT. */
    if (r == 0)
        log_msg("INFO", "Cliente #%d cerró la conexión", c->id);
    else if (r < 0)
        log_msg("WARN", "Cliente #%d: mensaje inválido, timeout o error", c->id);

    /* Primero se saca de la lista: así ningún otro hilo le vuelve a enviar. */
    int id = c->id;
    remove_client(c);

    /* Luego se resuelve su partida (si tenía) y se avisa al rival. */
    handle_disconnect(c);

    close(c->fd);
    pthread_mutex_destroy(&c->send_lock);
    free(c);

    log_msg("INFO", "Cliente #%d: conexión terminada", id);
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Programa principal                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <PORT> <LogFile>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Puerto inválido: %s\n", argv[1]);
        return 1;
    }

    if (logger_init(argv[2]) < 0) {
        perror("No se pudo abrir el log");
        return 1;
    }

    if (engine_init(&engine) != 0) {
        fprintf(stderr, "No se pudo inicializar el motor\n");
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
        if (!c) { close(fd); continue; }

        c->fd = fd;
        inet_ntop(AF_INET, &caddr.sin_addr, c->ip, sizeof c->ip);
        c->port = ntohs(caddr.sin_port);
        pthread_mutex_init(&c->send_lock, NULL);

        if (add_client(c) < 0) {
            log_msg("WARN", "Servidor lleno, rechazando %s", c->ip);
            pthread_mutex_destroy(&c->send_lock);
            close(fd); free(c); continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, c) != 0) {
            log_msg("ERROR", "No se pudo crear el hilo del cliente #%d", c->id);
            remove_client(c);
            pthread_mutex_destroy(&c->send_lock);
            close(fd); free(c); continue;
        }
        pthread_detach(tid);
    }

    logger_close();
    return 0;
}
