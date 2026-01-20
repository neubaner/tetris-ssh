#include <stdint.h>

#include "renderer.h"
#include "game.h"

#define FALLOUT_TIMER ((300ull * 1000 * 1000))

static const vec2i tetrimino_offsets[TETRIMINO_TYPE_COUNT][4] = {
    [TETRIMINO_TYPE_O] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} },
    [TETRIMINO_TYPE_I] = { {0, 0}, {1, 0}, {-1, 0}, {2, 0} },
    [TETRIMINO_TYPE_T] = { {0, 0}, {-1, 0}, {1, 0}, {0, -1} },
    [TETRIMINO_TYPE_J] = { {0, 0}, {-1, 0}, {1, 0}, {-1, -1} },
    [TETRIMINO_TYPE_L] = { {0, 0}, {-1, 0}, {1, 0}, {1, -1} },
    [TETRIMINO_TYPE_S] = { {0, 0}, {-1, 0}, {0, -1}, {1, -1} },
    [TETRIMINO_TYPE_Z] = { {0, 0}, {0, -1}, {-1, -1}, {1, 0} },
};

static const ansi_color tetrimino_colors[TETRIMINO_TYPE_COUNT] = {
    [TETRIMINO_TYPE_O] = ANSI_COLOR_BRIGHT_YELLOW,
    [TETRIMINO_TYPE_I] = ANSI_COLOR_CYAN,
    [TETRIMINO_TYPE_T] = ANSI_COLOR_MAGENTA,
    [TETRIMINO_TYPE_J] = ANSI_COLOR_BLUE,
    [TETRIMINO_TYPE_L] = ANSI_COLOR_YELLOW,
    [TETRIMINO_TYPE_S] = ANSI_COLOR_GREEN,
    [TETRIMINO_TYPE_Z] = ANSI_COLOR_RED,
};

void game_init(game_state *state)
{
    memset(state->grid, 0, sizeof(state->grid));
    state->type = TETRIMINO_TYPE_O;
    state->x = 4;
    state->y = 21;
    state->fallout_timer = FALLOUT_TIMER;
}

void game_update(game_state *state, uint64_t dt_ns)
{
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

static void tetrimino_render(renderer *renderer, tetrimino_type type, int x, int y, int offset_x, int offset_y) {
    vec2i piece_offsets[4];
    memcpy(piece_offsets, tetrimino_offsets[type], sizeof(piece_offsets));

    for (int i = 0; i < 4; i++) {
        vec2i piece_offset = piece_offsets[i];
        // Squares occupies two characters wide
        renderer_square(renderer, offset_x + (x + piece_offset.x) * 2, offset_y + piece_offset.y + y, tetrimino_colors[type]);
    }
}

static void board_render(game_state *state, renderer *renderer, int offset_x, int offset_y)
{
    for (int y = 20; y < GAME_GRID_HEIGHT; y++) {
        for (int x = 0; x < GAME_GRID_WIDTH; x++) {
            block_state block = state->grid[y][x];
            if (block & BLOCK_STATE_FILLED) {
                renderer_square(renderer, offset_x + (x * 2), offset_y + y - 20, block & 0xff);
            }
        }
    }
}

void game_render(game_state *state, renderer *renderer)
{
    int offset_x = 1;
    int offset_y = 1;

    renderer_box(renderer, offset_x, offset_y, (GAME_GRID_WIDTH) * 2, GAME_GRID_HEIGHT - 20);
    board_render(state, renderer, offset_x, offset_y);
    tetrimino_render(renderer, state->type, state->x, state->y - 20, offset_x, offset_y);
}
