#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pty.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

struct pty_data {
    struct winsize win_size;
};

struct session_ctx {
    ssh_event event;
    ssh_channel channel;
    struct ssh_channel_callbacks_struct channel_cbs;

    const char *username;
    struct pty_data pty;
};

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

static int x = 10;
static int y = 10;

static void render(ssh_channel channel, int x, int y)
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

static int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
    (void)session;
    (void)channel;
    (void)userdata;

    // struct session_ctx *ctx = (struct session_ctx *)userdata;

    render(channel, x, y);

    return SSH_OK;
}

static int pty_request(ssh_session session,
                        ssh_channel channel,
                        const char *term,
                        int width, int height,
                        int pxwidth, int pwheight,
                        void *userdata)
{
    (void)session;
    (void)channel;

    struct session_ctx *ctx = (struct session_ctx *)userdata;

    ctx->pty = (struct pty_data){
        .win_size.ws_col = width,
        .win_size.ws_row = height,
        .win_size.ws_xpixel = pxwidth,
        .win_size.ws_ypixel = pwheight,
    };

    printf("term=%s width=%d height=%d, pxwidth=%d, pwheight=%d\n", term, width, height, pxwidth, pwheight);
    return SSH_OK;
}

static int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
{
    (void)channel;
    (void)session;
    (void)is_stderr;
    (void)userdata;

    // struct session_ctx *ctx = (struct session_ctx *)userdata;

    printf("Received %d bytes:", len);
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X", ((unsigned char *)data)[i]);
    }
    printf("\n");

    if (len > 0) {
        // return ssh_channel_write(channel, data, len);
    }

    char *chars = (char *)data;
    uint32_t i;
    for (i = 0; i < len; i++) {
        char c = chars[i];

        if (c == 'j') {
            y += 1;
        } else if (c == 'k') {
            y -= 1;
        } else if (c == 'l') {
            x += 1;
        } else if (c == 'h') {
            x -= 1;
        } else if (c == 0x03) {
            ssh_channel_close(channel);
            return -1;
        }
    }

    render(channel, x, y);

    return i;
}


static ssh_channel channel_open(ssh_session session, void *userdata)
{
    struct session_ctx *ctx = (struct session_ctx *)userdata;

    if (ctx->channel != NULL) {
        // A session can only have a single channel
        return NULL;
    }

    ctx->channel = ssh_channel_new(session);
    ctx->channel_cbs = (struct ssh_channel_callbacks_struct){
        .userdata = ctx,
        .channel_pty_request_function = pty_request,
        .channel_shell_request_function = shell_request,
        .channel_data_function = data_function,
    };

    ssh_callbacks_init(&ctx->channel_cbs);
    ssh_set_channel_callbacks(ctx->channel, &ctx->channel_cbs);

    return ctx->channel;
}

static int pubkey_auth(ssh_session session, const char *user, struct ssh_key_struct *pubkey, char signature_state, void *userdata)
{
    (void)session;
    (void)pubkey;
    (void)signature_state;

    struct session_ctx *ctx = (struct session_ctx *)userdata;
    ctx->username = user;

    // Anyone can play :)
    return SSH_AUTH_SUCCESS;
}

static void handle_session(ssh_session session, ssh_event event)
{
    ssh_set_auth_methods(session, SSH_AUTH_METHOD_PUBLICKEY);

    struct session_ctx ctx = {
        .event = event,
    };

    struct ssh_server_callbacks_struct server_cbs = {
        .userdata = &ctx,
        .channel_open_request_session_function = channel_open,
        .auth_pubkey_function = pubkey_auth,
    };
    ssh_callbacks_init(&server_cbs);
    ssh_set_server_callbacks(session, &server_cbs);

    if (ssh_handle_key_exchange(session) != SSH_OK) {
        fprintf(stderr, "%s", ssh_get_error(session));
        return;
    }

    ssh_event_add_session(event, session);

    while(true) {
        if (ssh_event_dopoll(event, -1) == SSH_ERROR) {
            ssh_channel_close(ctx.channel);
            break;
        }
    }

    ssh_channel_free(ctx.channel);
}

int main(void)
{
    if (ssh_init() != SSH_OK) {
        fprintf(stderr, "Failed to initialize libssh\n");
        return EXIT_FAILURE;
    }

    ssh_bind sshbind = ssh_bind_new();
    if (sshbind == NULL) {
        fprintf(stderr, "Out of memory\n");
        ssh_finalize();
        return EXIT_FAILURE;
    }

    const char address[] = "localhost";
    // TODO: Accept key file as a parameter. Check how to generate a strong key
    const char host_key_path[] = "/home/neubaner/code/tetris-ssh/rsa_key";
    const int port = 2222;

    // TODO: Check options result
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, address);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, host_key_path);

    if (ssh_bind_listen(sshbind) < 0) {
        fprintf(stderr, "Failed to bind: %s\n", ssh_get_error(sshbind));

        ssh_bind_free(sshbind);
        ssh_finalize();
        return EXIT_FAILURE;
    }

    printf("Listening on %s:%d\n", address, port);

    while(true) {
        ssh_session session = ssh_new();

        if (session == NULL) {
            fprintf(stderr, "Out of memory\n");
            ssh_bind_free(sshbind);
            ssh_finalize();

            return EXIT_FAILURE;
        }

        if (ssh_bind_accept(sshbind, session) != SSH_OK) {
            fprintf(stderr, "Failed to accept session: %s\n", ssh_get_error(sshbind));

            ssh_bind_free(sshbind);
            ssh_free(session);
            return EXIT_FAILURE;
        }

        printf("Accepted connection!\n");

        int pid = fork();
        if (pid == 0) {
            ssh_event event = ssh_event_new();
            if (event == NULL) {
                fprintf(stderr, "Out of memory\n");
                ssh_free(session);
                ssh_bind_free(sshbind);
                ssh_finalize();

                return EXIT_FAILURE;
            }
            handle_session(session, event);
            ssh_event_free(event);

            ssh_disconnect(session);
            ssh_free(session);

            return EXIT_SUCCESS;
        }
    }

    ssh_bind_free(sshbind);
    ssh_finalize();

    return EXIT_SUCCESS;
}
