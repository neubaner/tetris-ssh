#ifndef TETRIS_SSH_RING_BUF_H
#define TETRIS_SSH_RING_BUF_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// TODO: Make ring_buf accept any power of 2 capacity
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

inline uint8_t ring_buf_size(const ring_buf *buf)
{
    return buf->tail - buf->head;
}

inline bool ring_buf_is_empty(const ring_buf *buf)
{
    return buf->head == buf->tail;
}

inline bool ring_buf_is_full(const ring_buf *buf)
{
    return (buf->tail - buf->head) >= RING_BUF_CAPACITY;
}

inline char ring_buf_shift(ring_buf *buf)
{
    assert(!ring_buf_is_empty(buf));

    char value = buf->buffer[RING_BUF_MASK(buf->head)];
    buf->head++;

    return value;
}

inline char ring_buf_peek(const ring_buf *buf)
{
    assert(!ring_buf_is_empty(buf));

    return buf->buffer[RING_BUF_MASK(buf->head)];
}

inline void ring_buf_push(ring_buf *buf, char value)
{
    assert(!ring_buf_is_full(buf));

    buf->buffer[RING_BUF_MASK(buf->tail)] = value;
    buf->tail++;
}

#endif // TETRIS_SSH_RING_BUF_H
