#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "renderer.h"
#include "game.h"

#define FALL_TIMER ((1000ull * 1000 * 1000))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(a, l, u) MIN(MAX(a, l), u)

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

static void tetrimino_load_offset(tetrimino_type type, vec2i offsets[4]) {
    memcpy(offsets, tetrimino_offsets[type], sizeof(vec2i) * 4);
}

static vec2i tetrimino_rotate(vec2i offset, tetrimino_type type, tetrimino_rotation rot) {
    if (type == TETRIMINO_TYPE_O) {
        return offset;
    }

    switch (rot) {
    case TETRIMINO_ROTATION_0:
        return offset;
    case TETRIMINO_ROTATION_90:
        return (vec2i) { .x = -offset.y, .y = offset.x };
    case TETRIMINO_ROTATION_180:
        return (vec2i) { .x = -offset.x, .y = -offset.y };
    case TETRIMINO_ROTATION_270:
        return (vec2i) { .x = offset.y, .y = -offset.x };
    default:
      return offset;
    }
}

static bool tetrimino_hor_collision_test(const game_state *state, vec2i offsets[4], int x, int y)
{
    for (int i = 0; i < 4; i++) {
        vec2i rotate_offset = tetrimino_rotate(offsets[i], state->type, state->rot);
        int block_x = rotate_offset.x + x;
        int block_y = rotate_offset.y + y;

        if (block_x >= GAME_GRID_WIDTH || block_x < 0 || state->grid[block_y][block_x] & BLOCK_STATE_FILLED) {
            return true;
        }
    }

    return false;
}

static bool tetrimino_vert_collision_test(const game_state *state, vec2i offsets[4], int x, int y)
{
    for (int i = 0; i < 4; i++) {
        vec2i rotate_offset = tetrimino_rotate(offsets[i], state->type, state->rot);
        int block_x = rotate_offset.x + x;
        int block_y = rotate_offset.y + y;

        if (block_y >= GAME_GRID_HEIGHT || state->grid[block_y][block_x] & BLOCK_STATE_FILLED) {
            return true;
        }
    }

    return false;
}

static void merge_into_board(game_state *state, vec2i offsets[4])
{
    for (int i = 0; i < 4; i++) {
        vec2i rotate_offset = tetrimino_rotate(offsets[i], state->type, state->rot);
        int x = rotate_offset.x + state->x;
        int y = rotate_offset.y + state->y;

        state->grid[y][x] = BLOCK_STATE_FILLED | (tetrimino_colors[state->type] & 0xff);
    }
}

static void clear_rows(game_state *state)
{
    int write = GAME_GRID_HEIGHT - 1;

    for (int y = GAME_GRID_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < GAME_GRID_WIDTH; x++) {
            if (!(state->grid[y][x] & BLOCK_STATE_FILLED)) {
                full = false;
                break;
            }
        }

        if (!full) {
            if (write != y) {
                for (int x = 0; x < GAME_GRID_WIDTH; x++) {
                    state->grid[write][x] = state->grid[y][x];
                }
            }
            write--;
        }
    }

    for (int y = write; y >= 0; y--) {
        for (int x = 0; x < GAME_GRID_WIDTH; x++) {
            state->grid[y][x] = BLOCK_STATE_EMPTY;
        }
    }
}

static void tetrimino_bag_shuffle(tetrimino_bag *bag)
{
    for (int i = 0; i < TETRIMINO_TYPE_COUNT; i++) {
        bag->queue[i] = i;
    }

    // Fisher-yates shuffle
    for (int i = TETRIMINO_TYPE_COUNT - 1; i > 0; i--) {
        // TODO: replace rand with a proper implementation due to its poor quality.
        // Also, each game should have its own random seed instead of sharing it with other
        // game sessions.
        int j = rand() % (i + 1);
        uint8_t tmp = bag->queue[i];
        bag->queue[i] = bag->queue[j];
        bag->queue[j] = tmp;
    }

    bag->count = TETRIMINO_TYPE_COUNT;
}

static tetrimino_type tetrimino_bag_pull(tetrimino_bag *bag)
{
    if (bag->count == 0) {
        tetrimino_bag_shuffle(bag);
    }

    return bag->queue[--bag->count];
}

static void place_new_tetrimino(game_state *state)
{
    state->type = tetrimino_bag_pull(&state->bag);
    state->x = 4;
    state->y = 21;
    state->fall_timer = FALL_TIMER;
    state->rot = TETRIMINO_ROTATION_0;
}

static void hard_drop(game_state *state, vec2i offsets[4])
{
    for (int y = state->y; y < GAME_GRID_HEIGHT; y++) {
        if (tetrimino_vert_collision_test(state, offsets, state->x, y + 1)) {
            state->y = y;
            break;
        }
    }
}

static void tetrimino_render(renderer *renderer, tetrimino_type type, tetrimino_rotation rot, int x, int y, int offset_x, int offset_y) {
    vec2i piece_offsets[4];
    memcpy(piece_offsets, tetrimino_offsets[type], sizeof(piece_offsets));

    for (int i = 0; i < 4; i++) {
        vec2i piece_offset_orig = piece_offsets[i];
        vec2i piece_offset = tetrimino_rotate(piece_offset_orig, type, rot);
        // Squares occupies two characters wide
        renderer_square(renderer, offset_x + (x + piece_offset.x) * 2, offset_y + piece_offset.y + y, tetrimino_colors[type]);
    }
}

