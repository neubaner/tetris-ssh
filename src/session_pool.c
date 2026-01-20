#include "session_pool.h"

void session_pool_init(session_pool *pool)
{
    for (size_t i = 0; i < SESSION_MANAGER_SESSION_COUNT_MAX - 1; i++) {
        pool->slots[i].next_free = &pool->slots[i + 1];
    }

    pool->slots[SESSION_MANAGER_SESSION_COUNT_MAX - 1].next_free = NULL;
    pool->next_free_slot = &pool->slots[0];
    pool->sessions_count = 0;
}

session_ctx *session_pool_add(session_pool *pool, ssh_session session)
{
    session_slot *slot = pool->next_free_slot;

    if (slot == NULL) {
        return NULL;
    }

    session_slot *next_free_slot = slot->next_free;
    slot->session = (session_ctx) {
        .session = session,
        .status = SESSION_STATUS_ALIVE,
    };
    session_ctx_setup_server_callbacks(&slot->session);
    game_init(&slot->session.game_state);

    pool->next_free_slot = next_free_slot;
    pool->sessions_count++;

    return &slot->session;
}

void session_pool_remove(session_pool *pool, session_ctx *ctx)
{
    session_slot *slot = (session_slot *)ctx;
    assert(slot >= pool->slots && slot <= &pool->slots[SESSION_MANAGER_SESSION_COUNT_MAX - 1]);

    slot->next_free = pool->next_free_slot;
    pool->next_free_slot = slot;
    pool->sessions_count--;
}

