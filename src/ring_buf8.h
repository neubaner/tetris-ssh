#ifndef TETRIS_SSH_RING_BUF8_H
#define TETRIS_SSH_RING_BUF8_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define RING_BUF8_CAPACITY(bits) (1 << (bits))
#define RING_BUF8_MASK(bits) ((1 << (bits)) - 1)

// A ring buffer using unmasked cursors that wrap on uint8_t overflow.
//
// This reclaims a slot that a masking cursors waster, but the capacity
// is capped to 2^7 to be able to differentiate between empty and full
// buffers.
typedef struct ring_buf8 {
    // Reader cursor of the buffer
    uint8_t head;
    // Writer cursor of the buffer
    uint8_t tail;
} ring_buf8;

static inline uint8_t ring_buf8_size(const ring_buf8 *buf)
{
    return buf->tail - buf->head;
}

static inline bool ring_buf8_is_empty(const ring_buf8 *buf)
{
    return buf->head == buf->tail;
}

static inline bool ring_buf8_is_full(const ring_buf8 *buf, int cap_bits)
{
    assert(cap_bits >= 0 && cap_bits <= 7);
    return ring_buf8_size(buf) >= RING_BUF8_CAPACITY(cap_bits);
}

static inline ptrdiff_t ring_buf8_peek_read(const ring_buf8 *buf, int cap_bits)
{
    assert(cap_bits >= 0 && cap_bits <= 7);
    if (ring_buf8_is_empty(buf)) { return -1; }

    return buf->head & RING_BUF8_MASK(cap_bits);
}

static inline bool ring_buf8_commit_read(ring_buf8 *buf)
{
    if (ring_buf8_is_empty(buf)) { return false; }

    buf->head++;
    return true;
}

static inline ptrdiff_t ring_buf8_peek_write(const ring_buf8 *buf, int cap_bits)
{
    assert(cap_bits >= 0 && cap_bits <= 7);
    if (ring_buf8_is_full(buf, cap_bits)) { return -1; }

    return buf->tail & RING_BUF8_MASK(cap_bits);
}

static inline bool ring_buf8_commit_write(ring_buf8 *buf, int cap_bits)
{
    assert(cap_bits >= 0 && cap_bits <= 7);
    if (ring_buf8_is_full(buf, cap_bits)) { return false; }

    buf->tail++;
    return true;
}

#endif // TETRIS_SSH_RING_BUF8_H
