#include <stdbool.h>
#include <stdint.h>

#include "test_lib.h"
#include "../src/ring_buf.h"

int main(void)
{
    test_category("ring_buf");

    test_case("zero initialized ring_buf is empty") {
        ring_buf buf = {0};
        test_assert_eq_uint(uint8_t, ring_buf_size(&buf), 0);
        test_assert(ring_buf_is_empty(&buf));
        test_assert(!ring_buf_is_full(&buf));
    }

    test_case("one item") {
        ring_buf buf = {0};
        ring_buf_push(&buf, 'a');
        test_assert_eq_uint(uint8_t, ring_buf_size(&buf), 1);
        test_assert(!ring_buf_is_empty(&buf));
        test_assert(!ring_buf_is_full(&buf));
    }

    test_case("full buffer") {
        ring_buf buf = {0};
        for (int i = 0; i < RING_BUF_CAPACITY; i++) {
            ring_buf_push(&buf, 'a');
        }
        test_assert_eq_uint(uint8_t, ring_buf_size(&buf), RING_BUF_CAPACITY);
        test_assert(!ring_buf_is_empty(&buf));
        test_assert(ring_buf_is_full(&buf));
    }

    test_case("shift one item") {
        ring_buf buf = {0};
        ring_buf_push(&buf, 'a');
        char elem = ring_buf_shift(&buf);

        test_assert_eq_char(elem, 'a');
        test_assert(ring_buf_is_empty(&buf));
    }

    test_case("shift three items") {
        ring_buf buf = {0};
        ring_buf_push(&buf, 'a');
        ring_buf_push(&buf, 'b');
        ring_buf_push(&buf, 'c');

        char a = ring_buf_shift(&buf);
        char b = ring_buf_shift(&buf);
        char c = ring_buf_shift(&buf);

        test_assert_eq_char(a, 'a');
        test_assert_eq_char(b, 'b');
        test_assert_eq_char(c, 'c');
    }

    test_case("peek") {
        ring_buf buf = {0};
        ring_buf_push(&buf, 'a');
        char elem = ring_buf_peek(&buf);

        test_assert_eq_char(elem, 'a');
        test_assert_eq_uint(uint8_t, ring_buf_size(&buf), 1);
    }

    test_case("cursor wrapping") {
        ring_buf buf = {
            .head = 0xff,
            .tail = 0xff,
        };

        ring_buf_push(&buf, 'a');
        test_assert_eq_uint(uint8_t, ring_buf_size(&buf), 1);
        test_assert(!ring_buf_is_empty(&buf));
        test_assert(!ring_buf_is_full(&buf));
    }

    return test_summary();
}
