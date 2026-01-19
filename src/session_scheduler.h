#ifndef TETRIS_SSH_SESSION_SCHEDULER_H
#define TETRIS_SSH_SESSION_SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "session_pool.h"

typedef struct session_scheduler_item {
    uint64_t timestamp;
    session_ctx *ctx;
} session_scheduler_item;

// The scheduler is an binary min-heap containing the next session pointer and
// the timestamp of the sessions's next frame. Items are ordered by their timestamp
// Reference: https://en.wikipedia.org/wiki/Binary_heap
typedef struct session_scheduler {
    size_t count;
    session_scheduler_item heap[SESSION_MANAGER_SESSION_COUNT_MAX];
} session_scheduler;


#define HEAP_LEFT_CHILD_IDX(i) (2 * (i) + 1)
#define HEAP_RIGHT_CHILD_IDX(i) (2 * (i) + 2)
#define HEAP_PARENT_IDX(i) (((i) - 1) / 2)

bool session_scheduler_min(session_scheduler *scheduler, session_scheduler_item *item)
{
    if (scheduler->count > 0) {
        *item = scheduler->heap[0];
        return true;
    }

    return false;
}

void session_scheduler_bubble_down(session_scheduler *scheduler, size_t index)
{
    assert(index < scheduler->count);

    size_t left = HEAP_LEFT_CHILD_IDX(index);
    size_t right = HEAP_RIGHT_CHILD_IDX(index);
    size_t smallest = index;

    // Find the largest timestamp between the current index and both children
    if (left < scheduler->count && scheduler->heap[left].timestamp < scheduler->heap[smallest].timestamp) {
        smallest = left;
    }

    if (right < scheduler->count && scheduler->heap[right].timestamp < scheduler->heap[smallest].timestamp) {
        smallest = right;
    }

    // Swap and keep bubbling down if the children is smaller
    if (smallest != index) {
        session_scheduler_item tmp = scheduler->heap[index];
        scheduler->heap[index] = scheduler->heap[smallest];
        scheduler->heap[smallest] = tmp;
        session_scheduler_bubble_down(scheduler, smallest);
    }
}

void session_scheduler_bubble_up(session_scheduler *scheduler, size_t index)
{
    assert(index < scheduler->count);

    // Already at the root, can't bubble up further
    if (index == 0) {
        return;
    }

    size_t parent = HEAP_PARENT_IDX(index);
    size_t largest = index;

    // Find the smaller timestamp between the current index and the parent
    if (scheduler->heap[parent].timestamp > scheduler->heap[largest].timestamp) {
        largest = parent;
    }

    // Swap and keep bubbling up if the parent is larger
    if (largest != index) {
        session_scheduler_item tmp = scheduler->heap[index];
        scheduler->heap[index] = scheduler->heap[largest];
        scheduler->heap[largest] = tmp;
        session_scheduler_bubble_up(scheduler, largest);
    }
}

bool session_scheduler_extract_min(session_scheduler *scheduler, session_scheduler_item *item)
{
    if (scheduler->count == 0) {
        return false;
    }

    *item = scheduler->heap[0];
    // Swap the last item with the root
    scheduler->heap[0] = scheduler->heap[scheduler->count - 1];
    scheduler->count--;

    // Bubble down the root item if it's not the only item in the heap
    if (scheduler->count > 1) {
        session_scheduler_bubble_down(scheduler, 0);
    }

    return true;
}

bool session_scheduler_extract_min_threshold(session_scheduler *scheduler, uint64_t current_timestamp, session_scheduler_item *item)
{
    session_scheduler_item min;
    if (session_scheduler_min(scheduler, &min) && min.timestamp < current_timestamp) {
        return session_scheduler_extract_min(scheduler, item);
    }

    return false;
}

void session_scheduler_insert(session_scheduler *scheduler, session_scheduler_item item)
{
    assert(scheduler->count < SESSION_MANAGER_SESSION_COUNT_MAX - 1);

    size_t next_idx = scheduler->count;
    scheduler->heap[next_idx] = item;
    scheduler->count++;

    // Bubble up the last element if it's not the only item in the heap
    if (scheduler->count > 1) {
        session_scheduler_bubble_up(scheduler, next_idx);
    }
}

#endif // TETRIS_SSH_SESSION_SCHEDULER_H
