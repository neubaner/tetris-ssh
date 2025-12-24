#ifndef TETRIS_SSH_INPUT_H
#define TETRIS_SSH_INPUT_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define RING_BUF_CAPACITY 64
#define RING_BUF_MASK(v) ((v) % RING_BUF_CAPACITY)

typedef struct ring_buf {
    // Reader cursor of the buffer
    uint32_t head;
    // Writer cursor of the buffer
    uint32_t tail;
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

    char value = buf->buffer[RING_BUF_MASK(buf->head++)];
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

#define KEY_SYM_X_MACRO(X) \
    X(KEY_SYM_NO_KEY, "(no key)") \
    X(KEY_SYM_ARROW_UP, "up") \
    X(KEY_SYM_ARROW_DOWN, "down") \
    X(KEY_SYM_ARROW_LEFT, "left") \
    X(KEY_SYM_ARROW_RIGHT, "right") \
    X(KEY_SYM_ENTER, "enter") \
    X(KEY_SYM_A, "A") \
    X(KEY_SYM_B, "B") \
    X(KEY_SYM_C, "C") \
    X(KEY_SYM_D, "D") \
    X(KEY_SYM_E, "E") \
    X(KEY_SYM_F, "F") \
    X(KEY_SYM_G, "G") \
    X(KEY_SYM_H, "H") \
    X(KEY_SYM_I, "I") \
    X(KEY_SYM_J, "J") \
    X(KEY_SYM_K, "K") \
    X(KEY_SYM_L, "L") \
    X(KEY_SYM_M, "M") \
    X(KEY_SYM_N, "N") \
    X(KEY_SYM_O, "O") \
    X(KEY_SYM_P, "P") \
    X(KEY_SYM_Q, "Q") \
    X(KEY_SYM_R, "R") \
    X(KEY_SYM_S, "S") \
    X(KEY_SYM_T, "T") \
    X(KEY_SYM_U, "U") \
    X(KEY_SYM_V, "V") \
    X(KEY_SYM_W, "W") \
    X(KEY_SYM_X, "X") \
    X(KEY_SYM_Y, "Y") \
    X(KEY_SYM_Z, "Z")

#define KEY_SYM_ENUM(code, str) code,
#define KEY_SYM_TABLE(code, str) [code] = str,

typedef enum {
    KEY_SYM_X_MACRO(KEY_SYM_ENUM)
    KEY_SYM_COUNT,
} key_sym;

const char *keys_table[] = {
    KEY_SYM_X_MACRO(KEY_SYM_TABLE)
};

#undef KEY_SYM_TABLE
#undef KEY_SYM_ENUM
#undef KEY_SYM_X_MACRO

typedef enum {
    KEY_MOD_NONE = 0,
    KEY_MOD_CTRL = 1 << 0,
    KEY_MOD_SHIFT = 1 << 1,
    KEY_MOD_ALT = 1 << 2,
} key_mods;

typedef struct key_press {
    key_sym sym;
    key_mods mods;
} key_press;

const char *key_sym_str(key_sym sym)
{
    assert(sym >= KEY_SYM_NO_KEY && sym < KEY_SYM_COUNT);
    return keys_table[sym];
}

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
                    .press.sym = KEY_SYM_ENTER,
                };
            default:
                if (top >= 'A' && top <= 'Z') {
                    return (key_press_result) {
                        .press.sym =  top - 'A' + KEY_SYM_A,
                        .press.mods = KEY_MOD_SHIFT,
                    };
                } else if (top >= 'a' && top <= 'z') {
                    return (key_press_result) {
                        .press.sym =  top - 'a' + KEY_SYM_A,
                    };
                } else if (top >= ('A' & 0x1f) && top <= ('Z' & 0x1f)) {
                    return (key_press_result) {
                        .press.sym =  (top & 0x1f) + KEY_SYM_A - 1,
                        .press.mods = KEY_MOD_CTRL,
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
                    .press.sym = KEY_SYM_ARROW_UP,
                };
            case 'B':
                return (key_press_result) {
                    .press.sym = KEY_SYM_ARROW_DOWN,
                };
            case 'C':
                return (key_press_result) {
                    .press.sym = KEY_SYM_ARROW_RIGHT,
                };
            case 'D':
                return (key_press_result) {
                    .press.sym = KEY_SYM_ARROW_LEFT,
                };
            }
        }
        }
    }
}

#endif // TETRIS_SSH_INPUT_H
