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

const vec2i tetrimino_offsets[TETRIMINO_TYPE_COUNT][4] = {
    [TETRIMINO_TYPE_O] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} },
    [TETRIMINO_TYPE_I] = { {0, 0}, {1, 0}, {-1, 0}, {2, 0} },
    [TETRIMINO_TYPE_T] = { {0, 0}, {-1, 0}, {1, 0}, {0, -1} },
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

void tetrimino_render(grid *grid, tetrimino_type type, int x, int y, int offset_x, int offset_y) {
    vec2i piece_offsets[4];
    memcpy(piece_offsets, tetrimino_offsets[type], sizeof(piece_offsets));

    for (int i = 0; i < 4; i++) {
        vec2i piece_offset = piece_offsets[i];
        // Squares occupies two characters wide
        grid_square(grid, offset_x + (x + piece_offset.x) * 2, offset_y + piece_offset.y + y, tetrimino_colors[type]);
    }
}

void game_init(game_state *state) {
    memset(state->grid, 0, sizeof(state->grid));
    state->type = TETRIMINO_TYPE_O;
    state->x = 4;
    state->y = 21;
    state->fallout_timer = FALLOUT_TIMER;
}

void game_update(game_state *state, uint64_t dt_ns) {
    if (state->fallout_timer < dt_ns) {
        vec2i offsets[4];
        memcpy(offsets, tetrimino_offsets[state->type], sizeof(offsets));

        bool collided = false;
        for (int i = 0; i < 4; i++) {
            int x = offsets[i].x + state->x;
            int y = offsets[i].y + state->y + 1;

            if (y >= GAME_GRID_HEIGHT) {
                collided = true;
                break;
            }

            if (state->grid[y][x] & BLOCK_STATE_FILLED) {
                collided = true;
                break;
            }
        }

        if (collided) {
            for (int i = 0; i < 4; i++) {
                int x = offsets[i].x + state->x;
                int y = offsets[i].y + state->y;

                state->grid[y][x] = BLOCK_STATE_FILLED | (tetrimino_colors[state->type] & 0xff);
            }

            state->type = (state->type + 1) % TETRIMINO_TYPE_COUNT;
            state->x = 4;
            state->y = 21;
            state->fallout_timer = FALLOUT_TIMER;
        } else {
            state->y++;
            state->fallout_timer = FALLOUT_TIMER;
        }
    } else {
        state->fallout_timer -= dt_ns;
    }
}

void board_render(game_state *state, grid *grid, int offset_x, int offset_y)
{
    for (int y = 20; y < GAME_GRID_HEIGHT; y++) {
        for (int x = 0; x < GAME_GRID_WIDTH; x++) {
            block_state block = state->grid[y][x];
            if (block & BLOCK_STATE_FILLED) {
                grid_square(grid, offset_x + (x * 2), offset_y + y - 20, block & 0xff);
            }
        }
    }
}

void game_render(game_state *state, grid *grid) {
    int offset_x = 1;
    int offset_y = 1;

    grid_box(grid, offset_x, offset_y, (GAME_GRID_WIDTH) * 2, GAME_GRID_HEIGHT - 20);
    board_render(state, grid, offset_x, offset_y);
    tetrimino_render(grid, state->type, state->x, state->y - 20, offset_x, offset_y);
}

#endif // TETRIS_SSH_GAME_H
