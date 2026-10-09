#include <assert.h>
#include <stdio.h>
#include "../src/game.h"

int main(void) {
    game_t game = game_create(1, 10, 20);

    assert(game.paddle_left_y == 250.0f);
    assert(game.paddle_right_y == 250.0f);

    /* Moverse a la izquierda no debe implicar moverse a la derecha.. */
    assert(game_move_paddle(&game, 10, MOVE_UP, 0.1f) == 0);
    assert(game.paddle_left_y == 220.0f);
    assert(game.paddle_right_y == 250.0f);

    /* Las palas deben permanecer dentro de la pista. */
    assert(game_move_paddle(&game, 10, MOVE_UP, 1.0f) == 0);
    assert(game.paddle_left_y == 0.0f);

    assert(game_move_paddle(&game, 20, MOVE_DOWN, 1.0f) == 0);
    assert(game.paddle_right_y == 500.0f);

    /* Una entrada no válida no debe modificar la posición. */
    assert(game_move_paddle(&game, 99, MOVE_DOWN, 0.1f) == -1);
    assert(game_move_paddle(&game, 10, 2, 0.1f) == -1);
    assert(game_move_paddle(&game, 10, MOVE_DOWN, -0.1f) == -1);
    assert(game.paddle_left_y == 0.0f);

    game.status = GAME_FINISHED;
    assert(game_move_paddle(&game, 10, MOVE_DOWN, 0.1f) == -1);
    assert(game.paddle_left_y == 0.0f);

    /* Crear una partida independiente para probar la pelota. */
    game_t ball_game = game_create(2, 30, 40);

    /* Comprobar el movimiento hacia la derecha y abajo. */
    assert(game_update(&ball_game, 0.1f) == 0);
    assert(ball_game.ball_x == 430.0f);
    assert(ball_game.ball_y == 315.0f);

    /* Una velocidad negativa debe mover la pelota a la izquierda. */
    ball_game.ball_vx = -300.0f;

    assert(game_update(&ball_game, 0.1f) == 0);
    assert(ball_game.ball_x == 400.0f);
    assert(ball_game.ball_y == 330.0f);

    /* Un tiempo inválido no debe cambiar la posición. */
    assert(game_update(&ball_game, 0.0f) == -1);
    assert(ball_game.ball_x == 400.0f);
    assert(ball_game.ball_y == 330.0f);

    /* Una partida finalizada no debe seguir avanzando. */
    ball_game.status = GAME_FINISHED;

    assert(game_update(&ball_game, 0.1f) == -1);
    assert(ball_game.ball_x == 400.0f);
    assert(ball_game.ball_y == 330.0f);

    /* Comprobar el rebote contra el techo. */
    game_t bounce_game = game_create(3, 50, 60);

    bounce_game.ball_y = 15.0f;
    bounce_game.ball_vy = -150.0f;

    assert(game_update(&bounce_game, 0.1f) == 0);
    assert(bounce_game.ball_y == 20.0f);
    assert(bounce_game.ball_vy == 150.0f);
    assert(bounce_game.ball_vx == 300.0f);

    /* Comprobar el rebote contra el piso. */
    bounce_game.ball_y = 585.0f;
    bounce_game.ball_vy = 150.0f;

    assert(game_update(&bounce_game, 0.1f) == 0);
    assert(bounce_game.ball_y == 580.0f);
    assert(bounce_game.ball_vy == -150.0f);

    puts("All game tests passed.");
    return 0;
}