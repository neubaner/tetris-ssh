#define _POSIX_C_SOURCE 199309L

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <pthread.h>
#include <threads.h>

#include "game.h"
#include "input.h"
#include "session.h"
#include "session_pool.h"
#include "session_scheduler.h"
#include "spsc_queue.h"
#include "renderer.h"

#define GAME_FRAMERATE_MS (1000 / 60)
#define GAME_FRAMERATE_NS ((GAME_FRAMERATE_MS) * 1000 * 1000)
#define SESSION_TIMEOUT_NS ((uint64_t)5 * 1000 * 1000 * 1000)
#define WORKER_THREADS_COUNT 12
#define WORKER_THREAD_SESSION_COUNT_MAX 1024

static uint64_t get_nanos(void)
{
    struct timespec time;
    assert(clock_gettime(CLOCK_MONOTONIC, &time) == 0);

    return (time.tv_sec * 1000 * 1000 * 1000) + time.tv_nsec;
}

typedef struct worker_thread {
    pthread_t thread;
    size_t id;
    ssh_event event;

    spsc_queue incoming_sessions;
    session_pool session_pool;
    session_scheduler scheduler;
} worker_thread;

static worker_thread worker_threads[WORKER_THREADS_COUNT] = {0};

void print_ansii(const char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        char c = buffer[i];
        if (isprint(c)) {
            printf("%c", c);
        } else {
            printf("\\x%02x", (unsigned char)c);
        }
    }
}

static uint64_t sat_sub_uint64(uint64_t a, uint64_t b)
{
    return a > b ? a - b : 0;
}

static int compute_poll_timeout(worker_thread *thread)
{
    uint64_t current_ns = get_nanos();
    uint64_t poll_timeout_ns;
    session_scheduler_item item;
    if (session_scheduler_min(&thread->scheduler, &item)) {
        uint64_t next_frame_ns = item.timestamp;

        // Wait up to the next frame time
        poll_timeout_ns = sat_sub_uint64(next_frame_ns, current_ns);
    } else {
        // No sessions are scheduled. Wait for a frame.
        poll_timeout_ns = GAME_FRAMERATE_NS;
    }
    // Subtract slack_ns from poll_timeout_ns to wake up the thread before a full frame would have elapsed.
    // The kernel mostly likely will not wake up the thread in time.
    const uint64_t slack_ns = 1 * 1000 * 1000;
    poll_timeout_ns = sat_sub_uint64(poll_timeout_ns, slack_ns);

    return poll_timeout_ns / (1000 * 1000);
}

static void poll_events(worker_thread *thread)
{
    int poll_timeout_ms = compute_poll_timeout(thread);

    if(ssh_event_dopoll(thread->event, poll_timeout_ms) == SSH_ERROR) {
        // If dopoll fails it means no session is attached to the event loop, we should just
        // wait for a frame and try later and try again later
        struct timespec sleep_duration = {
            .tv_nsec = GAME_FRAMERATE_NS,
            .tv_sec = 0,
        };
        nanosleep(&sleep_duration, NULL);
    }
}

static void register_new_sessions(worker_thread *thread)
{
    ssh_session new_session;
    while (spsc_queue_shift(&thread->incoming_sessions, (void **)&new_session)) {
        ssh_set_blocking(new_session, false);

        session_ctx *ctx = session_pool_alloc(&thread->session_pool);
        if (ctx == NULL) {
            fprintf(stderr, "No empty session slot found on thread %zu\n", thread->id);
            ssh_free(new_session);
            continue;
        }

        *ctx = (session_ctx) {
            .session = new_session,
            .status = SESSION_STATUS_ALIVE,
        };
        session_ctx_setup_server_callbacks(ctx);

        printf("Accepted connection on thread %zu\n", thread->id);

        uint64_t current_ns = get_nanos();
        ctx->last_frame_ns = current_ns;
        ctx->timeout_dealine_ns = current_ns + SESSION_TIMEOUT_NS;
        ctx->status = SESSION_STATUS_KEY_EXCHANGE_PENDING;

        session_scheduler_insert(&thread->scheduler, (session_scheduler_item) {
            .ctx = ctx,
            .timestamp = ctx->last_frame_ns,
        });
    }
}

