#ifndef LOBBY_H
#define LOBBY_H

#include "game.h"
#include "players.h"

#define MATCH_CAPACITY (PLAYER_CAPACITY / 2)

typedef struct {
    int active;
    game_t game;
} match_slot_t;

typedef struct {
    int waiting_ids[PLAYER_CAPACITY];
    int waiting_count;

    match_slot_t matches[MATCH_CAPACITY];
    int next_match_id;
} lobby_t;

typedef enum {
    LOBBY_WAITING = 0,
    LOBBY_MATCH_STARTED,
    LOBBY_ERROR_INVALID,
    LOBBY_ERROR_FULL
} lobby_result_t;

void lobby_init(lobby_t *lobby);

lobby_result_t lobby_join(
    lobby_t *lobby,
    player_registry_t *registry,
    int player_id,
    int *out_match_id
);

int lobby_cancel_waiting(
    lobby_t *lobby,
    player_registry_t *registry,
    int player_id
);

int lobby_finish_match(
    lobby_t *lobby,
    player_registry_t *registry,
    int match_id,
    game_t *out_result
);

int lobby_disconnect(
    lobby_t *lobby,
    player_registry_t *registry,
    int player_id,
    game_t *out_result
);

#endif