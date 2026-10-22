#ifndef GAME_H
#define GAME_H
#define BALL_RADIUS 10.0f
#define PADDLE_WIDTH 20.0f
#define PADDLE_MARGIN 30.0f

#define PADDLE_LEFT_X PADDLE_MARGIN
#define PADDLE_RIGHT_X (GAME_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH)
#define WINNING_SCORE 5

typedef enum {
    GAME_WAITING,
    GAME_PLAYING,
    GAME_FINISHED
} game_status_t;

typedef struct {
    int id;

    int player_left_id;
    int player_right_id;

    float paddle_left_y;
    float paddle_right_y;

    float ball_x;
    float ball_y;
    float ball_vx;
    float ball_vy;

    int score_left;
    int score_right;
    int winner_id; /* 0 significa que todavía no hay ganador. */

    game_status_t status;
} game_t;

#define GAME_WIDTH 800
#define GAME_HEIGHT 600
#define PADDLE_HEIGHT 100

game_t game_create(int id, int player_left_id, int player_right_id);

#define PADDLE_SPEED 300.0f

typedef enum {
    MOVE_UP = -1,
    MOVE_STOP = 0,
    MOVE_DOWN = 1
} move_direction_t;

int game_move_paddle(game_t *game, int player_id,
                     move_direction_t direction, float dt);

int game_update(game_t *game, float dt);

int game_forfeit(game_t *game, int player_id);

#endif