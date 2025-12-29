#ifndef TETRIS_SSH_INPUT_H
#define TETRIS_SSH_INPUT_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#define RING_BUF_CAPACITY 64
#define RING_BUF_MASK(v) ((v) % RING_BUF_CAPACITY)

typedef struct ring_buf {
    // Reader cursor of the buffer
    uint8_t head;
    // Writer cursor of the buffer
    uint8_t tail;
    // Contents of the buffer
    char buffer[RING_BUF_CAPACITY];
} ring_buf;

uint32_t ring_buf_size(const ring_buf *buf)
{
    return buf->tail - buf->head;
}

bool ring_buf_is_empty(const ring_buf *buf)
{
    return buf->head == buf->tail;
}

bool ring_buf_is_full(const ring_buf *buf)
{
    return (buf->tail - buf->head) >= RING_BUF_CAPACITY;
}

char ring_buf_shift(ring_buf *buf)
{
    assert(!ring_buf_is_empty(buf));

    char value = buf->buffer[RING_BUF_MASK(buf->head)];
    buf->head++;

    return value;
}

char ring_buf_peak(const ring_buf *buf)
{
    assert(!ring_buf_is_empty(buf));

    return buf->buffer[RING_BUF_MASK(buf->head)];
}

void ring_buf_push(ring_buf *buf, char value)
{
    assert(!ring_buf_is_full(buf));

    buf->buffer[RING_BUF_MASK(buf->tail)] = value;
    buf->tail++;
}

#define KEY_KIND_X_MACRO(X) \
    X(KEY_KIND_NO_KEY, "(no key)") \
    X(KEY_KIND_CHAR, "char") \
    X(KEY_KIND_CTRL, "ctrl") \
    X(KEY_KIND_ARROW_UP, "up") \
    X(KEY_KIND_ARROW_DOWN, "down") \
    X(KEY_KIND_ARROW_LEFT, "left") \
    X(KEY_KIND_ARROW_RIGHT, "right") \
    X(KEY_KIND_ENTER, "enter")

#define KEY_KIND_ENUM(code, str) code,
#define KEY_KIND_TABLE(code, str) [code] = str,

typedef enum key_kind {
    KEY_KIND_X_MACRO(KEY_KIND_ENUM)
    KEY_KIND_COUNT,
} key_kind;

const char *keys_kind_str_table[] = {
    KEY_KIND_X_MACRO(KEY_KIND_TABLE)
};

const char *key_kind_str(key_kind kind) {
    assert(kind >= KEY_KIND_NO_KEY && kind < KEY_KIND_COUNT);
    return keys_kind_str_table[kind];
}

#undef KEY_KIND_TABLE
#undef KEY_KIND_ENUM
#undef KEY_KIND_X_MACRO

typedef struct key_press {
    key_kind kind;
    char ch;
} key_press;

typedef struct key_press_result {
    key_press press;
    bool needs_more_input;
} key_press_result ;

typedef enum key_press_state {
    KEY_PRESS_STATE_INITIAL,
    KEY_PRESS_STATE_ESCAPE,
    KEY_PRESS_STATE_CSI,
} key_press_state;

typedef struct key_press_reader {
    ring_buf buf;
    key_press_state state;
} key_press_reader;

// Reference: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
key_press_result key_press_read(key_press_reader *reader)
{
    while(true) {
        if (ring_buf_is_empty(&reader->buf)) {
            return (key_press_result){ .needs_more_input = true, };
        }

        char top = ring_buf_shift(&reader->buf);

        switch(reader->state) {
        case KEY_PRESS_STATE_INITIAL: {
            switch(top) {
            case '\x1b':
                reader->state = KEY_PRESS_STATE_ESCAPE;
                break;
            case '\x9b':
                reader->state = KEY_PRESS_STATE_CSI;
                break;
            case '\r':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ENTER,
                };
            default:
                if (top >= ('A' & 0x1f) && top <= ('Z' & 0x1f)) {
                    return (key_press_result) {
                        .press.ch = top + 'A' - 1,
                        .press.kind = KEY_KIND_CTRL,
                    };
                } else if (isprint(top)) {
                    return (key_press_result) {
                        .press.ch = top,
                        .press.kind = KEY_KIND_CHAR,
                    };
                } else {
                    // XTerm resets their state machine to the initial state when finding unexcepted charaters.
                    reader->state = KEY_PRESS_STATE_INITIAL;
                }
            }
        } break;
        case KEY_PRESS_STATE_ESCAPE: {
            if (top == '[') {
                reader->state = KEY_PRESS_STATE_CSI;
            } else {
                // TODO: Should we handle other escape sequences?
                reader->state = KEY_PRESS_STATE_INITIAL;
            }
        } break;
        case KEY_PRESS_STATE_CSI: {
            reader->state = KEY_PRESS_STATE_INITIAL;

            // TODO: Cursor movement might have an parameter
            switch (top) {
            case 'A':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_UP,
                };
            case 'B':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_DOWN,
                };
            case 'C':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_RIGHT,
                };
            case 'D':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_LEFT,
                };
            }
        }
        break;
        }
    }
}

#endif // TETRIS_SSH_INPUT_H
