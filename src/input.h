#ifndef TETRIS_SSH_INPUT_H
#define TETRIS_SSH_INPUT_H

#include <assert.h>
#include <stdbool.h>
#include "ring_buf.h"

#define KEY_KIND_X_MACRO(X) \
    X(KEY_KIND_NO_KEY, "(no key)") \
    X(KEY_KIND_CHAR, "char") \
    X(KEY_KIND_CTRL, "ctrl") \
    X(KEY_KIND_ARROW_UP, "up") \
    X(KEY_KIND_ARROW_DOWN, "down") \
    X(KEY_KIND_ARROW_LEFT, "left") \
    X(KEY_KIND_ARROW_RIGHT, "right") \
    X(KEY_KIND_ENTER, "enter")

typedef enum key_kind {
#define KEY_KIND_ENUM(code, str) code,
    KEY_KIND_X_MACRO(KEY_KIND_ENUM)
    KEY_KIND_COUNT,
#undef KEY_KIND_ENUM
} key_kind;


typedef struct key_press {
    key_kind kind;
    char ch;
} key_press;

typedef struct key_press_result {
    key_press press;
    bool needs_more_input;
} key_press_result ;

typedef enum key_press_reader_state {
    KEY_PRESS_READER_STATE_INITIAL,
    KEY_PRESS_READER_STATE_ESCAPE,
    KEY_PRESS_READER_STATE_CSI,
} key_press_reader_state;

typedef struct key_press_reader {
    ring_buf buf;
    key_press_reader_state state;
} key_press_reader;

const char *key_kind_str(key_kind kind);

key_press_result key_press_read(key_press_reader *reader);

#endif // TETRIS_SSH_INPUT_H