static void run_sessions_tick(worker_thread *thread)
{
    uint64_t current_ns = get_nanos();
    session_scheduler_item item;
    while (session_scheduler_extract_min_threshold(&thread->scheduler, current_ns, &item)) {
        session_ctx *ctx = item.ctx;
        assert(ctx != NULL);
        uint64_t dt_ns = current_ns - ctx->last_frame_ns;

        session_status status = ctx->status;
        switch (status) {
        case SESSION_STATUS_KEY_EXCHANGE_PENDING: {
            int rc = ssh_handle_key_exchange(ctx->session);

            switch (rc) {
            case SSH_OK: {
                ctx->status = SESSION_STATUS_CHANNEL_PENDING;
                // Key exchange was successful, now we can add the session to the event loop
                if (ssh_event_add_session(thread->event, ctx->session) != SSH_OK) {
                    fprintf(stderr, "Could not add the session to the event pool: %s\n", strerror(errno));
                    ctx->status = SESSION_STATUS_CLOSING;
                };
            } break;
            case SSH_AGAIN: {
                if (ctx->timeout_dealine_ns < current_ns) {
                    // We could not do a key exchange within the timeout.
                    ctx->status = SESSION_STATUS_CLOSING;
                }
            } break;
            case SSH_ERROR: {
                fprintf(stderr, "Got SSH_ERROR %s\n", ssh_get_error(ctx->session));
                ctx->status = SESSION_STATUS_CLOSING;
            } break;
            default: {
                fprintf(stderr, "Unknown return code while doing key exchange: %d\n", rc);
                ctx->status = SESSION_STATUS_CLOSING;
            } break;
            }

        } break;
        case SESSION_STATUS_CHANNEL_PENDING: {
            if (ctx->timeout_dealine_ns < current_ns) {
                // The channel was not created within the timeout, close the session.
                ctx->status = SESSION_STATUS_CLOSING;
            }
        } break;
        case SESSION_STATUS_CHANNEL_OPEN: {
            // TODO: We should not need a separate buffer to write to the channel.
            // Going to alternative screen has a well-known size and we can just use it...
            char buffer[32] = {0};
            char *cursor = buffer;

            cursor += renderer_enter_alt_screen(cursor, sizeof(buffer) + (buffer - cursor));
            size_t written = cursor - buffer;
            ssh_channel_write(ctx->channel, buffer, written);

            game_init(&ctx->game_state);
            ctx->status = SESSION_STATUS_ALIVE;
        } break;
        case SESSION_STATUS_ALIVE: {
            // TODO: Should this me moved to the game loop?
            while (!ring_buf_is_empty(&ctx->key_reader.buf)) {
                key_press_result result = key_press_read(&ctx->key_reader);

                if (result.needs_more_input) {
                    break;
                }

                key_press press = result.press;

                printf("Got key ");
                if (press.kind == KEY_KIND_CHAR) {
                    printf("'%c'\n", press.ch);
                } else if (press.kind == KEY_KIND_CTRL) {
                    printf("ctrl+'%c'\n", press.ch);
                } else {
                    printf("%s\n", key_kind_str(press.kind));
                }

                if (press.kind == KEY_KIND_CTRL && press.ch == 'C') {
                    ctx->status = SESSION_STATUS_CLOSING;
                }

                game_input(&ctx->game_state, press);
            }

            if (ctx->channel != NULL) {
                // That's probably more than plently for most use cases
                char buffer[4 * 1024] = {0};
                char *cursor = buffer;

                cursor += renderer_clear(&ctx->renderer, cursor, sizeof(buffer) + (buffer - cursor));

                game_update(&ctx->game_state, dt_ns);
                game_render(&ctx->game_state, &ctx->renderer);
                cursor += renderer_render(&ctx->renderer, cursor, sizeof(buffer) + (buffer - cursor));

                size_t written = cursor - buffer;
                ssh_channel_write(ctx->channel, buffer, written);
            }

        } break;
        case SESSION_STATUS_CLOSING: {
            if (ctx->channel != NULL) {
                char buffer[16] = {0};
                char *cursor = buffer;

                cursor += renderer_exit_alt_screen(cursor, sizeof(buffer) + (buffer - cursor));
                size_t written = cursor - buffer;
                ssh_channel_write(ctx->channel, buffer, written);

                ssh_channel_send_eof(ctx->channel);
                ssh_channel_request_send_exit_status(ctx->channel, 0);
                ssh_channel_close(ctx->channel);
                ssh_channel_free(ctx->channel);
            }
            ctx->status = SESSION_STATUS_DEAD;
            ctx->timeout_dealine_ns = current_ns + SESSION_TIMEOUT_NS;
        } break;
        case SESSION_STATUS_DEAD: {
            int is_session_closed = (ssh_get_status(ctx->session) & (SSH_CLOSED | SSH_CLOSED_ERROR)) != 0;

            // Either close the session immediatly when it is closed or when the timeout is expired.
            if (is_session_closed || ctx->timeout_dealine_ns < current_ns) {
                ssh_disconnect(ctx->session);
                ssh_event_remove_session(thread->event, ctx->session);
                ssh_free(ctx->session);
                session_pool_free(&thread->session_pool, ctx);
                ctx = NULL;
            }
        } break;
        default:
            break;
        }

        current_ns = get_nanos();
        if (ctx != NULL) {
            ctx->last_frame_ns = current_ns;

            session_scheduler_insert(&thread->scheduler, (session_scheduler_item) {
                .ctx = ctx,
                .timestamp = ctx->last_frame_ns + GAME_FRAMERATE_NS
            });
        }
    }
}

static void *worker_thread_proc(void *arg)
{
    worker_thread *thread = (worker_thread *)arg;

    for(;;) {
        register_new_sessions(thread);
        poll_events(thread);
        run_sessions_tick(thread);
    }

    ssh_event_free(thread->event);
    return NULL;
}

static void setup_workers(void)
{
    for (size_t id = 0; id < WORKER_THREADS_COUNT; id++) {
        int result;

        ssh_event event = ssh_event_new();
        if (event == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        worker_threads[id].id = id;
        worker_threads[id].event = event;
        session_pool_init(&worker_threads[id].session_pool);

        if ((result = pthread_create(&worker_threads[id].thread, NULL, worker_thread_proc, (void *)&worker_threads[id])) != 0) {
            fprintf(stderr, "ERROR: pthread_create %s(%d)", strerror(result), result);
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
            ssh_free(session);
            continue;
        }

        int thread_index = 0;
        for (int i = 0, min = INT_MAX; i < WORKER_THREADS_COUNT; i++) {
            int session_count = worker_threads[i].session_pool.sessions_count;
            if (session_count < min) {
                thread_index = i;
                min = session_count;
            }
        }

        while (spsc_queue_push(&worker_threads[thread_index].incoming_sessions, session) != true) {
            fprintf(stderr, "Failed to push incoming session\n");
            ssh_free(session);
        };
    }

    ssh_bind_free(sshbind);
    ssh_finalize();

    return EXIT_SUCCESS;
}
