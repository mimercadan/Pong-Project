#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include "../src/match_loop.h"

/* Esperar brevemente, reintentando si llega una señal. */
static void pause_briefly(void) {
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 20000000L
    };

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
    }
}

int main(void) {
    engine_t engine;
    assert(engine_init(&engine) == 0);

    game_t state;
    game_t result;
    player_t player;
    int match_id = 0;

    /* Registrar y emparejar a los primeros dos jugadores. */
    assert(engine_register(
        &engine, 1, "Left", "left@example.com"
    ) == PLAYER_OK);

    assert(engine_register(
        &engine, 2, "Right", "right@example.com"
    ) == PLAYER_OK);

    assert(engine_join(&engine, 1, &match_id) == LOBBY_WAITING);
    assert(engine_join(&engine, 2, &match_id) ==
           LOBBY_MATCH_STARTED);

    match_loop_t loop = {0};
    match_loop_t duplicate = {0};

    assert(match_loop_start(&loop, &engine, match_id) == 0);

    /* Rechazar un segundo ciclo para la misma partida. */
    assert(match_loop_start(
        &duplicate, &engine, match_id
    ) == -1);

    /* Comprobar que la pelota avanza automáticamente. */
    int moved = 0;

    for (int attempt = 0; attempt < 100; attempt++) {
        pause_briefly();

        assert(engine_get_match(&engine, match_id, &state) == 0);

        if (state.ball_x > 400.0f) {
            moved = 1;
            break;
        }
    }

    assert(moved);
    assert(match_loop_stop(&loop) == 0);

    /* Tras recoger el hilo, la pelota debe permanecer quieta. */
    game_t stopped;
    assert(engine_get_match(&engine, match_id, &stopped) == 0);

    pause_briefly();

    assert(engine_get_match(&engine, match_id, &state) == 0);
    assert(state.ball_x == stopped.ball_x);
    assert(state.ball_y == stopped.ball_y);

    /* Detener el ciclo no significa finalizar la partida. */
    assert(engine_finish_match(
        &engine, match_id, &result
    ) == -1);

    /* Desconectar a un jugador con el ciclo iniciado. */
    match_loop_t disconnect_loop = {0};

    assert(match_loop_start(
        &disconnect_loop, &engine, match_id
    ) == 0);

    assert(engine_disconnect(&engine, 1, &result) == 1);
    assert(result.id == match_id);
    assert(result.winner_id == 2);

    assert(match_loop_stop(&disconnect_loop) == 0);

    assert(engine_get_match(&engine, match_id, &state) == -1);
    assert(engine_get_player(&engine, 1, &player) ==
           PLAYER_ERROR_NOT_FOUND);

    assert(engine_get_player(&engine, 2, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    /* Crear una segunda partida con el jugador que permaneció. */
    assert(engine_register(
        &engine, 3, "Third", "third@example.com"
    ) == PLAYER_OK);

    int second_match_id = 0;

    assert(engine_join(&engine, 2, &second_match_id) ==
           LOBBY_WAITING);
    assert(engine_join(&engine, 3, &second_match_id) ==
           LOBBY_MATCH_STARTED);

    match_loop_t old_loop = {0};

    assert(match_loop_start(
        &old_loop, &engine, second_match_id
    ) == 0);

    /* Liberar su partida antes de recoger el hilo antiguo. */
    assert(engine_disconnect(&engine, 2, &result) == 1);
    assert(result.winner_id == 3);

    assert(engine_register(
        &engine, 4, "Fourth", "fourth@example.com"
    ) == PLAYER_OK);

    int new_match_id = 0;

    assert(engine_join(&engine, 3, &new_match_id) ==
           LOBBY_WAITING);
    assert(engine_join(&engine, 4, &new_match_id) ==
           LOBBY_MATCH_STARTED);

    assert(new_match_id != second_match_id);
    assert(match_loop_stop(&old_loop) == 0);

    /* El hilo antiguo no debe modificar la partida nueva. */
    assert(engine_get_match(
        &engine, new_match_id, &state
    ) == 0);

    assert(state.ball_x == 400.0f);
    assert(state.ball_y == 300.0f);
    assert(state.score_left == 0);
    assert(state.score_right == 0);

    assert(engine_step_match(
        &engine, second_match_id, 0.1f, &state
    ) == -1);

    /*
     * Preparar el quinto punto solo para esta prueba.
     * La nueva partida todavía no tiene un ciclo iniciado.
     */
    int prepared = 0;

    pthread_mutex_lock(&engine.lock);

    for (int i = 0; i < MATCH_CAPACITY; i++) {
        match_slot_t *slot = &engine.lobby.matches[i];

        if (slot->active && slot->game.id == new_match_id) {
            slot->game.score_left = WINNING_SCORE - 1;
            slot->game.ball_x = 805.0f;
            slot->game.ball_y = 100.0f;
            slot->game.ball_vx = 300.0f;
            slot->game.ball_vy = 0.0f;

            prepared = 1;
            break;
        }
    }

    pthread_mutex_unlock(&engine.lock);
    assert(prepared);

    /* El ciclo debe calcular el punto ganador. */
    match_loop_t scoring_loop = {0};

    assert(match_loop_start(
        &scoring_loop, &engine, new_match_id
    ) == 0);

    int finished = 0;

    for (int attempt = 0; attempt < 100; attempt++) {
        pause_briefly();

        assert(engine_get_match(
            &engine, new_match_id, &state
        ) == 0);

        if (state.status == GAME_FINISHED) {
            finished = 1;
            break;
        }
    }

    assert(finished);
    assert(state.score_left == WINNING_SCORE);
    assert(state.winner_id == 3);

    /* Recoger el hilo antes de liberar la partida finalizada. */
    assert(match_loop_stop(&scoring_loop) == 0);

    assert(engine_finish_match(
        &engine, new_match_id, &result
    ) == 0);

    assert(result.id == new_match_id);
    assert(result.score_left == WINNING_SCORE);
    assert(result.winner_id == 3);

    assert(engine_get_match(
        &engine, new_match_id, &state
    ) == -1);

    /* Ambos jugadores quedan disponibles. */
    assert(engine_get_player(&engine, 3, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    assert(engine_get_player(&engine, 4, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    assert(engine_finish_match(
        &engine, new_match_id, &result
    ) == -1);

    assert(engine_destroy(&engine) == 0);

    puts("All match loop tests passed.");
    return 0;
}