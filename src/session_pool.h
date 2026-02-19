#ifndef TETRIS_SSH_SESSION_MANAGER_H
#define TETRIS_SSH_SESSION_MANAGER_H

#include <assert.h>
#include <stddef.h>
#include <unistd.h>
#include "session.h"

#define SESSION_MANAGER_SESSION_COUNT_MAX 1024

typedef union session_slot {
    session_ctx session;
    // A pointer to the next free slot. If it points to NULL, no free slot is available
    union session_slot *next_free;
    static_assert(sizeof(session_ctx) >= sizeof(session_ctx *), "session_ctx must be at least as large as a pointer to it.");
} session_slot;

typedef struct session_pool {
    // TODO: This should be a dynamically sized array
    session_slot slots[SESSION_MANAGER_SESSION_COUNT_MAX];
    session_slot *next_free_slot;
    size_t sessions_count;
} session_pool;

void session_pool_init(session_pool *pool);

session_ctx *session_pool_alloc(session_pool *pool);

void session_pool_free(session_pool *pool, session_ctx *ctx);


#endif // TETRIS_SSH_SESSION_MANAGER_H
