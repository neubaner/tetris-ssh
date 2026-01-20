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

typedef struct cell {
    ansi_color fg;
    ansi_color bg;
    utf8_char ch;
} cell;

#define GRID_HEIGHT (60 * 1)
#define GRID_WIDTH (60 * 2)

typedef struct grid {
    cell cells[GRID_HEIGHT][GRID_WIDTH];
} grid;


size_t renderer_clear(char *buffer, size_t buffer_size);

void grid_set_piece(grid *grid, int x, int y, cell cell);

void grid_clear(grid *grid);

void grid_square(grid *grid, int x, int y, ansi_color color);

void grid_string(grid *grid, const char *str, int x, int y);

// Draws a box around the @x and @y position, that can fit @width and @height characters.
void grid_box(grid *grid, int x, int y, int width, int height);

size_t grid_render(grid *grid, char *buffer, size_t buffer_size);

#endif // TETRIS_SSH_RENDERER_H
