#include "players.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

/* Buscar la posición de un jugador registrado. */
static int find_player_index(
    const player_registry_t *registry,
    int player_id
) {
    for (int i = 0; i < PLAYER_CAPACITY; i++) {
        if (registry->players[i].active &&
            registry->players[i].id == player_id) {
            return i;
        }
    }

    return -1;
}

/* Validar un texto y obtener su longitud sin superar el límite. */
static int valid_text(const char *text, size_t max_length) {
    if (text == NULL) {
        return 0;
    }

    size_t length = 0;

    while (length <= max_length && text[length] != '\0') {
        unsigned char character = (unsigned char)text[length];

        /* Rechazar espacios y caracteres de control. */
        if (isspace(character) || iscntrl(character)) {
            return 0;
        }

        length++;
    }

    return length > 0 && length <= max_length;
}

/* Validación básica: una sola arroba, con texto a ambos lados. */
static int valid_email(const char *email) {
    if (!valid_text(email, PLAYER_EMAIL_MAX)) {
        return 0;
    }

    const char *at = strchr(email, '@');

    if (at == NULL || at == email || at[1] == '\0') {
        return 0;
    }

    return strchr(at + 1, '@') == NULL;
}

void players_init(player_registry_t *registry) {
    if (registry != NULL) {
        memset(registry, 0, sizeof *registry);
    }
}

player_result_t players_register(
    player_registry_t *registry,
    int connection_id,
    const char *nickname,
    const char *email
) {
    if (registry == NULL ||
        connection_id <= 0 ||
        !valid_text(nickname, PLAYER_NICKNAME_MAX) ||
        !valid_email(email)) {
        return PLAYER_ERROR_INVALID;
    }

    if (find_player_index(registry, connection_id) != -1) {
        return PLAYER_ERROR_ALREADY_REGISTERED;
    }

    for (int i = 0; i < PLAYER_CAPACITY; i++) {
        if (!registry->players[i].active) {
            player_t *player = &registry->players[i];

            /* Limpiar el espacio antes de guardar el perfil. */
            memset(player, 0, sizeof *player);

            player->id = connection_id;
            strcpy(player->nickname, nickname);
            strcpy(player->email, email);
            player->status = PLAYER_IDLE;
            player->match_id = 0;

            /* Marcar el espacio como ocupado al terminar. */
            player->active = 1;

            return PLAYER_OK;
        }
    }

    return PLAYER_ERROR_FULL;
}

player_result_t players_get(
    const player_registry_t *registry,
    int player_id,
    player_t *out_player
) {
    if (registry == NULL || out_player == NULL || player_id <= 0) {
        return PLAYER_ERROR_INVALID;
    }

    int index = find_player_index(registry, player_id);

    if (index == -1) {
        return PLAYER_ERROR_NOT_FOUND;
    }

    /* Copiar el perfil completo a la variable de quien consulta. */
    *out_player = registry->players[index];

    return PLAYER_OK;
}

/* Borrar un perfil solo cuando no está esperando ni jugando. */
player_result_t players_remove(
    player_registry_t *registry,
    int player_id
) {
    if (registry == NULL || player_id <= 0) {
        return PLAYER_ERROR_INVALID;
    }

    int index = find_player_index(registry, player_id);

    if (index == -1) {
        return PLAYER_ERROR_NOT_FOUND;
    }

    if (registry->players[index].status != PLAYER_IDLE) {
        return PLAYER_ERROR_INVALID;
    }

    memset(&registry->players[index], 0,
           sizeof registry->players[index]);

    return PLAYER_OK;
}