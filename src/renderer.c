#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "renderer.h"

typedef uint8_t ansi_color_escape;

#define ANSI_FG(color) ((color) + 30)
#define ANSI_BG(color) ((color) + 40)

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

#define ANSI_ERASE_ENTIRE_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_RESET_COLOR "\x1b[0m"
#define ANSI_ENTER_ALT_SCREEN_SAVE_CURSOR "\x1b[?1049h"
#define ANSI_EXIT_ALT_SCREEN_RESTORE_CURSOR "\x1b[?1049l"

size_t renderer_enter_alt_screen(char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    buf_writer_write(&writer, ANSI_ENTER_ALT_SCREEN_SAVE_CURSOR, sizeof(ANSI_ENTER_ALT_SCREEN_SAVE_CURSOR) - 1);

    return writer.cursor;
}

size_t renderer_exit_alt_screen(char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    buf_writer_write(&writer, ANSI_EXIT_ALT_SCREEN_RESTORE_CURSOR, sizeof(ANSI_EXIT_ALT_SCREEN_RESTORE_CURSOR) - 1);

    return writer.cursor;
}

size_t renderer_clear(renderer *renderer, char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    // TODO: This doesn't seem to clear the terminal buffer. Do we need to leverage the alternative screen?
    const char clear_all[] = ANSI_RESET_COLOR ANSI_ERASE_ENTIRE_SCREEN ANSI_CURSOR_HOME_POSITION ANSI_HIDE_CURSOR;
    buf_writer_write(&writer, clear_all, sizeof(clear_all) - 1);

    memset(renderer->cells, 0, sizeof(renderer->cells));

    return writer.cursor;
}

void renderer_set_cell(renderer *renderer, int x, int y, cell cell)
{
    if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
        renderer->cells[renderer->front_buffer_idx][y][x] = cell;
    }
}

void renderer_square(renderer *renderer, int x, int y, ansi_color color)
{
    renderer_set_cell(renderer, x, y, (cell) { .bg = color, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {' '} });
    renderer_set_cell(renderer, x + 1, y, (cell) { .bg = color, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {' '} });
}

void renderer_string(renderer *renderer, const char *str, int x, int y)
{
    // TODO: Handle utf-8
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        renderer_set_cell(renderer, x + i, y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {c} });
    }
}

// Draws a box around the @x and @y position, that can fit @width and @height characters.
void renderer_box(renderer *renderer, int x, int y, int width, int height)
{
    int left_x = x - 1;
    int right_x = x + width;
    int top_y = y - 1;
    int bot_y = y + height;

    renderer_set_cell(renderer, left_x, top_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_DOWN_RIGHT });
    renderer_set_cell(renderer, right_x, top_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_DOWN_LEFT });
    renderer_set_cell(renderer, left_x, bot_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_UP_RIGHT });
    renderer_set_cell(renderer, right_x, bot_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_UP_LEFT });

    for (int xx = left_x + 1; xx < right_x; xx++) {
        renderer_set_cell(renderer, xx, top_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_HORIZONTAL });
        renderer_set_cell(renderer, xx, bot_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_HORIZONTAL });
    }

    for (int yy = top_y + 1; yy < bot_y; yy++) {
        renderer_set_cell(renderer, left_x, yy, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_VERTICAL });
        renderer_set_cell(renderer, right_x, yy, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_VERTICAL });
    }
}

size_t renderer_render(renderer *renderer, char *buffer, size_t buffer_size)
{
    int cursor_x = 0;
    int cursor_y = 0;
    int front_buffer_idx = renderer->front_buffer_idx;
    int back_buffer_idx = renderer->front_buffer_idx ? 0 : 1;
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };

    buf_writer_write(&writer, ANSI_CURSOR_HOME_POSITION, sizeof(ANSI_CURSOR_HOME_POSITION) - 1);

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            cell front_cell = renderer->cells[front_buffer_idx][y][x];
            cell back_cell = renderer->cells[back_buffer_idx][y][x];

            if (memcmp(&front_cell, &back_cell, sizeof(front_cell)) != 0)
            {
                if (cursor_x != x || cursor_y != y) {
                    buf_writer_cursor_to(&writer, x + 1, y + 1);
                }

                // TODO: Refactor
                unsigned char empty[4] = {0};
                if (memcmp(front_cell.ch.bytes, empty, sizeof(empty)) == 0) {
                    if (renderer->fg != ANSI_COLOR_DEFAULT && renderer->bg != ANSI_COLOR_DEFAULT) {
                        buf_writer_color2(&writer, ANSI_FG(ANSI_COLOR_DEFAULT), ANSI_BG(ANSI_COLOR_DEFAULT));
                        renderer->fg = ANSI_COLOR_DEFAULT;
                        renderer->bg = ANSI_COLOR_DEFAULT;
                    } else if (renderer->fg != ANSI_COLOR_DEFAULT) {
                        buf_writer_color1(&writer, ANSI_FG(ANSI_COLOR_DEFAULT));
                        renderer->fg = ANSI_COLOR_DEFAULT;
                    } else if (renderer->bg != ANSI_COLOR_DEFAULT) {
                        buf_writer_color1(&writer, ANSI_BG(ANSI_COLOR_DEFAULT));
                        renderer->bg = ANSI_COLOR_DEFAULT;
                    }

                    buf_writer_utf8(&writer, (utf8_char){{' '}});
                } else {
                    if (renderer->fg != front_cell.fg && renderer->bg != front_cell.bg) {
                        buf_writer_color2(&writer, ANSI_FG(front_cell.fg), ANSI_BG(front_cell.bg));
                        renderer->fg = front_cell.fg;
                        renderer->bg = front_cell.bg;
                    } else if (renderer->bg != front_cell.bg) {
                        buf_writer_color1(&writer, ANSI_BG(front_cell.bg));
                        renderer->bg = front_cell.bg;
                    } else if (renderer->fg != front_cell.fg) {
                        buf_writer_color1(&writer, ANSI_FG(front_cell.fg));
                        renderer->fg = front_cell.fg;
                    }
                    buf_writer_utf8(&writer, front_cell.ch);
                }
                cursor_x = x + 1;
                cursor_y = y;
            }
        }
    }

    // Swap buffer
    memset(renderer->cells[back_buffer_idx], 0, sizeof(renderer->cells[0]));
    renderer->front_buffer_idx = back_buffer_idx;

    return cursor_x != 0 && cursor_y != 0 ? writer.cursor : 0;
}
