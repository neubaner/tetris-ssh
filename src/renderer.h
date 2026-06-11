#ifndef TETRIS_SSH_RENDERER_H
#define TETRIS_SSH_RENDERER_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unicode.h"

typedef enum cell_flags {
    CELL_FLAGS_NONE = 0,
    CELL_FLAGS_BG_BRIGHT = 1 << 0,
    CELL_FLAGS_FG_BRIGHT = 1 << 1,
    CELL_FLAGS_BOLD = 1 << 2,
    CELL_FLAGS_ITALIC = 1 << 3,
    CELL_FLAGS_UNDERLINE = 1 << 4,
    CELL_FLAGS_STRIKETHROUGH = 1 << 5,
} cell_flags;

typedef enum color {
    COLOR_BLACK = 0,
    COLOR_RED = 1,
    COLOR_GREEN = 2,
    COLOR_YELLOW = 3,
    COLOR_BLUE = 4,
    COLOR_MAGENTA = 5,
    COLOR_CYAN = 6,
    COLOR_WHITE = 7,
    COLOR_DEFAULT = 9,
} color;

#define CELL_STYLE_BG_COLOR(style) (((style).data) & 0xF)
#define CELL_STYLE_FG_COLOR(style) ((((style).data) >> 4) & 0xF)
#define CELL_STYLE_FLAGS(style) ((((style).data) >> 8) & 0xFF)
#define CELL_STYLE_PACK(bg, fg, flags) \
    ((cell_style) { \
        .data = (((flags) & 0xFF) << 8) | (((fg) & 0xF) << 4) | ((bg) & 0xF) \
    })

typedef struct cell_style {
    //  |  8 bits (flags) | 4 bits (fg) | 4 bits (bg) |
    // 15 --------------------------------------------0
    uint16_t data;
} cell_style;

// TODO: This cell wastes too much space
typedef struct cell {
    cell_style style;
    utf8_char ch;
} cell;

typedef struct pty_win_size {
    int width;
    int height;
    int px_width;
    int px_height;
} pty_win_size;

typedef enum draw_command_kind {
    DRAW_COMMAND_KIND_NONE,
    DRAW_COMMAND_KIND_SQUARE,
    DRAW_COMMAND_KIND_BOX,
    DRAW_COMMAND_KIND_STRING,
} draw_command_kind;

typedef struct draw_command {
    char kind;
    union {
        struct {
            char x, y, width, height;
        } box;
        struct {
            char x, y;
            cell_style style;
        } square;
        struct {
            // TODO
            char _pad[1];
        } string;
    };
} draw_command;

#define GRID_HEIGHT (60 * 1)
#define GRID_WIDTH (60 * 2)
#define DRAW_COMMAND_MAX_SIZE 1024

typedef struct draw_command_buffer
{
    // TODO: The command buffer size needs to be dynamic
    draw_command commands[DRAW_COMMAND_MAX_SIZE];
    size_t count;
} draw_command_buffer;

typedef struct renderer {
    draw_command_buffer command_buffer[2];
    pty_win_size prev_win_size;
    int front_buffer_idx;
} renderer;


size_t renderer_enter(char *buffer, size_t buffer_size);
size_t renderer_exit(char *buffer, size_t buffer_size);

void renderer_square(renderer *renderer, int x, int y, cell_style style);

void renderer_string(renderer *renderer, const char *str, int x, int y);

// Draws a box around the @x and @y position, that can fit @width and @height characters.
void renderer_box(renderer *renderer, int x, int y, int width, int height);

size_t renderer_render(renderer *renderer, pty_win_size win_size, char *buffer, size_t buffer_size);

#endif // TETRIS_SSH_RENDERER_H
