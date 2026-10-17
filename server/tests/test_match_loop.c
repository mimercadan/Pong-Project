#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include "../src/match_loop.h"

int main(void) {
    engine_t engine;
    assert(engine_init(&engine) == 0);

    assert(engine_register(
        &engine, 1, "Left", "left@example.com"
    ) == PLAYER_OK);

    assert(engine_register(
        &engine, 2, "Right", "right@example.com"
    ) == PLAYER_OK);

    int match_id = 0;

    assert(engine_join(&engine, 1, &match_id) == LOBBY_WAITING);
    assert(engine_join(&engine, 2, &match_id) ==
           LOBBY_MATCH_STARTED);

    match_loop_t loop = {0};
    match_loop_t duplicate = {0};

    assert(match_loop_start(&loop, &engine, match_id) == 0);

    /* Rechazar un segundo hilo para la misma partida. */
    assert(match_loop_start(
        &duplicate, &engine, match_id
    ) == -1);

    /*
     * Esperar hasta observar movimiento.
     * Dar margen de tiempo si el computador está ocupado.
     */
    game_t state;
    int moved = 0;

    for (int attempt = 0; attempt < 100; attempt++) {
        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 20000000L
        };

        while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
        }

        assert(engine_get_match(&engine, match_id, &state) == 0);

        if (state.ball_x > 400.0f) {
            moved = 1;
            break;
        }
    }

    assert(moved);

    /* Esperar la terminación antes de destruir el motor. */
    assert(match_loop_stop(&loop) == 0);

    game_t stopped;
    assert(engine_get_match(&engine, match_id, &stopped) == 0);

    assert(engine_get_match(&engine, match_id, &state) == 0);
    assert(state.ball_x == stopped.ball_x);
    assert(state.ball_y == stopped.ball_y);

    assert(engine_destroy(&engine) == 0);

    puts("All match loop tests passed.");
    return 0;
}