static void board_render(const game_state *state, renderer *renderer, int offset_x, int offset_y)
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

static void tetrimino_ghost_render(const game_state *state, renderer *renderer, int offset_x, int offset_y)
{
    vec2i offsets[4];
    tetrimino_load_offset(state->type, offsets);

    int y;
    for (y = state->y; y < GAME_GRID_HEIGHT; y++) {
        if (tetrimino_vert_collision_test(state, offsets, state->x, y + 1)) {
            break;
        }
    }

    for (int i = 0; i < 4; i++) {
        vec2i piece_offset = tetrimino_rotate(offsets[i], state->type, state->rot);
        renderer_square(renderer, offset_x + (state->x + piece_offset.x) * 2, offset_y + piece_offset.y + y - 20, ANSI_COLOR_BRIGHT_BLACK);
    }
}

void game_init(game_state *state)
{
    memset(state->grid, 0, sizeof(state->grid));
    tetrimino_bag_shuffle(&state->bag);
    place_new_tetrimino(state);
}

void game_input(game_state *state, key_press press)
{
    // Should repeated keys move the block twice?
    if ((press.kind == KEY_KIND_CHAR && (press.ch == 'j' || press.ch == 'J'))
            || press.kind == KEY_KIND_ARROW_DOWN) {
        state->actions |= GAME_ACTION_SOFT_DROP;
    } else if (press.kind == KEY_KIND_CHAR && press.ch == ' ') {
        state->actions |= GAME_ACTION_HARD_DROP;
    } else if ((press.kind == KEY_KIND_CHAR && (press.ch == 'h' || press.ch == 'H'))
        || press.kind == KEY_KIND_ARROW_LEFT) {
        state->actions |= GAME_ACTION_MOVE_LEFT;
    } else if ((press.kind == KEY_KIND_CHAR && (press.ch == 'l' || press.ch == 'L'))
        || press.kind == KEY_KIND_ARROW_RIGHT) {
        state->actions |= GAME_ACTION_MOVE_RIGHT;
    } else if ((press.kind == KEY_KIND_CHAR && (press.ch == 'e' || press.ch == 'E'))) {
        state->actions |= GAME_ACTION_ROTATE_RIGHT;
    } else if ((press.kind == KEY_KIND_CHAR && (press.ch == 'q' || press.ch == 'Q'))) {
        state->actions |= GAME_ACTION_ROTATE_LEFT;
    }
}

void game_update(game_state *state, uint64_t dt_ns)
{
    vec2i offsets[4];
    tetrimino_load_offset(state->type, offsets);

    if (state->actions & GAME_ACTION_HARD_DROP) {
        hard_drop(state, offsets);
        merge_into_board(state, offsets);
        place_new_tetrimino(state);
        clear_rows(state);
    } else {
        int dx = 0;
        int dy = 0;

        if (state->actions & GAME_ACTION_MOVE_LEFT) {
            dx -= 1;
        }

        if (state->actions & GAME_ACTION_MOVE_RIGHT) {
            dx += 1;
        }

        // TODO: Does soft drop needs a cooldown timer?
        if (state->actions & GAME_ACTION_SOFT_DROP) {
            dy += 1;
        }

        if (state->actions & GAME_ACTION_ROTATE_LEFT) {
            state->rot = (state->rot + TETRIMINO_ROTATION_COUNT - 1) % TETRIMINO_ROTATION_COUNT;
        }
        if (state->actions & GAME_ACTION_ROTATE_RIGHT) {
            state->rot = (state->rot + 1) % TETRIMINO_ROTATION_COUNT;
        }

        // TODO: Implement a collision test that checks for horizontal and vertical collisions
        if (!tetrimino_hor_collision_test(state, offsets, state->x + dx, state->y + dy)
            && !tetrimino_vert_collision_test(state, offsets, state->x + dx, state->y + dy)) {
            state->x += dx;
            state->y += dy;
        }

        // TODO: Implement fix update step
        if (state->fall_timer < dt_ns) {
            // If we would collide with the board when moving down, add the piece to the board
            if (tetrimino_vert_collision_test(state, offsets, state->x, state->y + 1)) {
                merge_into_board(state, offsets);
                place_new_tetrimino(state);
                clear_rows(state);
            } else {
                state->y++;
                state->fall_timer = FALL_TIMER;
            }
        } else {
            state->fall_timer -= dt_ns;
        }
    }

    // Consume all the actions
    state->actions = GAME_ACTION_NO_ACTION;
}

void game_render(const game_state *state, renderer *renderer)
{
    int offset_x = 1;
    int offset_y = 1;

    renderer_box(renderer, offset_x, offset_y, (GAME_GRID_WIDTH) * 2, GAME_GRID_HEIGHT - 20);
    tetrimino_ghost_render(state, renderer, offset_x, offset_y);
    tetrimino_render(renderer, state->type, state->rot, state->x, state->y - 20, offset_x, offset_y);
    board_render(state, renderer, offset_x, offset_y);
}
