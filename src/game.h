#ifndef TETRIS_SSH_GAME_H
#define TETRIS_SSH_GAME_H

#include "renderer.h"
#include <stdint.h>

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

// 1 second
#define FALLOUT_TIMER ((300ull * 1000 * 1000))

typedef struct game_state {
    block_state grid[GAME_GRID_HEIGHT][GAME_GRID_WIDTH];
    tetrimino_type type;
    int x;
    int y;
    uint64_t fallout_timer;
} game_state;

void game_init(game_state *state);
void game_update(game_state *state, uint64_t dt_ns);
void game_render(game_state *state, grid *grid);

#endif // TETRIS_SSH_GAME_H
