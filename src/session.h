#ifndef TETRIS_SSH_SESSION_H
#define TETRIS_SSH_SESSION_H

#include <stdint.h>
#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include "game.h"
#include "input.h"

typedef struct pty_win_size {
    int width;
    int height;
    int px_width;
    int px_height;
} pty_win_size;

typedef struct pty_data {
    pty_win_size win_size;
} pty_data;

typedef enum session_status {
    SESSION_STATUS_UNKNOWN,
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
    key_press_reader key_reader;
    struct pty_data pty;
    session_status status;
    uint64_t last_frame_ns;
    grid grid;
    game_state game_state;
} session_ctx;

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

void session_ctx_setup_server_callbacks(session_ctx *ctx);
void session_ctx_setup_channel_callbacks(session_ctx *ctx);

int shell_request(ssh_session session, ssh_channel channel, void *userdata);
int pty_request(ssh_session session,
                       ssh_channel channel,
                       const char *term,
                       int width, int height,
                       int pxwidth, int pxheight,
                       void *userdata);

int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata);

int pty_window_change(ssh_session session,
                             ssh_channel channel,
                             int width, int height,
                             int pxwidth, int pwheight,
                             void *userdata);

ssh_channel channel_open(ssh_session session, void *userdata);

int pubkey_auth(ssh_session session, const char *user, struct ssh_key_struct *pubkey, char signature_state, void *userdata);

#endif // TETRIS_SSH_SESSION_H
