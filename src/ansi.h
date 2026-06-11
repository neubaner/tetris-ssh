#ifndef TETRIS_SSH_ANSI_H
#define TETRIS_SSH_ANSI_H

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

#define ANSI_BRIGHT_MOD 60
#define ANSI_COLOR_ENUM(name, value) ANSI_COLOR_##name = value, ANSI_COLOR_BRIGHT_##name = value + ANSI_BRIGHT_MOD,

typedef enum ansi_color {
    ANSI_COLOR_X(ANSI_COLOR_ENUM)
} ansi_color;

#define ANSI_FG(color) ((color) + 30)
#define ANSI_BG(color) ((color) + 40)

#endif // TETRIS_SSH_ANSI_H
