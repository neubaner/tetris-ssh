#ifndef TETRIS_SSH_RENDERER_H
#define TETRIS_SSH_RENDERER_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unicode.h"

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

#define ANSI_COLOR_ENUM(name, value) ANSI_COLOR_##name = value, ANSI_COLOR_BRIGHT_##name = value + 60,

typedef enum ansi_color {
    ANSI_COLOR_X(ANSI_COLOR_ENUM)
} ansi_color;

typedef uint8_t ansi_color_escape;

#define ANSI_FG(color) ((color) + 30)
#define ANSI_BG(color) ((color) + 40)

typedef struct cell {
    ansi_color fg;
    ansi_color bg;
    utf8_char ch;
} cell;

#define GRID_HEIGHT (30 * 1)
#define GRID_WIDTH (20 * 2)

typedef struct grid {
    cell cells[GRID_HEIGHT][GRID_WIDTH];
} grid;

typedef struct cursor {
    int x;
    int y;
} cursor;

typedef struct buf_writer {
    char *content;
    size_t capacity;
    size_t cursor;
} buf_writer;

static void buf_writer_write_unchecked(buf_writer *writer, const char *data, size_t size)
{
    memcpy(&writer->content[writer->cursor], data, size);
    writer->cursor += size;
}

static void buf_writer_write(buf_writer *writer, const char *data, size_t size)
{
    assert(writer->capacity - writer->cursor >= size);
    buf_writer_write_unchecked(writer, data, size);
}

static void buf_writer_utf8(buf_writer *writer, utf8_char ch)
{
    const size_t remaining_capacity = writer->capacity - writer->cursor;
    writer->cursor += utf8_char_write(ch, &writer->content[writer->cursor], remaining_capacity);
}

static void buf_writer_int(buf_writer *writer, int value)
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

static void buf_writer_cursor_to(buf_writer *writer, int x, int y)
{
    buf_writer_write(writer, "\x1b[", 2);
    buf_writer_int(writer, y);
    buf_writer_write(writer, ";", 1);
    buf_writer_int(writer, x);
    buf_writer_write(writer, "H", 1);
}

static void buf_writer_color1(buf_writer *writer, ansi_color_escape color)
{
    buf_writer_write(writer, "\x1b[", 2);
    buf_writer_int(writer, color);
    buf_writer_write(writer, "m", 1);
}

static void buf_writer_color2(buf_writer *writer, ansi_color_escape color1, ansi_color_escape color2)
{
    buf_writer_write(writer, "\x1b[", 2);
    buf_writer_int(writer, color1);
    buf_writer_write(writer, ";", 1);
    buf_writer_int(writer, color2);
    buf_writer_write(writer, "m", 1);
}

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_RESET_COLOR "\x1b[0m"

size_t renderer_clear(char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    const char clear_all[] = ANSI_RESET_COLOR ANSI_ERASE_ENTITY_SCREEN ANSI_CURSOR_HOME_POSITION ANSI_HIDE_CURSOR;
    buf_writer_write(&writer, clear_all, sizeof(clear_all) - 1);

    return writer.cursor;
}

void grid_clear(grid *grid) {
    memset(grid, 0 , sizeof(*grid));
}

void grid_square(grid *grid, int x, int y, ansi_color color)
{
    grid->cells[y][x] = (cell) { .bg = color, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {' '} };
    grid->cells[y][x + 1] = (cell) { .bg = color, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {' '} };
}

void grid_string(grid *grid, const char *str, int x, int y)
{
    // TODO: Handle utf-8
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        grid->cells[y][x + i] = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {c} };
    }
}

void grid_box(grid *grid, int x, int y, int width, int height)
{
    grid->cells[y][x]                  = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_DOWN_RIGHT };
    grid->cells[y][x + width]          = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_DOWN_LEFT };
    grid->cells[y + height][x]         = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_UP_RIGHT };
    grid->cells[y + height][x + width] = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_UP_LEFT };

    for (int i = 1; i < width; i++) {
        grid->cells[y][x + i] = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_HORIZONTAL };
        grid->cells[y + height][x + i] = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_HORIZONTAL };
    }

    for (int i = 1; i < height; i++) {
        grid->cells[y + i][x] = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_VERTICAL };
        grid->cells[y + i][x + width] = (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_VERTICAL };
    }
}

size_t grid_render(grid *grid, char *buffer, size_t buffer_size)
{
    int cursor_x = 0;
    int cursor_y = 0;
    ansi_color fg = ANSI_COLOR_DEFAULT;
    ansi_color bg = ANSI_COLOR_DEFAULT;
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            cell cell = grid->cells[y][x];
            if (cell.ch.bytes[0] == '\0') {
                continue;
            }

            if (cursor_x != x || cursor_y != y) {
                buf_writer_cursor_to(&writer, x + 1, y + 1);
            }

            if (fg != cell.fg && bg != cell.bg) {
                buf_writer_color2(&writer, ANSI_FG(cell.fg), ANSI_BG(cell.bg));
                fg = cell.fg;
                bg = cell.bg;
            } else if (bg != cell.bg) {
                buf_writer_color1(&writer, ANSI_BG(cell.bg));
                bg = cell.bg;
            } else if (fg != cell.fg) {
                buf_writer_color1(&writer, ANSI_FG(cell.fg));
                fg = cell.fg;
            }
            buf_writer_utf8(&writer, cell.ch);
            cursor_x = x + 1;
            cursor_y = y;
        }
    }

    return writer.cursor;
}

#endif // TETRIS_SSH_RENDERER_H
