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

typedef enum block_state {
    BLOCK_STATE_EMPTY,
    BLOCK_STATE_BRIGHT_YELLOW,
    BLOCK_STATE_CYAN,
    BLOCK_STATE_MANGETA,
    BLOCK_STATE_BLUE,
    BLOCK_STATE_YELLOW,
    BLOCK_STATE_GREEN,
    BLOCK_STATE_RED,
} block_state;

// 1 second
#define FALLOUT_TIMER (1 * 1000 * 1000 * 1000)

typedef struct game_state {
    block_state grid[GAME_GRID_HEIGHT][GAME_GRID_WIDTH];
    tetrimino_type type;
    int x;
    int y;
    uint64_t fallout_timer;
} game_state;

const vec2i tetrimino_offsets[TETRIMINO_TYPE_COUNT][4] = {
    [TETRIMINO_TYPE_O] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} },
    [TETRIMINO_TYPE_I] = { {0, 0}, {1, 0}, {-1, 0}, {2, 0} },
    [TETRIMINO_TYPE_T] = { {0, 0}, {-1, 0}, {1, 0}, {0, 1} },
    [TETRIMINO_TYPE_J] = { {0, 0}, {-1, 0}, {1, 0}, {-1, -1} },
    [TETRIMINO_TYPE_L] = { {0, 0}, {-1, 0}, {1, 0}, {1, -1} },
    [TETRIMINO_TYPE_S] = { {0, 0}, {-1, 0}, {0, -1}, {1, -1} },
    [TETRIMINO_TYPE_Z] = { {0, 0}, {0, -1}, {-1, -1}, {1, 0} },
};

const ansi_color tetrimino_colors[TETRIMINO_TYPE_COUNT] = {
    [TETRIMINO_TYPE_O] = ANSI_COLOR_BRIGHT_YELLOW,
    [TETRIMINO_TYPE_I] = ANSI_COLOR_CYAN,
    [TETRIMINO_TYPE_T] = ANSI_COLOR_MAGENTA,
    [TETRIMINO_TYPE_J] = ANSI_COLOR_BLUE,
    [TETRIMINO_TYPE_L] = ANSI_COLOR_YELLOW,
    [TETRIMINO_TYPE_S] = ANSI_COLOR_GREEN,
    [TETRIMINO_TYPE_Z] = ANSI_COLOR_RED,
};

void tetrimino_render(grid *grid, int x, int y, tetrimino_type type) {
    vec2i offsets[4];
    memcpy(offsets, tetrimino_offsets[type], sizeof(offsets));

    for (int i = 0; i < 4; i++) {
        vec2i offset = offsets[i];
        // Squares occupies two characters wide
        grid_square(grid, (offset.x * 2) + x, offset.y + y, tetrimino_colors[type]);
    }
}

void game_init(game_state *state) {
    memset(state->grid, 0, sizeof(state->grid));
    state->type = TETRIMINO_TYPE_O;
    state->x = 5;
    state->y = 21;
    state->fallout_timer = FALLOUT_TIMER;
}

void game_update(game_state *state, uint64_t dt_ns) {
    if (state->fallout_timer < dt_ns) {
        state->y++;
        state->fallout_timer = FALLOUT_TIMER;
    } else {
        state->fallout_timer -= dt_ns;
    }
}

void game_render(game_state *state, grid *grid) {
    grid_box(grid, 0, 0, (GAME_GRID_WIDTH + 1) * 2, GAME_GRID_HEIGHT - 20 + 1);
    tetrimino_render(grid, state->x + 1, state->y - 20 + 1, state->type);
}

#endif // TETRIS_SSH_GAME_H
