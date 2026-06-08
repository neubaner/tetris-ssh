#ifndef TETRIS_SSH_RENDERER_H
#define TETRIS_SSH_RENDERER_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
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

#define ANSI_FG(color) ((color) + 30)
#define ANSI_BG(color) ((color) + 40)

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
            ansi_color color;
        } square;
        struct {
            // TODO
            char _pad[1];
        } string;
    };
} draw_command;

// TODO: This cell wastes too much space
typedef struct cell {
    ansi_color fg;
    ansi_color bg;
    utf8_char ch;
} cell;

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

void renderer_square(renderer *renderer, int x, int y, ansi_color color);

void renderer_string(renderer *renderer, const char *str, int x, int y);

// Draws a box around the @x and @y position, that can fit @width and @height characters.
void renderer_box(renderer *renderer, int x, int y, int width, int height);

size_t renderer_render(renderer *renderer, pty_win_size win_size, char *buffer, size_t buffer_size);

#endif // TETRIS_SSH_RENDERER_H
