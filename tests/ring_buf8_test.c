#include <stdbool.h>
#include <stdint.h>

#include "test_lib.h"
#include "../src/ring_buf8.h"

#define INT_QUEUE_CAPACITY_BITS 7
#define INT_QUEUE_CAPACITY (1 << INT_QUEUE_CAPACITY_BITS)

typedef struct int_queue {
    ring_buf8 ring;
    int data[INT_QUEUE_CAPACITY];
} int_queue;

bool int_queue_push(int_queue *queue, int data)
{
    ptrdiff_t idx = ring_buf8_peek_write(&queue->ring, INT_QUEUE_CAPACITY_BITS);
    if (idx < 0) { return false; }

    queue->data[idx] = data;
    return ring_buf8_commit_write(&queue->ring, INT_QUEUE_CAPACITY_BITS);
}

bool int_queue_pop(int_queue *queue, int *data)
{
    ptrdiff_t idx = ring_buf8_peek_read(&queue->ring, INT_QUEUE_CAPACITY_BITS);
    if (idx < 0) { return false; }

    *data = queue->data[idx];
    return ring_buf8_commit_read(&queue->ring);
}

int main(void)
{
    test_category("ring_buf8");

    test_case("zero initialized is empty") {
        ring_buf8 ring = {0};
        test_assert_eq_uint(uint8_t, ring_buf8_size(&ring), 0);
        test_assert(ring_buf8_is_empty(&ring));
    }

    test_case("one item") {
        int cap_bits = 7;
        ring_buf8 ring = {0};
        test_assert(ring_buf8_commit_write(&ring, cap_bits));
        test_assert_eq_uint(uint8_t, ring_buf8_size(&ring), 1);
    }

    test_case("full at 2^cap_bits") {
        int cap_bits = 7;
        ring_buf8 ring = {0};
        for (int i = 0; i < (1 << cap_bits); i++) {
            test_assert(!ring_buf8_is_full(&ring, cap_bits));
            test_assert(ring_buf8_commit_write(&ring, 7));
        }

        test_assert(ring_buf8_is_full(&ring, cap_bits));
        test_assert(!ring_buf8_commit_write(&ring, cap_bits));
    }

    test_case("wrapping") {
        int cap_bits = 7;
        // Empty ring about to be wrapped
        ring_buf8 ring = {
            .head = 0xff,
            .tail = 0xff,
        };

        test_assert(ring_buf8_commit_write(&ring, cap_bits));

        test_assert_eq_uint(uint8_t, ring_buf8_size(&ring), 1);
    }

    test_category("int_queue");

    test_case("one item") {
        int_queue queue = {0};
        test_assert(int_queue_push(&queue, 42));
        test_assert_eq_uint(uint8_t, ring_buf8_size(&queue.ring), 1);

        int data;
        test_assert(int_queue_pop(&queue, &data));
        test_assert_eq_int(int, data, 42);
        test_assert_eq_uint(uint8_t, ring_buf8_size(&queue.ring), 0);
    }

    test_case("ordered dequeue") {
        int test_data[] = {1, 2, 3, 4, 5};
        int_queue queue = {0};

        for (int i = 0; i < 5; i++) {
            test_assert(int_queue_push(&queue, test_data[i]));
        }
        test_assert_eq_uint(uint8_t, ring_buf8_size(&queue.ring), 5);

        for (int i = 0; i < 5; i++) {
            int data;
            test_assert(int_queue_pop(&queue, &data));
            test_assert_eq_int(int, data, test_data[i]);
        }
        test_assert_eq_uint(uint8_t, ring_buf8_size(&queue.ring), 0);
    }

    return test_summary();
}
