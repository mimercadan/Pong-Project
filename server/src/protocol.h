#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "game.h"
#include "players.h"

/* MyAppGameProtocol: contenido (payload) de cada mensaje.
   El encabezado de 4 bytes lo arma y lo lee net.c (send_message/recv_message).
   Todos los números van en orden de red (big-endian). */

/* Tipos de mensaje (byte "tipo" del encabezado).
   Cliente -> servidor: 0x01..0x7F. Servidor -> cliente: 0x81..0xFF. */
enum {
    MSG_REGISTER        = 0x01, /* nick_len 1B | nick | email_len 1B | email */
    MSG_JOIN            = 0x02, /* sin payload: buscar partida */
    MSG_MOVE            = 0x03, /* direccion 1B */
    MSG_PING            = 0x04, /* sin payload: "sigo aquí", el servidor lo ignora */
    MSG_QUIT            = 0x05, /* sin payload: el cliente se va */

    MSG_REGISTER_RESULT = 0x81, /* resultado 1B | player_id 4B */
    MSG_WAITING         = 0x82, /* sin payload: en cola esperando rival */
    MSG_MATCH_START     = 0x83, /* match_id 4B | lado 1B | nick_len 1B | nick rival */
    MSG_STATE           = 0x84, /* ball_x 2B | ball_y 2B | pal_izq_y 2B | pal_der_y 2B | pts_izq 1B | pts_der 1B */
    MSG_GAME_OVER       = 0x85, /* winner_id 4B | pts_izq 1B | pts_der 1B | motivo 1B */
    MSG_ERROR           = 0x86  /* codigo 1B */
};

/* Dirección en MOVE (en la red no se usan negativos). */
enum { PROTO_MOVE_STOP = 0, PROTO_MOVE_UP = 1, PROTO_MOVE_DOWN = 2 };

/* Resultado en REGISTER_RESULT. */
enum {
    PROTO_REG_OK      = 0,
    PROTO_REG_INVALID = 1,
    PROTO_REG_TAKEN   = 2, /* esta conexión ya estaba registrada */
    PROTO_REG_FULL    = 3
};

/* Lado en MATCH_START. */
enum { PROTO_SIDE_LEFT = 0, PROTO_SIDE_RIGHT = 1 };

/* Motivo en GAME_OVER. */
enum { PROTO_END_SCORE = 0, PROTO_END_FORFEIT = 1 };

/* Código en ERROR. */
enum {
    PROTO_ERR_BAD_MESSAGE    = 1, /* tipo desconocido o payload mal formado */
    PROTO_ERR_NOT_REGISTERED = 2, /* JOIN o MOVE antes de REGISTER */
    PROTO_ERR_INVALID_STATE  = 3, /* mensaje que no aplica ahora (p. ej. MOVE sin partida) */
    PROTO_ERR_SERVER_FULL    = 4  /* no hay cupo en el lobby */
};

/* Tamaños fijos de payload. */
#define PROTO_MOVE_LEN            1
#define PROTO_REGISTER_RESULT_LEN 5
#define PROTO_STATE_LEN           10
#define PROTO_GAME_OVER_LEN       7
#define PROTO_ERROR_LEN           1

/* ---- Leer (cliente -> servidor) ----
   Devuelven 0 si el payload es válido y -1 si está mal formado
   (en ese caso responder ERROR con PROTO_ERR_BAD_MESSAGE). */

/* Copia nickname y email terminados en '\0'. Solo valida la estructura;
   el contenido (espacios, arroba) lo valida players_register. */
int proto_parse_register(const uint8_t *payload, uint16_t len,
                         char nickname[PLAYER_NICKNAME_MAX + 1],
                         char email[PLAYER_EMAIL_MAX + 1]);

int proto_parse_move(const uint8_t *payload, uint16_t len,
                     move_direction_t *out_direction);

/* ---- Armar (servidor -> cliente) ----
   Escriben en out (al menos MAX_PAYLOAD bytes) y devuelven la longitud del
   payload, lista para send_message(fd, MSG_..., out, len). -1 si hay error. */

int proto_build_register_result(uint8_t *out, player_result_t result,
                                int player_id);

int proto_build_match_start(uint8_t *out, int match_id, int side,
                            const char *opponent_nickname);

/* Convierte las posiciones float de game_t a píxeles enteros. */
int proto_build_state(uint8_t *out, const game_t *game);

int proto_build_game_over(uint8_t *out, const game_t *game, uint8_t reason);

int proto_build_error(uint8_t *out, uint8_t code);

/* Nombre legible del tipo para el logger ("REGISTER", "STATE", ...). */
const char *proto_type_name(uint8_t type);

#endif
