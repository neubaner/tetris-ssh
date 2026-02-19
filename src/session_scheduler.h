#ifndef TETRIS_SSH_SESSION_SCHEDULER_H
#define TETRIS_SSH_SESSION_SCHEDULER_H

#include <stdint.h>
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

bool session_scheduler_min(session_scheduler *scheduler, session_scheduler_item *item);

bool session_scheduler_extract_min(session_scheduler *scheduler, session_scheduler_item *item);

bool session_scheduler_extract_min_threshold(session_scheduler *scheduler, uint64_t current_timestamp, session_scheduler_item *item);

void session_scheduler_insert(session_scheduler *scheduler, session_scheduler_item item);

#endif // TETRIS_SSH_SESSION_SCHEDULER_H
