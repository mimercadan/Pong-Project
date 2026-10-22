#include "game.h"
#include <stddef.h>

game_t game_create(int id, int player_left_id, int player_right_id) {
    game_t game = {0};

    game.id = id;
    game.player_left_id = player_left_id;
    game.player_right_id = player_right_id;

    game.paddle_left_y = (GAME_HEIGHT - PADDLE_HEIGHT) / 2.0f;
    game.paddle_right_y = (GAME_HEIGHT - PADDLE_HEIGHT) / 2.0f;

    game.ball_x = GAME_WIDTH / 2.0f;
    game.ball_y = GAME_HEIGHT / 2.0f;
    game.ball_vx = 300.0f;
    game.ball_vy = 150.0f;

    game.score_left = 0;
    game.score_right = 0;
    game.winner_id = 0;
    game.status = GAME_PLAYING;

    return game;
}

int game_move_paddle(game_t *game, int player_id,
                     move_direction_t direction, float dt) {
    if (game == NULL) {
        return -1;
    }

    if (game->status != GAME_PLAYING || !(dt > 0.0f && dt <= 1.0f)) {
        return -1;
    }

    if (direction != MOVE_UP &&
        direction != MOVE_STOP &&
        direction != MOVE_DOWN) {
        return -1;
    }

    float *paddle_y;

    if (player_id == game->player_left_id) {
        paddle_y = &game->paddle_left_y;
    } else if (player_id == game->player_right_id) {
        paddle_y = &game->paddle_right_y;
    } else {
        return -1;
    }

    *paddle_y += direction * PADDLE_SPEED * dt;

    if (*paddle_y < 0.0f) {
        *paddle_y = 0.0f;
    }

    float max_y = GAME_HEIGHT - PADDLE_HEIGHT;

    if (*paddle_y > max_y) {
        *paddle_y = max_y;
    }

    return 0;
}

/* Preparar un nuevo saque sin borrar el marcador. */
static void game_reset_ball(game_t *game, int direction) {
    game->ball_x = GAME_WIDTH / 2.0f;
    game->ball_y = GAME_HEIGHT / 2.0f;

    game->ball_vx = direction * 300.0f;
    game->ball_vy = 150.0f;
}

int game_update(game_t *game, float dt) {
    if (game == NULL) {
        return -1;
    }

    /* Solo actualizar partidas activas con un tiempo válido. */
    if (game->status != GAME_PLAYING ||
        !(dt > 0.0f && dt <= 1.0f)) {
        return -1;
    }

    float remaining = dt;

    /* Dividir el tiempo en pequeños pasos de simulación. */
    while (remaining > 0.0f) {
        float step = remaining;

        if (step > 0.005f) {
            step = 0.005f;
        }

        float previous_x = game->ball_x;

        /* Mover la pelota según su velocidad. */
        game->ball_x += game->ball_vx * step;
        game->ball_y += game->ball_vy * step;

        /* Rebotes contra el techo y el piso. */
        float min_y = BALL_RADIUS;
        float max_y = GAME_HEIGHT - BALL_RADIUS;

        while (game->ball_y < min_y || game->ball_y > max_y) {
            if (game->ball_y < min_y) {
                game->ball_y = 2.0f * min_y - game->ball_y;
            } else {
                game->ball_y = 2.0f * max_y - game->ball_y;
            }

            game->ball_vy = -game->ball_vy;
        }

        float left_contact =
            PADDLE_LEFT_X + PADDLE_WIDTH + BALL_RADIUS;

        float right_contact =
            PADDLE_RIGHT_X - BALL_RADIUS;

        /* Rebote contra la cara interior de la paleta izquierda. */
        if (game->ball_vx < 0.0f &&
            previous_x >= left_contact &&
            game->ball_x <= left_contact &&
            game->ball_y + BALL_RADIUS >= game->paddle_left_y &&
            game->ball_y - BALL_RADIUS <=
                game->paddle_left_y + PADDLE_HEIGHT) {

            game->ball_x = 2.0f * left_contact - game->ball_x;
            game->ball_vx = -game->ball_vx;
        }

        /* Rebote contra la cara interior de la paleta derecha. */
        else if (game->ball_vx > 0.0f &&
                 previous_x <= right_contact &&
                 game->ball_x >= right_contact &&
                 game->ball_y + BALL_RADIUS >= game->paddle_right_y &&
                 game->ball_y - BALL_RADIUS <=
                     game->paddle_right_y + PADDLE_HEIGHT) {

            game->ball_x = 2.0f * right_contact - game->ball_x;
            game->ball_vx = -game->ball_vx;
        }

        /* Si sale por la izquierda, marca el jugador derecho. */
        if (game->ball_x + BALL_RADIUS <= 0.0f) {
            game->score_right += 1;

            if (game->score_right >= WINNING_SCORE) {
                game->winner_id = game->player_right_id;
                game->status = GAME_FINISHED;
                game->ball_vx = 0.0f;
                game->ball_vy = 0.0f;
            } else {
                game_reset_ball(game, -1);
            }

            return 0;
        }

        /* Si sale por la derecha, marca el jugador izquierdo. */
        if (game->ball_x - BALL_RADIUS >= GAME_WIDTH) {
            game->score_left += 1;

            if (game->score_left >= WINNING_SCORE) {
                game->winner_id = game->player_left_id;
                game->status = GAME_FINISHED;
                game->ball_vx = 0.0f;
                game->ball_vy = 0.0f;
            } else {
                game_reset_ball(game, 1);
            }

            return 0;
        }

        remaining -= step;
    }

    return 0;
}
/* Terminar una partida por abandono de uno de sus jugadores. */
int game_forfeit(game_t *game, int player_id) {
    if (game == NULL || game->status != GAME_PLAYING) {
        return -1;
    }

    /* El rival del jugador que abandona será el ganador. */
    if (player_id == game->player_left_id) {
        game->winner_id = game->player_right_id;
    } else if (player_id == game->player_right_id) {
        game->winner_id = game->player_left_id;
    } else {
        /* El identificador no pertenece a esta partida. */
        return -1;
    }

    game->status = GAME_FINISHED;
    game->ball_vx = 0.0f;
    game->ball_vy = 0.0f;

    return 0;
}