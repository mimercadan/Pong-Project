#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/game.h"

int main(void) {
    /* Crear una partida y comprobar las posiciones iniciales. */
    game_t game = game_create(1, 10, 20);

    assert(game.paddle_left_y == 250.0f);
    assert(game.paddle_right_y == 250.0f);

    /* Mover la paleta izquierda no debe mover la derecha. */
    assert(game_move_paddle(&game, 10, MOVE_UP, 0.1f) == 0);
    assert(game.paddle_left_y == 220.0f);
    assert(game.paddle_right_y == 250.0f);

    /* Las paletas deben permanecer dentro de la cancha. */
    assert(game_move_paddle(&game, 10, MOVE_UP, 1.0f) == 0);
    assert(game.paddle_left_y == 0.0f);

    assert(game_move_paddle(&game, 20, MOVE_DOWN, 1.0f) == 0);
    assert(game.paddle_right_y == 500.0f);

    /* Una entrada inválida no debe cambiar la posición. */
    assert(game_move_paddle(&game, 99, MOVE_DOWN, 0.1f) == -1);
    assert(game_move_paddle(&game, 10, 2, 0.1f) == -1);
    assert(game_move_paddle(&game, 10, MOVE_DOWN, -0.1f) == -1);
    assert(game.paddle_left_y == 0.0f);

    /* Una partida finalizada no permite mover las paletas. */
    game.status = GAME_FINISHED;

    assert(game_move_paddle(&game, 10, MOVE_DOWN, 0.1f) == -1);
    assert(game.paddle_left_y == 0.0f);

    /* Crear otra partida para comprobar el movimiento de la pelota. */
    game_t ball_game = game_create(2, 30, 40);

    assert(game_update(&ball_game, 0.1f) == 0);
    assert(fabsf(ball_game.ball_x - 430.0f) < 0.01f);
    assert(fabsf(ball_game.ball_y - 315.0f) < 0.01f);

    /* Una velocidad negativa mueve la pelota a la izquierda. */
    ball_game.ball_vx = -300.0f;

    assert(game_update(&ball_game, 0.1f) == 0);
    assert(fabsf(ball_game.ball_x - 400.0f) < 0.01f);
    assert(fabsf(ball_game.ball_y - 330.0f) < 0.01f);

    /* Un tiempo inválido no debe modificar la pelota. */
    float previous_x = ball_game.ball_x;
    float previous_y = ball_game.ball_y;

    assert(game_update(&ball_game, 0.0f) == -1);
    assert(ball_game.ball_x == previous_x);
    assert(ball_game.ball_y == previous_y);

    /* Una partida finalizada no debe seguir avanzando. */
    ball_game.status = GAME_FINISHED;

    assert(game_update(&ball_game, 0.1f) == -1);
    assert(ball_game.ball_x == previous_x);
    assert(ball_game.ball_y == previous_y);

    /* Comprobar el rebote contra el techo. */
    game_t bounce_game = game_create(3, 50, 60);

    bounce_game.ball_y = 15.0f;
    bounce_game.ball_vy = -150.0f;

    assert(game_update(&bounce_game, 0.1f) == 0);
    assert(fabsf(bounce_game.ball_y - 20.0f) < 0.01f);
    assert(bounce_game.ball_vy == 150.0f);
    assert(bounce_game.ball_vx == 300.0f);

    /* Comprobar el rebote contra el piso. */
    bounce_game.ball_y = 585.0f;
    bounce_game.ball_vy = 150.0f;

    assert(game_update(&bounce_game, 0.1f) == 0);
    assert(fabsf(bounce_game.ball_y - 580.0f) < 0.01f);
    assert(bounce_game.ball_vy == -150.0f);

    /* Comprobar el rebote contra la paleta izquierda. */
    game_t collision_game = game_create(4, 70, 80);

    collision_game.ball_x = 65.0f;
    collision_game.ball_y = 300.0f;
    collision_game.ball_vx = -300.0f;
    collision_game.ball_vy = 0.0f;

    assert(game_update(&collision_game, 0.1f) == 0);
    assert(collision_game.ball_vx == 300.0f);
    assert(fabsf(collision_game.ball_x - 85.0f) < 0.01f);

    /* Comprobar el rebote contra la paleta derecha. */
    collision_game.ball_x = 735.0f;
    collision_game.ball_vx = 300.0f;

    assert(game_update(&collision_game, 0.1f) == 0);
    assert(collision_game.ball_vx == -300.0f);
    assert(fabsf(collision_game.ball_x - 715.0f) < 0.01f);

    /* Si pasa por encima de la paleta, no debe rebotar. */
    collision_game.ball_x = 65.0f;
    collision_game.ball_y = 100.0f;
    collision_game.ball_vx = -300.0f;

    assert(game_update(&collision_game, 0.1f) == 0);
    assert(collision_game.ball_vx == -300.0f);
    assert(fabsf(collision_game.ball_x - 35.0f) < 0.01f);

    /* Salir por la izquierda debe sumar un punto al jugador derecho. */
    game_t score_game = game_create(5, 90, 100);

    score_game.ball_x = -5.0f;
    score_game.ball_y = 100.0f;
    score_game.ball_vx = -300.0f;
    score_game.ball_vy = 0.0f;

    assert(game_update(&score_game, 0.1f) == 0);
    assert(score_game.score_right == 1);
    assert(score_game.score_left == 0);
    assert(score_game.ball_x == 400.0f);
    assert(score_game.ball_y == 300.0f);
    assert(score_game.ball_vx == -300.0f);

    /* Salir por la derecha debe sumar al izquierdo y conservar el otro punto. */
    score_game.ball_x = 805.0f;
    score_game.ball_y = 100.0f;
    score_game.ball_vx = 300.0f;
    score_game.ball_vy = 0.0f;

    assert(game_update(&score_game, 0.1f) == 0);
    assert(score_game.score_left == 1);
    assert(score_game.score_right == 1);
    assert(score_game.ball_x == 400.0f);
    assert(score_game.ball_y == 300.0f);
    assert(score_game.ball_vx == 300.0f);

   /* El cuarto punto todavía no debe terminar la partida. */
    game_t winner_game = game_create(6, 110, 120);

    assert(winner_game.winner_id == 0);

    winner_game.score_right = WINNING_SCORE - 2;
    winner_game.ball_x = -5.0f;
    winner_game.ball_y = 100.0f;
    winner_game.ball_vx = -300.0f;
    winner_game.ball_vy = 0.0f;

    assert(game_update(&winner_game, 0.1f) == 0);
    assert(winner_game.score_right == WINNING_SCORE - 1);
    assert(winner_game.status == GAME_PLAYING);
    assert(winner_game.winner_id == 0);

    /* El quinto punto debe dar la victoria al jugador derecho. */
    winner_game.ball_x = -5.0f;
    winner_game.ball_y = 100.0f;
    winner_game.ball_vx = -300.0f;
    winner_game.ball_vy = 0.0f;

    assert(game_update(&winner_game, 0.1f) == 0);
    assert(winner_game.score_right == WINNING_SCORE);
    assert(winner_game.status == GAME_FINISHED);
    assert(winner_game.winner_id == 120);
    assert(winner_game.ball_vx == 0.0f);
    assert(winner_game.ball_vy == 0.0f);

    /* La partida finalizada no debe moverse ni sumar más puntos. */
    float finished_x = winner_game.ball_x;
    float finished_y = winner_game.ball_y;

    assert(game_update(&winner_game, 0.1f) == -1);
    assert(winner_game.ball_x == finished_x);
    assert(winner_game.ball_y == finished_y);
    assert(winner_game.score_right == WINNING_SCORE);

    assert(game_move_paddle(&winner_game, 110, MOVE_UP, 0.1f) == -1);

    /* Comprobar también la victoria del jugador izquierdo. */
    game_t left_winner_game = game_create(7, 130, 140);

    left_winner_game.score_left = WINNING_SCORE - 1;
    left_winner_game.ball_x = 805.0f;
    left_winner_game.ball_y = 100.0f;
    left_winner_game.ball_vx = 300.0f;
    left_winner_game.ball_vy = 0.0f;

    assert(game_update(&left_winner_game, 0.1f) == 0);
    assert(left_winner_game.score_left == WINNING_SCORE);
    assert(left_winner_game.status == GAME_FINISHED);
    assert(left_winner_game.winner_id == 130);
    assert(left_winner_game.ball_vx == 0.0f);
    assert(left_winner_game.ball_vy == 0.0f);

    /* Un jugador ajeno no puede terminar la partida. */
    game_t forfeit_game = game_create(8, 150, 160);

    forfeit_game.score_left = 2;
    forfeit_game.score_right = 3;

    assert(game_forfeit(&forfeit_game, 999) == -1);
    assert(forfeit_game.status == GAME_PLAYING);
    assert(forfeit_game.winner_id == 0);

    /* Si abandona el izquierdo, gana el derecho. */
    assert(game_forfeit(&forfeit_game, 150) == 0);
    assert(forfeit_game.status == GAME_FINISHED);
    assert(forfeit_game.winner_id == 160);
    assert(forfeit_game.ball_vx == 0.0f);
    assert(forfeit_game.ball_vy == 0.0f);

    /* El abandono no debe alterar los puntos obtenidos. */
    assert(forfeit_game.score_left == 2);
    assert(forfeit_game.score_right == 3);

    /* Una segunda llamada no debe cambiar al ganador. */
    assert(game_forfeit(&forfeit_game, 160) == -1);
    assert(forfeit_game.winner_id == 160);

    /* Si abandona el derecho, gana el izquierdo. */
    game_t right_forfeit_game = game_create(9, 170, 180);

    assert(game_forfeit(&right_forfeit_game, 180) == 0);
    assert(right_forfeit_game.status == GAME_FINISHED);
    assert(right_forfeit_game.winner_id == 170);

    /* Rechazar un puntero que no apunta a una partida. */
    assert(game_forfeit(NULL, 170) == -1);

    puts("All game tests passed.");
    return 0;
}