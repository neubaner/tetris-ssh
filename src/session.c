#include <stdio.h>
#include <libssh/server.h>

#include "session.h"

static int on_shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
    (void)session;
    (void)channel;
    (void)userdata;

    return SSH_OK;
}

static int on_pty_request(ssh_session session,
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

static int on_data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
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

    return read;
}

static int on_pty_window_change(ssh_session session,
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

    return SSH_OK;
}

static void on_channel_closed(ssh_session session, ssh_channel channel, void *userdata)
{
    (void)session;
    (void)channel;

    session_ctx *ctx = (session_ctx *)userdata;
    fprintf(stderr, "Channel closed\n");
    ctx->status = SESSION_STATUS_CLOSING;
}

static ssh_channel on_channel_open(ssh_session session, void *userdata)
{
    session_ctx *ctx = (session_ctx *)userdata;

    if (ctx->channel != NULL) {
        // A session can only have a single channel
        return NULL;
    }

    ctx->status = SESSION_STATUS_CHANNEL_OPEN;
    ctx->channel = ssh_channel_new(session);
    ctx->channel_cbs = (struct ssh_channel_callbacks_struct){
        .userdata = ctx,
        .channel_pty_request_function = on_pty_request,
        .channel_shell_request_function = on_shell_request,
        .channel_data_function = on_data_function,
        .channel_pty_window_change_function = on_pty_window_change,
        .channel_close_function = on_channel_closed,
    };
    ssh_callbacks_init(&ctx->channel_cbs);
    ssh_set_channel_callbacks(ctx->channel, &ctx->channel_cbs);

    return ctx->channel;
}


static int on_pubkey_auth(ssh_session session, const char *user, struct ssh_key_struct *pubkey, char signature_state, void *userdata)
{
    (void)session;
    (void)pubkey;
    (void)signature_state;

    if (signature_state == SSH_PUBLICKEY_STATE_NONE) {
        return SSH_AUTH_SUCCESS;
    }

    if (signature_state != SSH_PUBLICKEY_STATE_VALID) {
        return SSH_AUTH_DENIED;
    }

    session_ctx *ctx = (session_ctx *)userdata;
    ctx->username = user;

    fprintf(stderr, "Authenticated. user=%s signature_state=%02x\n", user, (unsigned char)signature_state);
    // Anyone can play :)
    return SSH_AUTH_SUCCESS;
}

void session_ctx_setup_server_callbacks(session_ctx *ctx)
{
    ctx->server_cbs = (struct ssh_server_callbacks_struct){
        .userdata = ctx,
        .channel_open_request_session_function = on_channel_open,
        .auth_pubkey_function = on_pubkey_auth,
    };
    ssh_callbacks_init(&ctx->server_cbs);
    ssh_set_server_callbacks(ctx->session, &ctx->server_cbs);
}

