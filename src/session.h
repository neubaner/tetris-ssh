#ifndef TETRIS_SSH_SESSION_H
#define TETRIS_SSH_SESSION_H

#include <stdio.h>
#include <libssh/libssh.h>
#include <libssh/callbacks.h>
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
    size_t session_idx;
    struct ssh_channel_callbacks_struct channel_cbs;
    struct ssh_server_callbacks_struct server_cbs;

    const char *username;
    key_press_reader key_reader;
    struct pty_data pty;
    session_status status;
} session_ctx;

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

static int x = 0;
static int y = 0;

void session_ctx_setup_server_callbacks(session_ctx *ctx);
void session_ctx_setup_channel_callbacks(session_ctx *ctx);

void render(ssh_channel channel, int x, int y)
{
    char buffer[1024] = {0};
    char *cursor = buffer;

    memcpy(cursor, ANSI_ERASE_ENTITY_SCREEN, sizeof(ANSI_ERASE_ENTITY_SCREEN) - 1); cursor += sizeof(ANSI_ERASE_ENTITY_SCREEN) - 1;
    memcpy(cursor, ANSI_CURSOR_HOME_POSITION, sizeof(ANSI_CURSOR_HOME_POSITION) - 1); cursor += sizeof(ANSI_CURSOR_HOME_POSITION) - 1;
    memcpy(cursor, ANSI_HIDE_CURSOR, sizeof(ANSI_HIDE_CURSOR) - 1); cursor += sizeof(ANSI_HIDE_CURSOR) - 1;

    for (int i = 0; i < x; i++) {
        // Arrow right
        memcpy(cursor, "\x1b[C", 3);
        cursor += 3;
    }

    for (int i = 0; i < y; i++) {
        // Arrow down
        memcpy(cursor, "\x1b[B", 3);
        cursor += 3;
    }

    const char hello_world[] = "\x1b[1;4;31;44mHello world!\x1b[0mReset?\r\n";
    memcpy(cursor, hello_world, sizeof(hello_world) - 1);
    cursor += sizeof(hello_world) - 1;

    ssh_channel_write(channel, buffer, (uint32_t)(cursor - buffer));
}

int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
    (void)session;
    (void)channel;
    (void)userdata;

    // session_ctx *ctx = (session_ctx *)userdata;

    // render(channel, x, y);

    return SSH_OK;
}

int pty_request(ssh_session session,
                       ssh_channel channel,
                       const char *term,
                       int width, int height,
                       int pxwidth, int pxheight,
                       void *userdata)
{
    (void)session;
    (void)channel;

    session_ctx *ctx = (session_ctx *)userdata;

    ctx->pty = (struct pty_data){
        .win_size.width = width,
        .win_size.height = height,
        .win_size.px_width = pxwidth,
        .win_size.px_height = pxheight,
    };

    printf("term=%s width=%d height=%d, pxwidth=%d, pxheight=%d\n", term, width, height, pxwidth, pxheight);
    return SSH_OK;
}

int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
{
    (void)channel;
    (void)session;
    (void)is_stderr;
    (void)userdata;

    session_ctx *ctx = (session_ctx *)userdata;

    printf("Received %d bytes:", len);
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X", ((unsigned char *)data)[i]);
    }
    printf("\n");

    char *chars = (char *)data;
    uint32_t read;
    for (read = 0; read < len && !ring_buf_is_full(&ctx->key_reader.buf); read++) {
        ring_buf_push(&ctx->key_reader.buf, chars[read]);
    }

    // render(channel, x, y);

    return read;
}

int pty_window_change(ssh_session session,
                             ssh_channel channel,
                             int width, int height,
                             int pxwidth, int pwheight,
                             void *userdata)
{
    (void)session;
    (void)channel;
    session_ctx *ctx = (session_ctx *)userdata;

    ctx->pty.win_size.width = width;
    ctx->pty.win_size.height = height;
    ctx->pty.win_size.px_width = pxwidth;
    ctx->pty.win_size.px_height = pwheight;

    printf("resize: width=%d height=%d, pxwidth=%d, pxheight=%d\n", width, height, pxwidth, pwheight);
    // render(channel, x, y);

    return SSH_OK;
}

ssh_channel channel_open(ssh_session session, void *userdata)
{
    session_ctx *ctx = (session_ctx *)userdata;

    if (ctx->channel != NULL) {
        // A session can only have a single channel
        return NULL;
    }

    ctx->channel = ssh_channel_new(session);
    session_ctx_setup_channel_callbacks(ctx);

    return ctx->channel;
}

int pubkey_auth(ssh_session session, const char *user, struct ssh_key_struct *pubkey, char signature_state, void *userdata)
{
    (void)session;
    (void)pubkey;
    (void)signature_state;

    // TODO: is this really all we need to so for our use case?
    session_ctx *ctx = (session_ctx *)userdata;
    ctx->username = user;

    // Anyone can play :)
    return SSH_AUTH_SUCCESS;
}

void session_ctx_setup_server_callbacks(session_ctx *ctx)
{
    ctx->server_cbs = (struct ssh_server_callbacks_struct){
        .userdata = ctx,
        .channel_open_request_session_function = channel_open,
        .auth_pubkey_function = pubkey_auth,
    };
    ssh_callbacks_init(&ctx->server_cbs);
    ssh_set_server_callbacks(ctx->session, &ctx->server_cbs);
}

void session_ctx_setup_channel_callbacks(session_ctx *ctx)
{
    ctx->channel_cbs = (struct ssh_channel_callbacks_struct){
        .userdata = ctx,
        .channel_pty_request_function = pty_request,
        .channel_shell_request_function = shell_request,
        .channel_data_function = data_function,
        .channel_pty_window_change_function = pty_window_change,
    };
    ssh_callbacks_init(&ctx->channel_cbs);
    ssh_set_channel_callbacks(ctx->channel, &ctx->channel_cbs);
}

#endif // !TETRIS_SSH_SESSION_H
