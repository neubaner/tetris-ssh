#include <assert.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <threads.h>
#include "bits/pthreadtypes.h"
#include "input.h"
#include "pthread.h"

typedef struct pty_win_size {
    int width;
    int height;
    int px_width;
    int px_height;
} pty_win_size;

typedef struct pty_data {
    pty_win_size win_size;
} pty_data;

typedef struct session_ctx {
    size_t next_free;
    ssh_session session;
    ssh_channel channel;
    struct ssh_channel_callbacks_struct channel_cbs;

    const char *username;
    struct pty_data pty;
    key_press_reader key_reader;
} session_ctx;

#define ANSI_ERASE_ENTITY_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME_POSITION "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"

static int x = 0;
static int y = 0;

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

    // session_ctx *ctx = (session_ctx *)userdata;

    render(channel, x, y);

    return SSH_OK;
}

static int pty_request(ssh_session session,
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

static int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
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
    uint32_t i;
    for (i = 0; i < len; i++) {
        char c = chars[i];

        if (ring_buf_is_full(&ctx->key_reader.buf)) {
            break;
        }

        ring_buf_push(&ctx->key_reader.buf, c);
    }

    while (!ring_buf_is_empty(&ctx->key_reader.buf)) {
        key_press_result result = key_press_read(&ctx->key_reader);

        if (result.needs_more_input) {
            break;
        }

        key_press press = result.press;

        printf("Got key ");
        if (press.kind == KEY_KIND_CHAR) {
            printf("%c\n", press.ch);
        } else if (press.kind == KEY_KIND_CTRL) {
            printf("ctrl+%c\n", press.ch);
        } else {
            printf("%s\n", key_kind_str(press.kind));
        }

        int speed = press.kind == KEY_KIND_CHAR && isupper(press.ch) ? 2 : 1;

        if ((press.kind == KEY_KIND_CHAR && tolower(press.ch) == 'j') || press.kind == KEY_KIND_ARROW_DOWN) {
            y += 1 * speed;
        } else if ((press.kind == KEY_KIND_CHAR && tolower(press.ch) == 'k') || press.kind == KEY_KIND_ARROW_UP) {
            y -= 1 * speed;
        } else if ((press.kind == KEY_KIND_CHAR && tolower(press.ch) == 'h') || press.kind == KEY_KIND_ARROW_LEFT) {
            x -= 1 * speed;
        } else if ((press.kind == KEY_KIND_CHAR && tolower(press.ch) == 'l') || press.kind == KEY_KIND_ARROW_RIGHT) {
            x += 1 * speed;
        } else if (press.kind == KEY_KIND_CTRL && press.ch == 'C') {
            ssh_channel_close(channel);
            return -1;
        }
    }

    render(channel, x, y);

    return i;
}

static int pty_window_change(ssh_session session,
                             ssh_channel channel,
                             int width, int height,
                             int pxwidth, int pwheight,
                             void *userdata)
{
    (void)session;
    session_ctx *ctx = (session_ctx *)userdata;

    ctx->pty.win_size.width = width;
    ctx->pty.win_size.height = height;
    ctx->pty.win_size.px_width = pxwidth;
    ctx->pty.win_size.px_height = pwheight;

    printf("resize: width=%d height=%d, pxwidth=%d, pxheight=%d\n", width, height, pxwidth, pwheight);
    render(channel, x, y);

    return SSH_OK;
}

static ssh_channel channel_open(ssh_session session, void *userdata)
{
    session_ctx *ctx = (session_ctx *)userdata;

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
        .channel_pty_window_change_function = pty_window_change,
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

    // TODO: is this really all we need to so for our use case?
    session_ctx *ctx = (session_ctx *)userdata;
    ctx->username = user;

    // Anyone can play :)
    return SSH_AUTH_SUCCESS;
}

static void handle_session(ssh_session session, ssh_event event)
{
    ssh_set_auth_methods(session, SSH_AUTH_METHOD_PUBLICKEY);

    session_ctx ctx = {
        .session = session,
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

    for(;;) {
        if (ssh_event_dopoll(event, -1) == SSH_ERROR) {
            ssh_channel_close(ctx.channel);
            break;
        }
    }

    ssh_channel_free(ctx.channel);
}

#define WORKER_THREADS_COUNT 8
#define WORKER_THREAD_SESSION_COUNT_MAX 1024

typedef struct worker_thread {
    pthread_t thread;
    size_t id;
    ssh_event event;

    pthread_mutex_t session_mutex;
    size_t session_count;
    // Should only access the session array if you hold the session_mutex
    // TODO: This should be a dynamically sized array
    session_ctx sessions[WORKER_THREAD_SESSION_COUNT_MAX];
    // TODO: This is memory inneficient
    bool sessions_used[WORKER_THREAD_SESSION_COUNT_MAX];
} worker_thread;

#define SESSION_QUEUE_CAPACITY 16
#define SESSION_QUEUE_MASK(v) ((v) % SESSION_QUEUE_CAPACITY)

typedef struct session_queue {
    atomic_size_t head;
    atomic_size_t tail;
    ssh_session sessions[SESSION_QUEUE_CAPACITY];
} session_queue;

static bool session_queue_push(session_queue *queue, ssh_session session)
{
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t next_tail = tail + 1;
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);

    // The list is full
    if (tail == head) {
        return false;
    }
    queue->sessions[SESSION_QUEUE_MASK(tail)] = session;
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release);

    return true;
}

static bool session_queue_shift(session_queue *queue, ssh_session *session)
{
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t next_head = head + 1;
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

    // The list is full
    if (tail == head) {
        return false;
    }

    *session = queue->sessions[SESSION_QUEUE_MASK(head)];
    atomic_store_explicit(&queue->head, next_head, memory_order_release);

    return true;
}

static worker_thread worker_threads[WORKER_THREADS_COUNT] = {0};

static void *worker_thread_proc(void *arg)
{
    worker_thread *thread = (worker_thread *)arg;

    printf("Hello from thread %zu\n", thread->id);

    for(;;) {
        // Do not block poll
        if (ssh_event_dopoll(thread->event, 0) == SSH_ERROR) {
            break;
        }
    }

    ssh_event_free(thread->event);
    return NULL;
}

static void setup_workers(void)
{
    for (size_t id = 0; id < WORKER_THREADS_COUNT; id++) {
        pthread_attr_t attr;
        int result;
        if ((result = pthread_attr_init(&attr)) != 0) {
            fprintf(stderr, "ERROR: pthread_attr_init %s(%d)", strerror(result), result);
            exit(EXIT_FAILURE);
        }

        ssh_event event = ssh_event_new();
        if (event == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        if ((result = pthread_mutex_init(&worker_threads[id].session_mutex, NULL)) != 0) {
            fprintf(stderr, "ERROR: pthread_mutex_init %s(%d)", strerror(result), result);
            exit(EXIT_FAILURE);
        }
        worker_threads[id].id = id;
        worker_threads[id].event = event;

        if ((result = pthread_create(&worker_threads[id].thread, &attr, worker_thread_proc, (void *)&worker_threads[id])) != 0) {
            fprintf(stderr, "ERROR: pthread_create %s(%d)", strerror(result), result);
            exit(EXIT_FAILURE);
        }

        if((result = pthread_attr_destroy(&attr)) != 0) {
            fprintf(stderr, "ERROR: pthread_attr_destroy %s(%d)", strerror(result), result);
            exit(EXIT_FAILURE);
        }
    }
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

    setup_workers();

    for(;;) {
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
