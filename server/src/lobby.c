#include "lobby.h"

#include <stddef.h>
#include <string.h>

/* Buscar un jugador activo dentro del registro. */
static player_t *find_player(
    player_registry_t *registry,
    int player_id
) {
    for (int i = 0; i < PLAYER_CAPACITY; i++) {
        if (registry->players[i].active &&
            registry->players[i].id == player_id) {
            return &registry->players[i];
        }
    }

    return NULL;
}

void lobby_init(lobby_t *lobby) {
    if (lobby != NULL) {
        memset(lobby, 0, sizeof *lobby);
        lobby->next_match_id = 1;
    }
}

lobby_result_t lobby_join(
    lobby_t *lobby,
    player_registry_t *registry,
    int player_id,
    int *out_match_id
) {
    if (lobby == NULL || registry == NULL || out_match_id == NULL) {
        return LOBBY_ERROR_INVALID;
    }

    *out_match_id = 0;

    player_t *player = find_player(registry, player_id);

    /* Solo puede entrar un jugador registrado y disponible. */
    if (player == NULL || player->status != PLAYER_IDLE) {
        return LOBBY_ERROR_INVALID;
    }

    /* Si no hay rival, dejar al jugador esperando. */
    if (lobby->waiting_count == 0) {
        lobby->waiting_ids[lobby->waiting_count] = player_id;
        lobby->waiting_count++;
        player->status = PLAYER_WAITING;

        return LOBBY_WAITING;
    }

    /* Buscar espacio para una nueva partida. */
    int slot = -1;

    for (int i = 0; i < MATCH_CAPACITY; i++) {
        if (!lobby->matches[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return LOBBY_ERROR_FULL;
    }

    int opponent_id = lobby->waiting_ids[0];
    player_t *opponent = find_player(registry, opponent_id);

    if (opponent == NULL || opponent->status != PLAYER_WAITING) {
        return LOBBY_ERROR_INVALID;
    }

    /* El primero que llegó controla la paleta izquierda. */
    int match_id = lobby->next_match_id;

    lobby->matches[slot].game =
        game_create(match_id, opponent_id, player_id);

    lobby->matches[slot].active = 1;
    lobby->next_match_id++;

    /* Retirar de la cola al jugador emparejado. */
    for (int i = 1; i < lobby->waiting_count; i++) {
        lobby->waiting_ids[i - 1] = lobby->waiting_ids[i];
    }

    lobby->waiting_count--;
    lobby->waiting_ids[lobby->waiting_count] = 0;

    /* Ambos jugadores quedan asociados a la misma partida. */
    opponent->status = PLAYER_PLAYING;
    opponent->match_id = match_id;

    player->status = PLAYER_PLAYING;
    player->match_id = match_id;

    *out_match_id = match_id;

    return LOBBY_MATCH_STARTED;
}

/* Retirar de la cola a un jugador que está esperando. */
int lobby_cancel_waiting(
    lobby_t *lobby,
    player_registry_t *registry,
    int player_id
) {
    if (lobby == NULL || registry == NULL) {
        return -1;
    }

    player_t *player = find_player(registry, player_id);

    if (player == NULL || player->status != PLAYER_WAITING) {
        return -1;
    }

    int index = -1;

    /* Buscar al jugador dentro de la cola. */
    for (int i = 0; i < lobby->waiting_count; i++) {
        if (lobby->waiting_ids[i] == player_id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return -1;
    }

    /* Desplazar los elementos posteriores para cerrar el espacio. */
    for (int i = index + 1; i < lobby->waiting_count; i++) {
        lobby->waiting_ids[i - 1] = lobby->waiting_ids[i];
    }

    lobby->waiting_count--;
    lobby->waiting_ids[lobby->waiting_count] = 0;

    player->status = PLAYER_IDLE;
    player->match_id = 0;

    return 0;
}

/* Conservar el resultado y liberar una partida finalizada. */
int lobby_finish_match(
    lobby_t *lobby,
    player_registry_t *registry,
    int match_id,
    game_t *out_result
) {
    if (lobby == NULL || registry == NULL ||
        out_result == NULL || match_id <= 0) {
        return -1;
    }

    for (int i = 0; i < MATCH_CAPACITY; i++) {
        match_slot_t *slot = &lobby->matches[i];

        if (!slot->active || slot->game.id != match_id) {
            continue;
        }

        /* No liberar una partida que todavía está jugando. */
        if (slot->game.status != GAME_FINISHED) {
            return -1;
        }

        player_t *left = find_player(
            registry, slot->game.player_left_id
        );

        player_t *right = find_player(
            registry, slot->game.player_right_id
        );

        /* Copiar el resultado antes de borrar la partida. */
        *out_result = slot->game;

        /* Dejar disponibles a los jugadores de esta partida. */
        if (left != NULL && left->match_id == match_id) {
            left->status = PLAYER_IDLE;
            left->match_id = 0;
        }

        if (right != NULL && right->match_id == match_id) {
            right->status = PLAYER_IDLE;
            right->match_id = 0;
        }

        /* El espacio podrá utilizarse para otra partida. */
        memset(slot, 0, sizeof *slot);

        return 0;
    }

    return -1;
}

/*
 * Limpiar los datos de un jugador desconectado.
 * Devuelve 1 si terminó una partida, 0 si no había partida,
 * y -1 si ocurrió un error.
 */
int lobby_disconnect(
    lobby_t *lobby,
    player_registry_t *registry,
    int player_id,
    game_t *out_result
) {
    if (lobby == NULL || registry == NULL || out_result == NULL) {
        return -1;
    }

    memset(out_result, 0, sizeof *out_result);

    player_t *player = find_player(registry, player_id);

    if (player == NULL) {
        return -1;
    }

    int match_finished = 0;

    if (player->status == PLAYER_WAITING) {
        if (lobby_cancel_waiting(lobby, registry, player_id) != 0) {
            return -1;
        }
    } else if (player->status == PLAYER_PLAYING) {
        int match_id = player->match_id;
        match_slot_t *match = NULL;

        for (int i = 0; i < MATCH_CAPACITY; i++) {
            if (lobby->matches[i].active &&
                lobby->matches[i].game.id == match_id) {
                match = &lobby->matches[i];
                break;
            }
        }

        if (match == NULL) {
            return -1;
        }

        /* Si ya terminó por puntos, conservar ese ganador. */
        if (match->game.status == GAME_PLAYING) {
            if (game_forfeit(&match->game, player_id) != 0) {
                return -1;
            }
        }

        if (lobby_finish_match(
            lobby, registry, match_id, out_result
        ) != 0) {
            return -1;
        }

        match_finished = 1;
    }

    if (players_remove(registry, player_id) != PLAYER_OK) {
        return -1;
    }

    return match_finished;
}