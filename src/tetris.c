#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <pthread.h>
#include <threads.h>

#include "session.h"
#include "input.h"
#include "session_manager.h"
#include "spsc_queue.h"

#define WORKER_THREADS_COUNT 1
#define WORKER_THREAD_SESSION_COUNT_MAX 1024

typedef struct worker_thread {
    pthread_t thread;
    size_t id;
    ssh_event event;

    spsc_queue incoming_sessions;
    session_manager session_manager;
} worker_thread;

static worker_thread worker_threads[WORKER_THREADS_COUNT] = {0};

static void *worker_thread_proc(void *arg)
{
    worker_thread *thread = (worker_thread *)arg;

    for(;;) {
        ssh_session new_session;
        while (spsc_queue_shift(&thread->incoming_sessions, (void **)&new_session)) {
            session_ctx *ctx = session_manager_add(&thread->session_manager, new_session);

            if (ctx == NULL) {
                fprintf(stderr, "No empty session slot found on thread %zu\n", thread->id);
                ssh_free(ctx->session);
                continue;
            }

            printf("Accepted connection on thread %zu and slot %zu\n", thread->id, ctx->session_idx);

            // TODO: Does this block for the session?
            if (ssh_handle_key_exchange(ctx->session) != SSH_OK) {
                fprintf(stderr, "%s\n", ssh_get_error(ctx->session));
                ssh_free(ctx->session);
                session_manager_remove(&thread->session_manager, ctx->session_idx);
                continue;
            }

            if (ssh_event_add_session(thread->event, ctx->session) != SSH_OK) {
                fprintf(stderr, "%s\n", strerror(errno));
                ssh_free(ctx->session);
                session_manager_remove(&thread->session_manager, ctx->session_idx);
                continue;
            };
        }

        // TODO: We should not syncronize all players in the same event loop
        ssh_event_dopoll(thread->event, 0);

        for (size_t idx = 0; idx < thread->session_manager.sessions_count; idx++) {
            session_ctx *ctx = &thread->session_manager.sessions[idx];

            if (ctx->status == SESSION_STATUS_CLOSING) {
                printf("Closing...\n");
                ssh_channel_send_eof(ctx->channel);
                ssh_channel_request_send_exit_status(ctx->channel, 0);
                ssh_channel_close(ctx->channel);
                ssh_channel_free(ctx->channel);
                ctx->status = SESSION_STATUS_DEAD;
            }

            if (ctx->status == SESSION_STATUS_DEAD) {
                printf("Killing %d...\n", ssh_get_status(ctx->session));
                // TODO: We should have a timeout to forcelly close the connection.
                if ((ssh_get_status(ctx->session) & (SSH_CLOSED | SSH_CLOSED_ERROR)) != 0) {
                    printf("Freaking killed!\n");
                    ssh_disconnect(ctx->session);
                    ssh_event_remove_session(thread->event, ctx->session);
                    ssh_free(ctx->session);
                    session_manager_remove(&thread->session_manager, idx);
                }
            }

            if (ctx->status == SESSION_STATUS_ALIVE) {
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
                        ctx->status = SESSION_STATUS_CLOSING;
                    }
                }

                // render(ctx->channel, x, y);
            }
        }
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
            int session_count = worker_threads[i].session_manager.sessions_count;
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
