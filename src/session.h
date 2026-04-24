#ifndef TETRIS_SSH_SESSION_H
#define TETRIS_SSH_SESSION_H

#include <stdint.h>
#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include "game.h"
#include "input.h"

typedef struct pty_data {
    pty_win_size win_size;
} pty_data;

typedef enum session_status {
    SESSION_STATUS_KEY_EXCHANGE_PENDING,
    SESSION_STATUS_CHANNEL_PENDING,
    SESSION_STATUS_CHANNEL_OPEN,
    SESSION_STATUS_ALIVE,
    SESSION_STATUS_CLOSING,
    SESSION_STATUS_DEAD,
} session_status;

typedef struct session_ctx {
    ssh_session session;
    ssh_channel channel;
    struct ssh_channel_callbacks_struct channel_cbs;
    struct ssh_server_callbacks_struct server_cbs;

    const char *username;
    uint64_t last_frame_ns;
    uint64_t timeout_dealine_ns;
    key_press_reader key_reader;
    struct pty_data pty;
    session_status status;
    renderer renderer;
    game_state game_state;
} session_ctx;

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

void session_ctx_setup_server_callbacks(session_ctx *ctx);

#endif // TETRIS_SSH_SESSION_H
