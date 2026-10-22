#ifndef PLAYERS_H
#define PLAYERS_H

/* Límites provisionales, medidos en bytes. */
#define PLAYER_CAPACITY 64
#define PLAYER_NICKNAME_MAX 32
#define PLAYER_EMAIL_MAX 128

typedef enum {
    PLAYER_IDLE,
    PLAYER_WAITING,
    PLAYER_PLAYING
} player_status_t;

typedef struct {
    int active;
    int id;

    char nickname[PLAYER_NICKNAME_MAX + 1];
    char email[PLAYER_EMAIL_MAX + 1];

    player_status_t status;
    int match_id;
} player_t;

typedef struct {
    player_t players[PLAYER_CAPACITY];
} player_registry_t;

typedef enum {
    PLAYER_OK = 0,
    PLAYER_ERROR_INVALID,
    PLAYER_ERROR_ALREADY_REGISTERED,
    PLAYER_ERROR_FULL,
    PLAYER_ERROR_NOT_FOUND
} player_result_t;

void players_init(player_registry_t *registry);

player_result_t players_register(
    player_registry_t *registry,
    int connection_id,
    const char *nickname,
    const char *email
);

player_result_t players_get(
    const player_registry_t *registry,
    int player_id,
    player_t *out_player
);

player_result_t players_remove(
    player_registry_t *registry,
    int player_id
);

#endif