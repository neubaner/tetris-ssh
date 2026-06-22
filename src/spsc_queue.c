#include <stddef.h>

#include "spsc_queue.h"

#define SESSION_QUEUE_MASK(v) ((v) % SESSION_QUEUE_CAPACITY)

static bool spsc_queue__is_full(size_t head, size_t tail) {
    return (tail - head) >= SESSION_QUEUE_CAPACITY;
}

static bool spsc_queue__is_empty(size_t head, size_t tail) {
    return tail == head;
}

bool spsc_queue_push(spsc_queue *queue, void *item)
{
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t next_tail = tail + 1;
    size_t head = queue->cached_head;

    if (spsc_queue__is_full(head, tail)) {
        head = atomic_load_explicit(&queue->head, memory_order_acquire);
        queue->cached_head = head;
        if (spsc_queue__is_full(head, tail)) {
            return false;
        }
    }

    queue->ring[SESSION_QUEUE_MASK(tail)] = item;
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release);

    return true;
}

bool spsc_queue_shift(spsc_queue *queue, void **item)
{
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t next_head = head + 1;
    size_t tail = queue->cached_tail;

    if (spsc_queue__is_empty(head, tail)) {
        tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
        queue->cached_tail = tail;
        if (spsc_queue__is_empty(head, tail)) {
            return false;
        }
    }

    *item = queue->ring[SESSION_QUEUE_MASK(head)];
    atomic_store_explicit(&queue->head, next_head, memory_order_release);

    return true;
}
