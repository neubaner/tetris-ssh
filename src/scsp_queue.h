#ifndef TETRIS_SSH_SPSC_QUEUE_H
#define TETRIS_SSH_SPSC_QUEUE_H

#include <stdatomic.h>
#include <stdbool.h>

#define SESSION_QUEUE_CAPACITY 16
#define SESSION_QUEUE_MASK(v) ((v) % SESSION_QUEUE_CAPACITY)

typedef struct spsc_queue {
    atomic_size_t head;
    atomic_size_t tail;
    void *ring[SESSION_QUEUE_CAPACITY];
} spsc_queue;

static bool spsc_queue__is_full(size_t head, size_t tail) {
    return tail == head;
}

bool spsc_queue_push(spsc_queue *queue, void *item)
{
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t next_tail = tail + 1;
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);

    if (spsc_queue__is_full(head, tail)) {
        return false;
    }

    queue->ring[SESSION_QUEUE_MASK(tail)] = item;
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release);

    return true;
}

bool spsc_queue_shift(spsc_queue *queue, void **item)
{
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t next_head = head + 1;
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

    if (spsc_queue__is_full(head, tail)) {
        return false;
    }

    *item = queue->ring[SESSION_QUEUE_MASK(head)];
    atomic_store_explicit(&queue->head, next_head, memory_order_release);

    return true;
}

#endif // TETRIS_SSH_SPSC_QUEUE_H
