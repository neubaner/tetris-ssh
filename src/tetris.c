#include <errno.h>
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
    int master;
    int slave;
    struct winsize win_size;
};

struct session_ctx {
    ssh_event event;
    ssh_channel channel;
    struct ssh_channel_callbacks_struct channel_cbs;

    const char* username;
    struct pty_data pty;
};

static int ptsname_r(int fd, char *buf, size_t size)
{
    if (buf == NULL) {
        return EINVAL;
    }

    unsigned int slave_pty_number;
    int result = ioctl(fd, TIOCGPTN, &slave_pty_number);
    if (result != 0) {
        return result;
    }

    int written_bytes = snprintf(buf, size, "/dev/pts/%d", slave_pty_number);

    if ((size_t)written_bytes >= size) {
        return ERANGE;
    }

    return 0;
}

static int unlockpt(int fd)
{
    int zero = 0;
    return ioctl(fd, TIOCSPTLCK, &zero);
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

    int pty_master = open("/dev/ptmx", O_RDWR);
    int result = unlockpt(pty_master);
    if (result != 0) {
        fprintf(stderr, "Failed to unlock pty_master. %s\n", strerror(result));
        close(pty_master);
        return SSH_ERROR;
    }

    char pty_slave_name[64];
    result = ptsname_r(pty_master, pty_slave_name, sizeof(pty_slave_name));
    if (result != 0) {
        fprintf(stderr, "Failed to get pts name. %s.\n", strerror(result));
        close(pty_master);
        return SSH_ERROR;
    }

    int pty_slave = open(pty_slave_name, O_RDWR | O_NOCTTY);

    printf("Opening pts = %s\n", pty_slave_name);

    ctx->pty.master = pty_master;
    ctx->pty.slave = pty_slave;

    printf("term=%s width=%d height=%d, pxwidth=%d, pwheight=%d\n", term, width, height, pxwidth, pwheight);
    return SSH_OK;
}

static int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
{
    (void)session;
    (void)channel;
    (void)is_stderr;

    struct session_ctx *ctx = (struct session_ctx *)userdata;

    printf("Received %d bytes:", len);
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X", ((unsigned char *)data)[i]);
    }
    printf("\n");

    if (len > 0) {
        return write(ctx->pty.master, data, len);
    }

    return 0;
}

static int process_pty_event(socket_t fd, int revents, void *userdata)
{
    ssh_channel channel = (ssh_channel)userdata;

    if (revents & POLLIN) {
        char buffer[4096];

        int n = read(fd, &buffer, sizeof(buffer));

        if (n > 0) {
            ssh_channel_write(channel, &buffer, n);
        }

        return n;
    }

    return -1;
}

static int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
    (void)session;
    (void)channel;
    (void)userdata;

    struct session_ctx *ctx = (struct session_ctx *)userdata;

    ssh_event_add_fd(ctx->event, ctx->pty.master, POLLIN, process_pty_event, channel);

    const char hello_world[] = "Hello world!\n";
    ssize_t written = write(ctx->pty.master, &hello_world, sizeof(hello_world) - 1);
    (void)written;

    return SSH_OK;
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
