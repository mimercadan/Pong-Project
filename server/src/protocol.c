#include "protocol.h"
#include "net.h"

#include <string.h>
#include <arpa/inet.h>

/* Escribir enteros en orden de red sin depender de la alineación. */
static void put_u16(uint8_t *out, uint16_t value) {
    uint16_t net = htons(value);
    memcpy(out, &net, 2);
}

static void put_u32(uint8_t *out, uint32_t value) {
    uint32_t net = htonl(value);
    memcpy(out, &net, 4);
}

/* Redondear una posición a píxeles enteros dentro de 0..65535. */
static uint16_t to_pixels(float value) {
    if (!(value > 0.0f)) {
        return 0;               /* también cubre NaN */
    }
    if (value >= 65535.0f) {
        return 65535;
    }
    return (uint16_t)(value + 0.5f);
}

static uint8_t to_score(int score) {
    if (score < 0) {
        return 0;
    }
    return score > 255 ? 255 : (uint8_t)score;
}

/* Copiar un texto de la red: largo 1..max y sin '\0' en medio. */
static int read_text(const uint8_t *src, uint8_t text_len, size_t max,
                     char *dst) {
    if (text_len == 0 || text_len > max || memchr(src, '\0', text_len)) {
        return -1;
    }
    memcpy(dst, src, text_len);
    dst[text_len] = '\0';
    return 0;
}

int proto_parse_register(const uint8_t *payload, uint16_t len,
                         char nickname[PLAYER_NICKNAME_MAX + 1],
                         char email[PLAYER_EMAIL_MAX + 1]) {
    if (payload == NULL || nickname == NULL || email == NULL || len < 1) {
        return -1;
    }

    uint8_t nick_len = payload[0];
    /* Después del nickname debe venir al menos el byte email_len. */
    if ((size_t)1 + nick_len + 1 > len) {
        return -1;
    }

    uint8_t email_len = payload[1 + nick_len];
    if ((size_t)1 + nick_len + 1 + email_len != len) {
        return -1;
    }

    if (read_text(payload + 1, nick_len, PLAYER_NICKNAME_MAX, nickname) < 0 ||
        read_text(payload + 2 + nick_len, email_len, PLAYER_EMAIL_MAX,
                  email) < 0) {
        return -1;
    }
    return 0;
}

int proto_parse_move(const uint8_t *payload, uint16_t len,
                     move_direction_t *out_direction) {
    if (payload == NULL || out_direction == NULL || len != PROTO_MOVE_LEN) {
        return -1;
    }

    switch (payload[0]) {
    case PROTO_MOVE_STOP: *out_direction = MOVE_STOP; return 0;
    case PROTO_MOVE_UP:   *out_direction = MOVE_UP;   return 0;
    case PROTO_MOVE_DOWN: *out_direction = MOVE_DOWN; return 0;
    default:              return -1;
    }
}

int proto_build_register_result(uint8_t *out, player_result_t result,
                                int player_id) {
    if (out == NULL) {
        return -1;
    }

    uint8_t code;
    switch (result) {
    case PLAYER_OK:                       code = PROTO_REG_OK;      break;
    case PLAYER_ERROR_ALREADY_REGISTERED: code = PROTO_REG_TAKEN;   break;
    case PLAYER_ERROR_FULL:               code = PROTO_REG_FULL;    break;
    default:                              code = PROTO_REG_INVALID; break;
    }

    out[0] = code;
    /* Si el registro falló, el id no significa nada: se envía 0. */
    put_u32(out + 1, code == PROTO_REG_OK && player_id > 0
                         ? (uint32_t)player_id : 0);
    return PROTO_REGISTER_RESULT_LEN;
}

int proto_build_match_start(uint8_t *out, int match_id, int side,
                            const char *opponent_nickname) {
    if (out == NULL || opponent_nickname == NULL || match_id <= 0 ||
        (side != PROTO_SIDE_LEFT && side != PROTO_SIDE_RIGHT)) {
        return -1;
    }

    size_t nick_len = strlen(opponent_nickname);
    if (nick_len > PLAYER_NICKNAME_MAX) {
        return -1;
    }

    put_u32(out, (uint32_t)match_id);
    out[4] = (uint8_t)side;
    out[5] = (uint8_t)nick_len;
    memcpy(out + 6, opponent_nickname, nick_len);
    return (int)(6 + nick_len);
}

int proto_build_state(uint8_t *out, const game_t *game) {
    if (out == NULL || game == NULL) {
        return -1;
    }

    put_u16(out + 0, to_pixels(game->ball_x));
    put_u16(out + 2, to_pixels(game->ball_y));
    put_u16(out + 4, to_pixels(game->paddle_left_y));
    put_u16(out + 6, to_pixels(game->paddle_right_y));
    out[8] = to_score(game->score_left);
    out[9] = to_score(game->score_right);
    return PROTO_STATE_LEN;
}

int proto_build_game_over(uint8_t *out, const game_t *game, uint8_t reason) {
    if (out == NULL || game == NULL ||
        (reason != PROTO_END_SCORE && reason != PROTO_END_FORFEIT)) {
        return -1;
    }

    put_u32(out, game->winner_id > 0 ? (uint32_t)game->winner_id : 0);
    out[4] = to_score(game->score_left);
    out[5] = to_score(game->score_right);
    out[6] = reason;
    return PROTO_GAME_OVER_LEN;
}

int proto_build_error(uint8_t *out, uint8_t code) {
    if (out == NULL) {
        return -1;
    }

    out[0] = code;
    return PROTO_ERROR_LEN;
}

const char *proto_type_name(uint8_t type) {
    switch (type) {
    case MSG_REGISTER:        return "REGISTER";
    case MSG_JOIN:            return "JOIN";
    case MSG_MOVE:            return "MOVE";
    case MSG_PING:            return "PING";
    case MSG_QUIT:            return "QUIT";
    case MSG_REGISTER_RESULT: return "REGISTER_RESULT";
    case MSG_WAITING:         return "WAITING";
    case MSG_MATCH_START:     return "MATCH_START";
    case MSG_STATE:           return "STATE";
    case MSG_GAME_OVER:       return "GAME_OVER";
    case MSG_ERROR:           return "ERROR";
    default:                  return "UNKNOWN";
    }
}

/* El mensaje más grande (REGISTER) debe caber en un payload. */
_Static_assert(1 + PLAYER_NICKNAME_MAX + 1 + PLAYER_EMAIL_MAX <= MAX_PAYLOAD,
               "REGISTER no cabe en MAX_PAYLOAD");
