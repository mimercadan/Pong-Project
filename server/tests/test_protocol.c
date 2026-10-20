#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/net.h"
#include "../src/protocol.h"

/* Los mismos bytes se prueban en client/test_protocol.py:
   así C y Python quedan obligados a hablar igual. */

int main(void) {
    uint8_t out[MAX_PAYLOAD];
    char nickname[PLAYER_NICKNAME_MAX + 1];
    char email[PLAYER_EMAIL_MAX + 1];

    /* REGISTER válido: "Ana" / "ana@x.co". */
    const uint8_t reg[] = {
        3, 'A', 'n', 'a',
        8, 'a', 'n', 'a', '@', 'x', '.', 'c', 'o'
    };
    assert(proto_parse_register(reg, sizeof reg, nickname, email) == 0);
    assert(strcmp(nickname, "Ana") == 0);
    assert(strcmp(email, "ana@x.co") == 0);

    /* REGISTER mal formado: largos que no cuadran, vacíos o '\0' en medio. */
    assert(proto_parse_register(reg, sizeof reg - 1, nickname, email) == -1);
    assert(proto_parse_register(reg, 0, nickname, email) == -1);
    const uint8_t reg_short[] = { 5, 'A', 'n' };
    assert(proto_parse_register(reg_short, sizeof reg_short,
                                nickname, email) == -1);
    const uint8_t reg_empty_nick[] = { 0, 1, 'a' };
    assert(proto_parse_register(reg_empty_nick, sizeof reg_empty_nick,
                                nickname, email) == -1);
    const uint8_t reg_nul[] = { 2, 'A', '\0', 1, 'a' };
    assert(proto_parse_register(reg_nul, sizeof reg_nul,
                                nickname, email) == -1);

    /* Nickname más largo que el límite de players.h. */
    uint8_t reg_long[1 + 33 + 1 + 1];
    reg_long[0] = 33;
    memset(reg_long + 1, 'a', 33);
    reg_long[34] = 1;
    reg_long[35] = 'a';
    assert(proto_parse_register(reg_long, sizeof reg_long,
                                nickname, email) == -1);

    /* MOVE: 0 quieto, 1 arriba, 2 abajo. */
    move_direction_t direction;
    const uint8_t up[] = { 1 }, down[] = { 2 }, stop[] = { 0 }, bad[] = { 3 };
    assert(proto_parse_move(up, 1, &direction) == 0 && direction == MOVE_UP);
    assert(proto_parse_move(down, 1, &direction) == 0 && direction == MOVE_DOWN);
    assert(proto_parse_move(stop, 1, &direction) == 0 && direction == MOVE_STOP);
    assert(proto_parse_move(bad, 1, &direction) == -1);
    assert(proto_parse_move(up, 0, &direction) == -1);

    /* REGISTER_RESULT: OK con id 7, y error con id en 0. */
    const uint8_t reg_ok[] = { 0, 0, 0, 0, 7 };
    assert(proto_build_register_result(out, PLAYER_OK, 7) == 5);
    assert(memcmp(out, reg_ok, 5) == 0);

    const uint8_t reg_taken[] = { 2, 0, 0, 0, 0 };
    assert(proto_build_register_result(
        out, PLAYER_ERROR_ALREADY_REGISTERED, 7) == 5);
    assert(memcmp(out, reg_taken, 5) == 0);

    assert(proto_build_register_result(out, PLAYER_ERROR_INVALID, 7) == 5);
    assert(out[0] == PROTO_REG_INVALID);
    assert(proto_build_register_result(out, PLAYER_ERROR_FULL, 7) == 5);
    assert(out[0] == PROTO_REG_FULL);

    /* MATCH_START: partida 3, lado derecho, rival "Bob". */
    const uint8_t start[] = { 0, 0, 0, 3, 1, 3, 'B', 'o', 'b' };
    assert(proto_build_match_start(out, 3, PROTO_SIDE_RIGHT, "Bob") == 9);
    assert(memcmp(out, start, 9) == 0);
    assert(proto_build_match_start(out, 3, 2, "Bob") == -1);
    assert(proto_build_match_start(out, 0, PROTO_SIDE_LEFT, "Bob") == -1);

    /* STATE: los floats se redondean a píxeles. */
    game_t game = game_create(3, 7, 9);
    game.ball_x = 400.4f;
    game.ball_y = 300.6f;
    game.paddle_left_y = 250.0f;
    game.paddle_right_y = -2.0f;      /* fuera de rango -> 0 */
    game.score_left = 2;
    game.score_right = 4;

    const uint8_t state[] = {
        0x01, 0x90,   /* 400 */
        0x01, 0x2D,   /* 301 */
        0x00, 0xFA,   /* 250 */
        0x00, 0x00,   /* 0 */
        2, 4
    };
    assert(proto_build_state(out, &game) == PROTO_STATE_LEN);
    assert(memcmp(out, state, sizeof state) == 0);

    /* GAME_OVER: ganó el 7, 5 a 2, por puntaje. */
    game.winner_id = 7;
    game.score_left = 5;
    game.score_right = 2;
    const uint8_t over[] = { 0, 0, 0, 7, 5, 2, 0 };
    assert(proto_build_game_over(out, &game, PROTO_END_SCORE) ==
           PROTO_GAME_OVER_LEN);
    assert(memcmp(out, over, sizeof over) == 0);
    assert(proto_build_game_over(out, &game, 9) == -1);

    /* ERROR. */
    assert(proto_build_error(out, PROTO_ERR_INVALID_STATE) == 1);
    assert(out[0] == 3);

    assert(strcmp(proto_type_name(MSG_STATE), "STATE") == 0);
    assert(strcmp(proto_type_name(0x7F), "UNKNOWN") == 0);

    printf("test_protocol: OK\n");
    return 0;
}
