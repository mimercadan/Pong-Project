#include <assert.h>
#include <stdio.h>
#include "../src/lobby.h"

int main(void) {
    player_registry_t registry;
    lobby_t lobby;

    players_init(&registry);
    lobby_init(&lobby);

    /* Registrar cuatro jugadores. */
    for (int id = 1; id <= 4; id++) {
        assert(players_register(
            &registry, id, "Test", "test@example.com"
        ) == PLAYER_OK);
    }

    int match_id = 0;
    player_t player;

    /* El primero queda esperando. */
    assert(lobby_join(&lobby, &registry, 1, &match_id) ==
           LOBBY_WAITING);
    assert(match_id == 0);
    assert(lobby.waiting_count == 1);

    assert(players_get(&registry, 1, &player) == PLAYER_OK);
    assert(player.status == PLAYER_WAITING);

    /* No puede entrar dos veces a la cola. */
    assert(lobby_join(&lobby, &registry, 1, &match_id) ==
           LOBBY_ERROR_INVALID);
    assert(lobby.waiting_count == 1);

    /* El segundo forma una partida con el primero. */
    assert(lobby_join(&lobby, &registry, 2, &match_id) ==
           LOBBY_MATCH_STARTED);
    assert(match_id == 1);
    assert(lobby.waiting_count == 0);

    assert(lobby.matches[0].active == 1);
    assert(lobby.matches[0].game.player_left_id == 1);
    assert(lobby.matches[0].game.player_right_id == 2);
    assert(lobby.matches[0].game.status == GAME_PLAYING);

    assert(players_get(&registry, 1, &player) == PLAYER_OK);
    assert(player.status == PLAYER_PLAYING);
    assert(player.match_id == 1);

    assert(players_get(&registry, 2, &player) == PLAYER_OK);
    assert(player.status == PLAYER_PLAYING);
    assert(player.match_id == 1);

    /* Un jugador que ya juega no puede volver a entrar. */
    assert(lobby_join(&lobby, &registry, 2, &match_id) ==
           LOBBY_ERROR_INVALID);

    /* Los otros dos forman una partida independiente. */
    assert(lobby_join(&lobby, &registry, 3, &match_id) ==
           LOBBY_WAITING);
    assert(lobby_join(&lobby, &registry, 4, &match_id) ==
           LOBBY_MATCH_STARTED);

    assert(match_id == 2);
    assert(lobby.matches[1].game.player_left_id == 3);
    assert(lobby.matches[1].game.player_right_id == 4);
    assert(lobby.matches[0].game.id == 1);

    /* Un jugador no registrado no puede entrar al lobby. */
    assert(lobby_join(&lobby, &registry, 99, &match_id) ==
           LOBBY_ERROR_INVALID);


     /* Registrar dos jugadores adicionales. */
    assert(players_register(
        &registry, 5, "Five", "five@example.com"
    ) == PLAYER_OK);

    assert(players_register(
        &registry, 6, "Six", "six@example.com"
    ) == PLAYER_OK);

    /* El jugador 5 entra y luego cancela la espera. */
    assert(lobby_join(&lobby, &registry, 5, &match_id) ==
        LOBBY_WAITING);

    assert(lobby_cancel_waiting(&lobby, &registry, 5) == 0);
    assert(lobby.waiting_count == 0);

    assert(players_get(&registry, 5, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    /* No se puede cancelar dos veces ni cancelar a alguien que juega. */
    assert(lobby_cancel_waiting(&lobby, &registry, 5) == -1);
    assert(lobby_cancel_waiting(&lobby, &registry, 1) == -1);

    /* El jugador 6 debe esperar: el 5 ya no está en la cola. */
    assert(lobby_join(&lobby, &registry, 6, &match_id) ==
        LOBBY_WAITING);
    assert(lobby.waiting_count == 1);
    assert(lobby.waiting_ids[0] == 6);

    /* El jugador 5 puede regresar y formar pareja con el 6. */
    assert(lobby_join(&lobby, &registry, 5, &match_id) ==
        LOBBY_MATCH_STARTED);
    assert(match_id == 3);
    assert(lobby.waiting_count == 0);
    assert(lobby.matches[2].game.player_left_id == 6);
    assert(lobby.matches[2].game.player_right_id == 5);

    game_t result;

    /* No se puede liberar una partida que sigue jugando. */
    assert(lobby_finish_match(&lobby, &registry, 1, &result) == -1);
    assert(lobby.matches[0].active == 1);

    /* Terminar la primera partida por abandono del jugador 1. */
    assert(game_forfeit(&lobby.matches[0].game, 1) == 0);

    assert(lobby_finish_match(&lobby, &registry, 1, &result) == 0);

    /* La copia conserva el resultado aunque el espacio esté libre. */
    assert(result.id == 1);
    assert(result.status == GAME_FINISHED);
    assert(result.winner_id == 2);
    assert(result.player_left_id == 1);
    assert(result.player_right_id == 2);
    assert(lobby.matches[0].active == 0);

    /* Los dos jugadores quedan disponibles. */
    assert(players_get(&registry, 1, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    assert(players_get(&registry, 2, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    /* Las otras partidas no deben cambiar. */
    assert(lobby.matches[1].active == 1);
    assert(lobby.matches[1].game.id == 2);
    assert(lobby.matches[1].game.status == GAME_PLAYING);

    /* No se puede liberar dos veces la misma partida. */
    assert(lobby_finish_match(&lobby, &registry, 1, &result) == -1);

    /* Reutilizar el espacio con un nuevo identificador de partida. */
    assert(lobby_join(&lobby, &registry, 1, &match_id) ==
        LOBBY_WAITING);

    assert(lobby_join(&lobby, &registry, 2, &match_id) ==
        LOBBY_MATCH_STARTED);

    assert(match_id == 4);
    assert(lobby.matches[0].active == 1);
    assert(lobby.matches[0].game.id == 4);
    assert(lobby.matches[0].game.score_left == 0);
    assert(lobby.matches[0].game.score_right == 0);

    /* Desconectar al jugador 1 de la partida 4. */
    assert(lobby_disconnect(&lobby, &registry, 1, &result) == 1);
    assert(result.id == 4);
    assert(result.winner_id == 2);
    assert(lobby.matches[0].active == 0);

    /* El desconectado desaparece y su rival queda disponible. */
    assert(players_get(&registry, 1, &player) ==
        PLAYER_ERROR_NOT_FOUND);

    assert(players_get(&registry, 2, &player) == PLAYER_OK);
    assert(player.status == PLAYER_IDLE);
    assert(player.match_id == 0);

    /* Desconectar a un jugador que estaba esperando. */
    assert(lobby_join(&lobby, &registry, 2, &match_id) ==
        LOBBY_WAITING);

    assert(lobby_disconnect(&lobby, &registry, 2, &result) == 0);
    assert(lobby.waiting_count == 0);
    assert(result.id == 0);

    assert(players_get(&registry, 2, &player) ==
        PLAYER_ERROR_NOT_FOUND);

    /* Desconectar a un jugador disponible, sin partida. */
    assert(players_register(
        &registry, 7, "Seven", "seven@example.com"
    ) == PLAYER_OK);

    assert(lobby_disconnect(&lobby, &registry, 7, &result) == 0);
    assert(players_get(&registry, 7, &player) ==
        PLAYER_ERROR_NOT_FOUND);

    /* Repetir una desconexión no debe afectar otras partidas. */
    assert(lobby_disconnect(&lobby, &registry, 7, &result) == -1);
    assert(lobby.matches[1].active == 1);
    assert(lobby.matches[1].game.id == 2);

    puts("All lobby tests passed.");
    return 0;
}