#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "renderer.h"

typedef uint8_t ansi_color_escape;

typedef struct buf_writer {
    char *content;
    size_t capacity;
    size_t cursor;
} buf_writer;

typedef struct terminal_grid {
    cell cells[GRID_HEIGHT][GRID_WIDTH];
} terminal_grid;

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
    bool minus_sign = value < 0 ? true : false;

    do {
        int temp_value = value;
        value = value / 10;
        int removed_digit = temp_value - (value * 10);

        *cursor-- = (char)(removed_digit + '0');
    } while(value);

    if (minus_sign) {
        *cursor-- = '-';
    }

    buf_writer_write(writer, cursor + 1, sizeof(buffer) + (buffer - cursor - 1));
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
#define ANSI_RESET_COLOR "\x1b[0m"
#define ANSI_ENTER_ALT_SCREEN_SAVE_CURSOR "\x1b[?1049h"
#define ANSI_EXIT_ALT_SCREEN_RESTORE_CURSOR "\x1b[?1049l"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

size_t renderer_enter(char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    const char enter_all[] = ANSI_ENTER_ALT_SCREEN_SAVE_CURSOR ANSI_HIDE_CURSOR;
    buf_writer_write(&writer, enter_all, sizeof(enter_all) - 1);

    return writer.cursor;
}

size_t renderer_exit(char *buffer, size_t buffer_size)
{
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };
    buf_writer_write(&writer, ANSI_EXIT_ALT_SCREEN_RESTORE_CURSOR, sizeof(ANSI_EXIT_ALT_SCREEN_RESTORE_CURSOR) - 1);

    return writer.cursor;
}

void renderer_square(renderer *renderer, int x, int y, ansi_color color)
{
    int front_buffer_idx = renderer->front_buffer_idx;
    size_t count = renderer->command_buffer[front_buffer_idx].count;
    if (count < DRAW_COMMAND_MAX_SIZE)
    {
        renderer->command_buffer[front_buffer_idx].commands[count] = (struct draw_command) {
            .kind = DRAW_COMMAND_KIND_SQUARE,
            .square.x = x,
            .square.y = y,
            .square.color = color,
        };
        renderer->command_buffer[front_buffer_idx].count++;
    }
}

void renderer_string(renderer *renderer, const char *str, int x, int y)
{
    // TODO
    (void)renderer;
    (void)str;
    (void)x;
    (void)y;
}

// Draws a box around the @x and @y position, that can fit @width and @height characters.
void renderer_box(renderer *renderer, int x, int y, int width, int height)
{
    int front_buffer_idx = renderer->front_buffer_idx;
    size_t count = renderer->command_buffer[front_buffer_idx].count;
    if (count < DRAW_COMMAND_MAX_SIZE)
    {
        renderer->command_buffer[front_buffer_idx].commands[count] = (struct draw_command) {
            .kind = DRAW_COMMAND_KIND_BOX,
            .box.x = x,
            .box.y = y,
            .box.width = width,
            .box.height = height,
        };
        renderer->command_buffer[front_buffer_idx].count++;
    }
}

static void rasterize_cell(terminal_grid *grid, int x, int y, cell cell)
{
    if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
        grid->cells[y][x] = cell;
    }
}

static void rasterize_square(draw_command *command, terminal_grid *grid)
{
    assert(command->kind == DRAW_COMMAND_KIND_SQUARE);
    int x = command->square.x;
    int y = command->square.y;
    ansi_color color = command->square.color;

    rasterize_cell(grid, x, y, (cell) { .bg = color, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {' '} });
    rasterize_cell(grid, x + 1, y, (cell) { .bg = color, .fg = ANSI_COLOR_DEFAULT, .ch.bytes = {' '} });
}

static void rasterize_string(draw_command *command, terminal_grid *grid)
{
    // TODO
    assert(command->kind == DRAW_COMMAND_KIND_STRING);
    (void)command;
    (void)grid;
}

static void rasterize_box(draw_command *command, terminal_grid *grid)
{
    assert(command->kind == DRAW_COMMAND_KIND_BOX);
    int x = command->box.x;
    int y = command->box.y;
    int width = command->box.width;
    int height = command->box.height;

    int left_x = x - 1;
    int right_x = x + width;
    int top_y = y - 1;
    int bot_y = y + height;

    rasterize_cell(grid, left_x, top_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_DOWN_RIGHT });
    rasterize_cell(grid, right_x, top_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_DOWN_LEFT });
    rasterize_cell(grid, left_x, bot_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_UP_RIGHT });
    rasterize_cell(grid, right_x, bot_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_UP_LEFT });

    for (int xx = left_x + 1; xx < right_x; xx++) {
        rasterize_cell(grid, xx, top_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_HORIZONTAL });
        rasterize_cell(grid, xx, bot_y, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_HORIZONTAL });
    }

    for (int yy = top_y + 1; yy < bot_y; yy++) {
        rasterize_cell(grid, left_x, yy, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_VERTICAL });
        rasterize_cell(grid, right_x, yy, (cell) { .bg = ANSI_COLOR_DEFAULT, .fg = ANSI_COLOR_DEFAULT, .ch = BOX_LIGHT_VERTICAL });
    }
}

