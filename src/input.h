#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define RING_BUF_CAPACITY 64
#define RING_BUF_MASK(v) ((v) % RING_BUF_CAPACITY == 0)

typedef struct ring_buf {
    uint32_t read;
    uint32_t write;
    char buffer[RING_BUF_CAPACITY];
} ring_buf;

uint32_t ring_buf_size(const ring_buf *buf)
{
    return buf->write - buf->read;
}

bool ring_buf_is_empty(const ring_buf *buf)
{
    return buf->read == buf->write;
}

bool ring_buf_is_full(const ring_buf *buf)
{
    return (buf->write - buf->read) >= RING_BUF_CAPACITY;
}

char ring_buf_shift(ring_buf *buf)
{
    assert(!ring_buf_is_empty(buf));

    char value = buf->buffer[RING_BUF_MASK(buf->read)];
    buf->read++;

    return value;
}

char ring_buf_peak(const ring_buf *buf)
{
    assert(!ring_buf_is_empty(buf));

    return buf->buffer[RING_BUF_MASK(buf->read)];
}

void ring_buf_push(ring_buf *buf, char value)
{
    assert(!ring_buf_is_full(buf));

    buf->buffer[RING_BUF_MASK(buf->write)] = value;
    buf->write++;
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

key_press key_press_read(struct ring_buf *buf)
{
    if (ring_buf_is_empty(buf)) {
        return (key_press){};
    }

    char top = ring_buf_shift(buf);

    if (top >= 'A' && top <= 'Z') {
        return (key_press) {
            .sym =  top - 'A' + KEY_SYM_A,
            .mods = KEY_MOD_SHIFT,
        };
    } else if (top >= 'a' && top <= 'z') {
        return (key_press) {
            .sym =  top - 'a' + KEY_SYM_A,
        };
    } else if (top >= ('A' & 0x1f) && top <= ('Z' & 0x1f)) {
        return (key_press) {
            .sym =  (top & 0x1f) + KEY_SYM_A - 1,
            .mods = KEY_MOD_CTRL,
        };
    } else {
        return (key_press){};
    }
}
