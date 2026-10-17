#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include "../src/engine.h"

#define TEST_PLAYERS 8

typedef struct {
    engine_t *engine;
    int player_id;
} test_args_t;

/* Cada hilo registra un jugador y lo lleva al lobby. */
static void *register_and_join(void *argument) {
    test_args_t *args = argument;

    assert(engine_register(
        args->engine,
        args->player_id,
        "Test",
        "test@example.com"
    ) == PLAYER_OK);

    int match_id = 0;

    lobby_result_t result = engine_join(
        args->engine, args->player_id, &match_id
    );

    assert(result == LOBBY_WAITING ||
           result == LOBBY_MATCH_STARTED);

    return NULL;
}

int main(void) {
    engine_t engine;
    assert(engine_init(&engine) == 0);

    pthread_t threads[TEST_PLAYERS];
    test_args_t args[TEST_PLAYERS];

    /* Crear ocho hilos con argumentos independientes. */
    for (int i = 0; i < TEST_PLAYERS; i++) {
        args[i].engine = &engine;
        args[i].player_id = i + 1;

        assert(pthread_create(
            &threads[i], NULL, register_and_join, &args[i]
        ) == 0);
    }

    /* Esperar a que todos terminen antes de revisar resultados. */
    for (int i = 0; i < TEST_PLAYERS; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    int match_counts[TEST_PLAYERS / 2 + 1] = {0};

    for (int id = 1; id <= TEST_PLAYERS; id++) {
        player_t player;

        assert(engine_get_player(&engine, id, &player) == PLAYER_OK);
        assert(player.status == PLAYER_PLAYING);
        assert(player.match_id >= 1);
        assert(player.match_id <= TEST_PLAYERS / 2);

        match_counts[player.match_id]++;
    }

    /* Deben formarse cuatro partidas, con dos jugadores cada una. */
    for (int id = 1; id <= TEST_PLAYERS / 2; id++) {
        assert(match_counts[id] == 2);
    }

    /* Consultar la partida del jugador 1 sin asumir quién es su rival. */
    player_t first_player;
    assert(engine_get_player(&engine, 1, &first_player) == PLAYER_OK);

    int first_match_id = first_player.match_id;
    game_t snapshot;

    assert(engine_get_match(
        &engine, first_match_id, &snapshot
    ) == 0);

    assert(snapshot.id == first_match_id);
    assert(snapshot.status == GAME_PLAYING);

    int opponent_id;

    if (snapshot.player_left_id == 1) {
        opponent_id = snapshot.player_right_id;
    } else {
        opponent_id = snapshot.player_left_id;
    }

    /* Cambiar la copia no debe modificar la partida original. */
    snapshot.score_left = 99;

    game_t original;
    assert(engine_get_match(
        &engine, first_match_id, &original
    ) == 0);
    assert(original.score_left == 0);

    /* Desconectar al jugador 1 y conservar el resultado. */
    game_t final_result;

    assert(engine_disconnect(&engine, 1, &final_result) == 1);
    assert(final_result.id == first_match_id);
    assert(final_result.winner_id == opponent_id);
    assert(final_result.status == GAME_FINISHED);

    /* La partida ya no existe, pero su resultado sigue en la copia. */
    assert(engine_get_match(
        &engine, first_match_id, &original
    ) == -1);

    assert(final_result.winner_id == opponent_id);

    /* El desconectado desapareció y el rival quedó disponible. */
    assert(engine_get_player(&engine, 1, &first_player) ==
        PLAYER_ERROR_NOT_FOUND);

    player_t opponent;
    assert(engine_get_player(
        &engine, opponent_id, &opponent
    ) == PLAYER_OK);

    assert(opponent.status == PLAYER_IDLE);
    assert(opponent.match_id == 0);

    /* Repetir la desconexión se rechaza. */
    assert(engine_disconnect(&engine, 1, &final_result) == -1);

    assert(engine_destroy(&engine) == 0);

    /* Crear un escenario independiente para probar las entradas. */
    engine_t input_engine;
    assert(engine_init(&input_engine) == 0);

    assert(engine_register(
        &input_engine, 101, "Left", "left@example.com"
    ) == PLAYER_OK);

    assert(engine_register(
        &input_engine, 102, "Right", "right@example.com"
    ) == PLAYER_OK);

    int input_match_id = 0;

    assert(engine_join(&input_engine, 101, &input_match_id) ==
        LOBBY_WAITING);
    assert(engine_join(&input_engine, 102, &input_match_id) ==
        LOBBY_MATCH_STARTED);

    game_t input_state;

    /* Recibir la misma dirección varias veces no debe mover la paleta. */
    for (int i = 0; i < 100; i++) {
        assert(engine_set_input(&input_engine, 101, MOVE_UP) == 0);
    }

    assert(engine_get_match(
        &input_engine, input_match_id, &input_state
    ) == 0);
    assert(input_state.paddle_left_y == 250.0f);

    /* El movimiento depende del tiempo de simulación. */
    assert(engine_step_match(
        &input_engine, input_match_id, 0.1f, &input_state
    ) == 0);

    assert(input_state.paddle_left_y == 220.0f);
    assert(input_state.paddle_right_y == 250.0f);

    /* Detener la paleta debe mantener su posición. */
    assert(engine_set_input(&input_engine, 101, MOVE_STOP) == 0);

    assert(engine_step_match(
        &input_engine, input_match_id, 0.1f, &input_state
    ) == 0);

    assert(input_state.paddle_left_y == 220.0f);

    /* Rechazar jugadores desconocidos y direcciones inválidas. */
    assert(engine_set_input(&input_engine, 999, MOVE_UP) == -1);
    assert(engine_set_input(&input_engine, 101, 2) == -1);

    /* Rechazar tiempos inválidos y partidas inexistentes. */
    assert(engine_step_match(
        &input_engine, input_match_id, -0.1f, &input_state
    ) == -1);

    assert(engine_step_match(
        &input_engine, 999, 0.1f, &input_state
    ) == -1);

    assert(engine_destroy(&input_engine) == 0);

    puts("All engine tests passed.");
    return 0;
}