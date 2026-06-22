#ifndef TETRIS_SSH_RING_BUF_H
#define TETRIS_SSH_RING_BUF_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include "ring_buf8.h"

// TODO: Make ring_buf accept any power of 2 capacity
#define RING_BUF_CAPACITY_BITS 6
#define RING_BUF_CAPACITY (1 << (RING_BUF_CAPACITY_BITS))

typedef struct ring_buf {
    ring_buf8 ring;
    // Contents of the buffer
    char buffer[RING_BUF_CAPACITY];
} ring_buf;

static inline uint8_t ring_buf_size(const ring_buf *buf)
{
    return ring_buf8_size(&buf->ring);
}

static inline bool ring_buf_is_empty(const ring_buf *buf)
{
    return ring_buf8_is_empty(&buf->ring);
}

static inline bool ring_buf_is_full(const ring_buf *buf)
{
    return ring_buf8_is_full(&buf->ring, RING_BUF_CAPACITY_BITS);
}

static inline char ring_buf_peek(const ring_buf *buf)
{
    ptrdiff_t idx = ring_buf8_peek_read(&buf->ring, RING_BUF_CAPACITY_BITS);
    assert(idx >= 0);

    return buf->buffer[idx];
}

static inline char ring_buf_shift(ring_buf *buf)
{
    char value = ring_buf_peek(buf);
    assert(ring_buf8_commit_read(&buf->ring));

    return value;
}

static inline void ring_buf_push(ring_buf *buf, char value)
{
    ptrdiff_t idx = ring_buf8_peek_write(&buf->ring, RING_BUF_CAPACITY_BITS);
    assert(idx >= 0);

    buf->buffer[idx] = value;
    assert(ring_buf8_commit_write(&buf->ring, RING_BUF_CAPACITY_BITS));
}

#endif // TETRIS_SSH_RING_BUF_H
