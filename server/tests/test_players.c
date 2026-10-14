#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/players.h"

int main(void) {
    player_registry_t registry;
    players_init(&registry);

    player_t player;

    /* Antes del registro, el jugador no existe. */
    assert(players_get(&registry, 1, &player) ==
           PLAYER_ERROR_NOT_FOUND);

    /* Registrar y consultar un perfil válido. */
    assert(players_register(
        &registry, 1, "Daya", "daya@example.com"
    ) == PLAYER_OK);

    assert(players_get(&registry, 1, &player) == PLAYER_OK);
    assert(player.id == 1);
    assert(player.active == 1);
    assert(strcmp(player.nickname, "Daya") == 0);
    assert(strcmp(player.email, "daya@example.com") == 0);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    /* Una misma conexión no puede registrarse dos veces. */
    assert(players_register(
        &registry, 1, "Otro", "otro@example.com"
    ) == PLAYER_ERROR_ALREADY_REGISTERED);

    /* Rechazar datos inválidos. */
    assert(players_register(
        &registry, 2, "", "test@example.com"
    ) == PLAYER_ERROR_INVALID);

    assert(players_register(
        &registry, 2, "Test", "sin-arroba"
    ) == PLAYER_ERROR_INVALID);

    assert(players_register(
        &registry, 2, "Test", "a@@example.com"
    ) == PLAYER_ERROR_INVALID);

    assert(players_register(
        &registry, 2, "Con espacio", "test@example.com"
    ) == PLAYER_ERROR_INVALID);

    assert(players_register(
        &registry, 0, "Test", "test@example.com"
    ) == PLAYER_ERROR_INVALID);

    /* Rechazar un nickname que supera el límite. */
    char long_nickname[PLAYER_NICKNAME_MAX + 2];
    memset(long_nickname, 'a', sizeof long_nickname);
    long_nickname[sizeof long_nickname - 1] = '\0';

    assert(players_register(
        &registry, 2, long_nickname, "test@example.com"
    ) == PLAYER_ERROR_INVALID);

    /* Los intentos inválidos no deben crear un perfil. */
    assert(players_get(&registry, 2, &player) ==
           PLAYER_ERROR_NOT_FOUND);

    /* Completar la capacidad disponible. */
    for (int id = 2; id <= PLAYER_CAPACITY; id++) {
        assert(players_register(
            &registry, id, "Test", "test@example.com"
        ) == PLAYER_OK);
    }

    /* Rechazar un registro cuando la lista está llena. */
    assert(players_register(
        &registry, PLAYER_CAPACITY + 1, "Extra", "extra@example.com"
    ) == PLAYER_ERROR_FULL);

    puts("All player tests passed.");
    return 0;
}