static void rasterize(draw_command_buffer *buffer, terminal_grid *grid)
{
    for (size_t i = 0; i < buffer->count; i++)
    {
        draw_command *command = &buffer->commands[i];
        switch (command->kind)
        {
        case DRAW_COMMAND_KIND_SQUARE:
            rasterize_square(command, grid);
            break;
        case DRAW_COMMAND_KIND_BOX:
            rasterize_box(command, grid);
            break;
        case DRAW_COMMAND_KIND_STRING:
            rasterize_string(command, grid);
            break;
        default:
            break;
        }
    }
}

size_t renderer_render(renderer *renderer, pty_win_size win_size, char *buffer, size_t buffer_size)
{
    (void)win_size;

    int cursor_x = 0;
    int cursor_y = 0;
    ansi_color current_bg = ANSI_COLOR_DEFAULT;
    ansi_color current_fg = ANSI_COLOR_DEFAULT;
    int front_buffer_idx = renderer->front_buffer_idx;
    int back_buffer_idx = renderer->front_buffer_idx ? 0 : 1;
    buf_writer writer = { .content = buffer, .capacity = buffer_size, .cursor = 0 };

    buf_writer_write(&writer, ANSI_CURSOR_HOME_POSITION, sizeof(ANSI_CURSOR_HOME_POSITION) - 1);

    terminal_grid front_grid = {0};
    terminal_grid back_grid = {0};

    rasterize(&renderer->command_buffer[front_buffer_idx], &front_grid);
    rasterize(&renderer->command_buffer[back_buffer_idx], &back_grid);

    printf("Draw commands: %zu\n", renderer->command_buffer->count);

    // TODO: Fix tearing when screen resizes
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            cell front_cell = front_grid.cells[y][x];
            cell back_cell = back_grid.cells[y][x];

            if (memcmp(&front_cell, &back_cell, sizeof(front_cell)) != 0)
            {
                if (cursor_x != x || cursor_y != y) {
                    buf_writer_cursor_to(&writer, x + 1, y + 1);
                }

                // TODO: Refactor
                unsigned char empty[4] = {0};
                if (memcmp(front_cell.ch.bytes, empty, sizeof(empty)) == 0) {
                    if (current_fg != ANSI_COLOR_DEFAULT && current_bg != ANSI_COLOR_DEFAULT) {
                        buf_writer_color2(&writer, ANSI_FG(ANSI_COLOR_DEFAULT), ANSI_BG(ANSI_COLOR_DEFAULT));
                        current_fg = ANSI_COLOR_DEFAULT;
                        current_bg = ANSI_COLOR_DEFAULT;
                    } else if (current_fg != ANSI_COLOR_DEFAULT) {
                        buf_writer_color1(&writer, ANSI_FG(ANSI_COLOR_DEFAULT));
                        current_fg = ANSI_COLOR_DEFAULT;
                    } else if (current_bg != ANSI_COLOR_DEFAULT) {
                        buf_writer_color1(&writer, ANSI_BG(ANSI_COLOR_DEFAULT));
                        current_bg = ANSI_COLOR_DEFAULT;
                    }

                    buf_writer_write(&writer, " ", 1);
                } else {
                    if (current_fg != front_cell.fg && current_bg != front_cell.bg) {
                        buf_writer_color2(&writer, ANSI_FG(front_cell.fg), ANSI_BG(front_cell.bg));
                        current_fg = front_cell.fg;
                        current_bg = front_cell.bg;
                    } else if (current_bg != front_cell.bg) {
                        buf_writer_color1(&writer, ANSI_BG(front_cell.bg));
                        current_bg = front_cell.bg;
                    } else if (current_fg != front_cell.fg) {
                        buf_writer_color1(&writer, ANSI_FG(front_cell.fg));
                        current_fg = front_cell.fg;
                    }
                    buf_writer_utf8(&writer, front_cell.ch);
                }
                cursor_x = x + 1;
                cursor_y = y;
            }
        }
    }
    buf_writer_write(&writer, ANSI_RESET_COLOR, sizeof(ANSI_RESET_COLOR) - 1);

    // Swap buffer
    memset(&renderer->command_buffer[back_buffer_idx], 0, sizeof(renderer->command_buffer[0]));
    renderer->front_buffer_idx = back_buffer_idx;

    // Cursor did not move, not need to write anything.
    return cursor_x != 0 && cursor_y != 0 ? writer.cursor : 0;
}
