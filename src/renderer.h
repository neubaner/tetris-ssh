#ifndef TETRIS_SSH_RENDERER_H
#define TETRIS_SSH_RENDERER_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ANSI_COLOR_X(X) \
    X(BLACK, 0) \
    X(RED, 1) \
    X(GREEN, 2) \
    X(YELLOW, 3) \
    X(BLUE, 4) \
    X(MAGENTA, 5) \
    X(CYAN, 6) \
    X(WHITE, 7) \
    X(DEFAULT, 9)

#define ANSI_COLOR_ENUM(name, value) ANSI_COLOR_##name = value,

typedef enum ansi_color {
    ANSI_COLOR_X(ANSI_COLOR_ENUM)
} ansi_color;

#define ANSI_FG(color) (30 + color)
#define ANSI_BG(color) (40 + color)

#define ANSI_COLOR_TABLE(name, value) \
    [ANSI_FG(ANSI_COLOR_##name)] = {'3', value + '0'}, \
    [ANSI_BG(ANSI_COLOR_##name)] = {'4', value + '0'},

static const char color_table[][2] = {
    ANSI_COLOR_X(ANSI_COLOR_TABLE)
};

typedef struct cell {
    ansi_color fg;
    ansi_color bg;
    char ch;
} cell;

#define GRID_HEIGHT (10 * 1)
#define GRID_WIDTH (20 * 2)

typedef struct grid {
    char front_idx;
    cell cells[2][GRID_HEIGHT][GRID_WIDTH];
    ansi_color fg;
    ansi_color bg;
} grid;

grid grid_new(void)
{
    grid grid;
    grid.front_idx = 0;

    for (int buffer = 0; buffer < 2; buffer++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                grid.cells[buffer][y][x] = (cell){
                    .ch = ' ',
                    .fg = ANSI_COLOR_DEFAULT,
                    .bg = ANSI_COLOR_DEFAULT,
                };
            }
        }
    }

    return grid;
}

typedef struct cursor {
    int x;
    int y;
} cursor;

typedef struct buf_writer {
    char *content;
    size_t capacity;
    size_t cursor;
} buf_writer;

void buf_writer_write(buf_writer *writer, const char *data, size_t size)
{
    assert(writer->cursor + size <= writer->capacity);
    memcpy(&writer->content[writer->cursor], data, size);
    writer->cursor += size;
}

void buf_writer_int(buf_writer *writer, int value)
{
    char buffer[16];
    char *cursor = buffer + sizeof(buffer) - 1;
    int minus_sign = value < 0 ? true : false;

    do {
        int temp_value = value;
        value = value / 10;
        int removed_digit = temp_value - (value * 10);

        *cursor-- = (char)(removed_digit + '0');
    } while(value);

    if (minus_sign) {
        *cursor-- = '-';
    }

    buf_writer_write(writer, cursor + 1, sizeof(buffer) - (cursor - buffer) - 1);
}

void buf_writer_cursor_to(buf_writer *writer, int x, int y)
{
    buf_writer_write(writer, "\x1b[", 2);
    buf_writer_int(writer, y);
    buf_writer_write(writer, ";", 1);
    buf_writer_int(writer, x);
    buf_writer_write(writer, "H", 1);
}

void buf_writer_color1(buf_writer *writer, ansi_color color)
{
    buf_writer_write(writer, "\x1b[", 2);
    buf_writer_write(writer, color_table[color], sizeof(color_table[color]));
    buf_writer_write(writer, "m", 1);
}

void buf_writer_color2(buf_writer *writer, ansi_color color1, ansi_color color2)
{
    buf_writer_write(writer, "\x1b[", 2);
    buf_writer_write(writer, color_table[color1], sizeof(color_table[color1]));
    buf_writer_write(writer, ";", 1);
    buf_writer_write(writer, color_table[color2], sizeof(color_table[color2]));
    buf_writer_write(writer, "m", 1);
}

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

static int x = 0;
static int y = 0;

size_t renderer_clear(char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    buf_writer_write(&writer, ANSI_ERASE_ENTITY_SCREEN, sizeof(ANSI_ERASE_ENTITY_SCREEN) - 1);
    buf_writer_write(&writer, ANSI_CURSOR_HOME_POSITION, sizeof(ANSI_CURSOR_HOME_POSITION) - 1);
    buf_writer_write(&writer, ANSI_HIDE_CURSOR, sizeof(ANSI_HIDE_CURSOR) - 1);

    return writer.cursor;
}

void grid_rect(grid *grid, int x, int y)
{
    size_t front_idx = grid->front_idx;
    grid->cells[front_idx][y][x] = (cell) { .bg = ANSI_COLOR_RED, .ch = ' '};
    grid->cells[front_idx][y][x + 1] = (cell) { .bg = ANSI_COLOR_BLUE, .ch = ' '};
}

size_t grid_render(grid *grid, char *buffer, size_t buffer_size)
{
    const size_t front_idx = grid->front_idx;
    const size_t back_idx = front_idx == 0 ? 1 : 0;

    int cursor_x = 0;
    int cursor_y = 0;
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            cell front_cell = grid->cells[front_idx][y][x];
            cell back_cell = grid->cells[back_idx][y][x];

            bool bg_equal = front_cell.bg == back_cell.bg;
            bool fg_equal = front_cell.fg == back_cell.fg;

            if (front_cell.ch == back_cell.ch && bg_equal && fg_equal) {
                continue;
            }

            if (cursor_x != x || cursor_y != y) {
                buf_writer_cursor_to(&writer, x + 1, y + 1);
            }

            if (grid->fg != front_cell.fg && grid->bg != front_cell.bg) {
                buf_writer_color2(&writer, ANSI_FG(front_cell.fg), ANSI_BG(front_cell.bg));
                grid->fg = front_cell.fg;
                grid->bg = front_cell.bg;
            } else if (grid->bg != front_cell.bg) {
                buf_writer_color1(&writer, ANSI_BG(front_cell.bg));
                grid->bg = front_cell.bg;
            } else if (grid->fg != front_cell.fg) {
                buf_writer_color1(&writer, ANSI_FG(front_cell.fg));
                grid->fg = front_cell.fg;
            }
            buf_writer_write(&writer, &front_cell.ch, 1);
            cursor_x++;
        }
    }

    grid->front_idx = back_idx;

    return writer.cursor;
}

#endif // TETRIS_SSH_RENDERER_H
