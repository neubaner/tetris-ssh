#ifndef TETRIS_SSH_GAME_H
#define TETRIS_SSH_GAME_H

#include <stdint.h>

#include "renderer.h"
#include "input.h"

typedef struct vec2i {
    int x;
    int y;
} vec2i;

typedef enum tetrimino_type {
    TETRIMINO_TYPE_O,
    TETRIMINO_TYPE_I,
    TETRIMINO_TYPE_T,
    TETRIMINO_TYPE_J,
    TETRIMINO_TYPE_L,
    TETRIMINO_TYPE_S,
    TETRIMINO_TYPE_Z,
    TETRIMINO_TYPE_COUNT,
} tetrimino_type;

typedef enum tetrimino_rotation {
    TETRIMINO_ROTATION_0,
    TETRIMINO_ROTATION_90,
    TETRIMINO_ROTATION_180,
    TETRIMINO_ROTATION_270,
    TETRIMINO_ROTATION_COUNT,
} tetrimino_rotation;

#define GAME_GRID_HEIGHT 40
#define GAME_GRID_WIDTH 10

// The lower 8 bits represent the color and the higher 8 bits are for flags.
typedef enum block_state {
    BLOCK_STATE_EMPTY = 0,
    BLOCK_STATE_FILLED = 1 << 8,
    BLOCK_STATE_BRIGHT_YELLOW = BLOCK_STATE_FILLED | (ANSI_COLOR_BRIGHT_YELLOW & 0xff),
    BLOCK_STATE_CYAN = BLOCK_STATE_FILLED | (ANSI_COLOR_CYAN & 0xff),
    BLOCK_STATE_MANGETA = BLOCK_STATE_FILLED | (ANSI_COLOR_MAGENTA & 0xff),
    BLOCK_STATE_BLUE = BLOCK_STATE_FILLED | (ANSI_COLOR_BLUE & 0xff),
    BLOCK_STATE_YELLOW = BLOCK_STATE_FILLED | (ANSI_COLOR_YELLOW & 0xff),
    BLOCK_STATE_GREEN = BLOCK_STATE_FILLED | (ANSI_COLOR_GREEN & 0xff),
    BLOCK_STATE_RED = BLOCK_STATE_FILLED | (ANSI_COLOR_RED & 0xff),
} block_state;

typedef enum game_actions {
    GAME_ACTION_NO_ACTION = 0,
    GAME_ACTION_MOVE_LEFT = 1,
    GAME_ACTION_MOVE_RIGHT = 1 << 1,
    GAME_ACTION_MOVE_DOWN = 1 << 2,
    GAME_ACTION_ROTATE_LEFT = 1 << 3,
    GAME_ACTION_ROTATE_RIGHT = 1 << 4,
} game_actions;

typedef struct game_state {
    block_state grid[GAME_GRID_HEIGHT][GAME_GRID_WIDTH];
    tetrimino_type type;
    tetrimino_rotation rot;
    int x;
    int y;
    uint64_t fallout_timer;
    game_actions actions;
} game_state;

void game_init(game_state *state);
void game_input(game_state *state, key_press key_press);
void game_update(game_state *state, uint64_t dt_ns);
void game_render(const game_state *state, renderer *grid);

#endif // TETRIS_SSH_GAME_H
