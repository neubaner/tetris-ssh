#ifndef TETRIS_SSH_SESSION_MANAGER_H
#define TETRIS_SSH_SESSION_MANAGER_H

#include <stddef.h>
#include "game.h"
#include "session.h"

#define SESSION_MANAGER_SESSION_COUNT_MAX 1024

typedef struct session_manager {
    size_t sessions_count;
    // TODO: This should be a dynamically sized array, probably
    session_ctx sessions[SESSION_MANAGER_SESSION_COUNT_MAX];
} session_manager;

session_ctx *session_manager_add(session_manager *manager, ssh_session session)
{
    size_t next_idx = manager->sessions_count;
    if (next_idx >= SESSION_MANAGER_SESSION_COUNT_MAX) {
        return NULL;
    }

    session_ctx *ctx = &manager->sessions[next_idx];
    *ctx = (session_ctx){
        .session = session,
        .session_idx = next_idx,
        .status = SESSION_STATUS_ALIVE,
    };
    session_ctx_setup_server_callbacks(ctx);
    game_init(&ctx->game_state);

    manager->sessions_count++;
    return ctx;
}

void session_manager_remove(session_manager *manager, size_t removed_idx)
{
    assert(removed_idx < manager->sessions_count);

    if (manager->sessions_count > 0) {
        size_t last_idx = manager->sessions_count - 1;

        // Swap the last session with the removed one
        session_ctx *removed_slot = &manager->sessions[removed_idx];
        *removed_slot = manager->sessions[last_idx];

        // Reset the callbacks to point to the new location
        removed_slot->session_idx = removed_idx;
        session_ctx_setup_server_callbacks(removed_slot);

        if (removed_slot->channel != NULL) {
            session_ctx_setup_channel_callbacks(removed_slot);
        }
    }

    manager->sessions_count--;
}


#endif // TETRIS_SSH_SESSION_MANAGER_H
