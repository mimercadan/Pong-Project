#include "engine.h"

#include <stddef.h>

int engine_init(engine_t *engine) {
    if (engine == NULL) {
        return -1;
    }

    players_init(&engine->registry);
    lobby_init(&engine->lobby);

    if (pthread_mutex_init(&engine->lock, NULL) != 0) {
        return -1;
    }

    return 0;
}

/* Llamar únicamente cuando ningún hilo siga utilizando el motor. */
int engine_destroy(engine_t *engine) {
    if (engine == NULL) {
        return -1;
    }

    return pthread_mutex_destroy(&engine->lock) == 0 ? 0 : -1;
}

player_result_t engine_register(
    engine_t *engine,
    int connection_id,
    const char *nickname,
    const char *email
) {
    if (engine == NULL) {
        return PLAYER_ERROR_INVALID;
    }

    pthread_mutex_lock(&engine->lock);

    player_result_t result = players_register(
        &engine->registry, connection_id, nickname, email
    );

    pthread_mutex_unlock(&engine->lock);

    return result;
}

player_result_t engine_get_player(
    engine_t *engine,
    int player_id,
    player_t *out_player
) {
    if (engine == NULL) {
        return PLAYER_ERROR_INVALID;
    }

    pthread_mutex_lock(&engine->lock);

    player_result_t result = players_get(
        &engine->registry, player_id, out_player
    );

    pthread_mutex_unlock(&engine->lock);

    return result;
}

lobby_result_t engine_join(
    engine_t *engine,
    int player_id,
    int *out_match_id
) {
    if (engine == NULL) {
        return LOBBY_ERROR_INVALID;
    }

    pthread_mutex_lock(&engine->lock);

    lobby_result_t result = lobby_join(
        &engine->lobby, &engine->registry, player_id, out_match_id
    );

    pthread_mutex_unlock(&engine->lock);

    return result;
}

/* Obtener una copia coherente de una partida. */
int engine_get_match(
    engine_t *engine,
    int match_id,
    game_t *out_game
) {
    if (engine == NULL || out_game == NULL || match_id <= 0) {
        return -1;
    }

    int result = -1;

    pthread_mutex_lock(&engine->lock);

    for (int i = 0; i < MATCH_CAPACITY; i++) {
        match_slot_t *slot = &engine->lobby.matches[i];

        if (slot->active && slot->game.id == match_id) {
            *out_game = slot->game;
            result = 0;
            break;
        }
    }

    pthread_mutex_unlock(&engine->lock);

    return result;
}

/* Resolver la desconexión como una operación protegida completa. */
int engine_disconnect(
    engine_t *engine,
    int player_id,
    game_t *out_result
) {
    if (engine == NULL) {
        return -1;
    }

    pthread_mutex_lock(&engine->lock);

    int result = lobby_disconnect(
        &engine->lobby,
        &engine->registry,
        player_id,
        out_result
    );

    pthread_mutex_unlock(&engine->lock);

    return result;
}

/* Guardar la dirección sin mover inmediatamente la paleta. */
int engine_set_input(
    engine_t *engine,
    int player_id,
    move_direction_t direction
) {
    if (engine == NULL ||
        (direction != MOVE_UP &&
         direction != MOVE_STOP &&
         direction != MOVE_DOWN)) {
        return -1;
    }

    int result = -1;

    pthread_mutex_lock(&engine->lock);

    player_t player;

    if (players_get(
        &engine->registry, player_id, &player
    ) == PLAYER_OK && player.status == PLAYER_PLAYING) {

        for (int i = 0; i < MATCH_CAPACITY; i++) {
            match_slot_t *slot = &engine->lobby.matches[i];

            if (!slot->active ||
                slot->game.id != player.match_id ||
                slot->game.status != GAME_PLAYING) {
                continue;
            }

            if (slot->game.player_left_id == player_id) {
                slot->left_direction = direction;
                result = 0;
            } else if (slot->game.player_right_id == player_id) {
                slot->right_direction = direction;
                result = 0;
            }

            break;
        }
    }

    pthread_mutex_unlock(&engine->lock);

    return result;
}

/* Avanzar una partida y obtener una copia de su nuevo estado. */
int engine_step_match(
    engine_t *engine,
    int match_id,
    float dt,
    game_t *out_game
) {
    if (engine == NULL || out_game == NULL || match_id <= 0 ||
        !(dt > 0.0f && dt <= 1.0f)) {
        return -1;
    }

    int result = -1;

    pthread_mutex_lock(&engine->lock);

    for (int i = 0; i < MATCH_CAPACITY; i++) {
        match_slot_t *slot = &engine->lobby.matches[i];

        if (!slot->active ||
            slot->game.id != match_id ||
            slot->game.status != GAME_PLAYING) {
            continue;
        }

        game_move_paddle(
            &slot->game,
            slot->game.player_left_id,
            slot->left_direction,
            dt
        );

        game_move_paddle(
            &slot->game,
            slot->game.player_right_id,
            slot->right_direction,
            dt
        );

        result = game_update(&slot->game, dt);
        *out_game = slot->game;

        break;
    }

    pthread_mutex_unlock(&engine->lock);

    return result;
}

/* Liberar una partida finalizada cuando su ciclo ya no está activo. */
int engine_finish_match(
    engine_t *engine,
    int match_id,
    game_t *out_result
) {
    if (engine == NULL || out_result == NULL || match_id <= 0) {
        return -1;
    }

    int result = -1;

    pthread_mutex_lock(&engine->lock);

    for (int i = 0; i < MATCH_CAPACITY; i++) {
        match_slot_t *slot = &engine->lobby.matches[i];

        if (!slot->active || slot->game.id != match_id) {
            continue;
        }

        /* Esperar a que el ciclo deje de utilizar esta partida. */
        if (!slot->worker_running) {
            result = lobby_finish_match(
                &engine->lobby,
                &engine->registry,
                match_id,
                out_result
            );
        }

        break;
    }

    pthread_mutex_unlock(&engine->lock);

    return result;
}