#ifndef ENGINE_H
#define ENGINE_H

#include <pthread.h>
#include "lobby.h"

typedef struct {
    player_registry_t registry;
    lobby_t lobby;
    pthread_mutex_t lock;
} engine_t;

int engine_init(engine_t *engine);
int engine_destroy(engine_t *engine);

player_result_t engine_register(
    engine_t *engine,
    int connection_id,
    const char *nickname,
    const char *email
);

player_result_t engine_get_player(
    engine_t *engine,
    int player_id,
    player_t *out_player
);

lobby_result_t engine_join(
    engine_t *engine,
    int player_id,
    int *out_match_id
);

int engine_get_match(
    engine_t *engine,
    int match_id,
    game_t *out_game
);

int engine_disconnect(
    engine_t *engine,
    int player_id,
    game_t *out_result
);

int engine_set_input(
    engine_t *engine,
    int player_id,
    move_direction_t direction
);

int engine_step_match(
    engine_t *engine,
    int match_id,
    float dt,
    game_t *out_game
);

int engine_finish_match(
    engine_t *engine,
    int match_id,
    game_t *out_result
);

#endif