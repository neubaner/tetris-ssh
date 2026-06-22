#ifndef TETRIS_SSH_SPSC_QUEUE_H
#define TETRIS_SSH_SPSC_QUEUE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdalign.h>

// TODO: The capacity of the queue should not be fixed

#define SESSION_QUEUE_CAPACITY 16
#define CACHE_LINE_SIZE 64

// A single producer single consumer queue. There must be only one consumer thread and one producer thread.
// The capacity of queue is limited by SESSION_QUEUE_CAPACITY.
typedef struct spsc_queue {
    struct {
        alignas(CACHE_LINE_SIZE) atomic_size_t head;
        size_t cached_tail;
        alignas(CACHE_LINE_SIZE) atomic_size_t tail;
        size_t cached_head;
    };
    void *ring[SESSION_QUEUE_CAPACITY];
} spsc_queue;

// Pushes an item to the queue. Returns true if the operation was successful or false if the queue is full.
bool spsc_queue_push(spsc_queue *queue, void *item);

// Removes the first item in the queue and set it to value pointed by `item`. Returns true if the operation
// was successful or false if the queue was empty.
bool spsc_queue_shift(spsc_queue *queue, void **item);

#endif // TETRIS_SSH_SPSC_QUEUE_H
