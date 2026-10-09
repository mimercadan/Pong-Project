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

int game_update(game_t *game, float dt) {
    if (game == NULL) {
        return -1;
    }

    if (game->status != GAME_PLAYING ||
        !(dt > 0.0f && dt <= 1.0f)) {
        return -1;
    }

    /* Actualizar la posición según la velocidad y el tiempo. */
    game->ball_x += game->ball_vx * dt;
    game->ball_y += game->ball_vy * dt;

    float min_y = BALL_RADIUS;
    float max_y = GAME_HEIGHT - BALL_RADIUS;

    /*
     * Reflejar la distancia que sobrepasó el borde.
     * Repetir si el desplazamiento atravesó varios bordes.
     */
    while (game->ball_y < min_y || game->ball_y > max_y) {
        if (game->ball_y < min_y) {
            game->ball_y = 2.0f * min_y - game->ball_y;
            game->ball_vy = -game->ball_vy;
        } else {
            game->ball_y = 2.0f * max_y - game->ball_y;
            game->ball_vy = -game->ball_vy;
        }
    }

    return 0;
}