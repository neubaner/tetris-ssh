#include <stdbool.h>

#include "test_lib.h"
#include "../src/input.h"

int main(void)
{
    test_category("input printable");

    for (int c = 0; c < 256; c++) {
        if (isprint(c)) {
            test_case("printable char '%c'", c) {
                key_press_reader reader = {0};
                ring_buf_push(&reader.buf, (char)c);
                key_press_result result = key_press_read(&reader);

                test_assert(!result.needs_more_input);
                test_assert_eq_enum(result.press.kind, KEY_KIND_CHAR);
                test_assert_eq_char(result.press.ch, c);
            }
        }
    }

    test_category("input ctrl");

    // NOTE: ctrl+M is \r and it's not recognized as a ctrl char
    const char letters[] = "ABCDEFGHIJKLNOPQRSTUVWXYZ";
    for (const char *c = letters; *c != '\0'; c++) {
        test_case("ctrl char '%c'", (char)*c) {
            key_press_reader reader = {0};
            const char ctrl_char = (*c & 0x1f);
            ring_buf_push(&reader.buf, ctrl_char);
            key_press_result result = key_press_read(&reader);

            test_assert(!result.needs_more_input);
            test_assert_eq_enum(result.press.kind, KEY_KIND_CTRL);
            test_assert_eq_char(result.press.ch, *c);
        }
    }

    test_category("input");

    test_case("enter") {
        key_press_reader reader = {0};
        ring_buf_push(&reader.buf, '\r');
        key_press_result result = key_press_read(&reader);

        test_assert(!result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_ENTER);
        test_assert_eq_char(result.press.ch, '\0');
    }

    test_case("arrow key up") {
        key_press_reader reader = {0};
        ring_buf_push(&reader.buf, '\x1b');
        ring_buf_push(&reader.buf, '[');
        ring_buf_push(&reader.buf, 'A');
        key_press_result result = key_press_read(&reader);

        test_assert(!result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_ARROW_UP);
        test_assert_eq_char(result.press.ch, '\0');
    }

    test_case("arrow key down") {
        key_press_reader reader = {0};
        ring_buf_push(&reader.buf, '\x1b');
        ring_buf_push(&reader.buf, '[');
        ring_buf_push(&reader.buf, 'B');
        key_press_result result = key_press_read(&reader);

        test_assert(!result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_ARROW_DOWN);
        test_assert_eq_char(result.press.ch, '\0');
    }

    test_case("arrow key right") {
        key_press_reader reader = {0};
        ring_buf_push(&reader.buf, '\x1b');
        ring_buf_push(&reader.buf, '[');
        ring_buf_push(&reader.buf, 'C');
        key_press_result result = key_press_read(&reader);

        test_assert(!result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_ARROW_RIGHT);
        test_assert_eq_char(result.press.ch, '\0');
    }

    test_case("arrow key left") {
        key_press_reader reader = {0};
        ring_buf_push(&reader.buf, '\x1b');
        ring_buf_push(&reader.buf, '[');
        ring_buf_push(&reader.buf, 'D');
        key_press_result result = key_press_read(&reader);

        test_assert(!result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_ARROW_LEFT);
        test_assert_eq_char(result.press.ch, '\0');
    }

    test_case("more input") {
        key_press_reader reader = {0};

        ring_buf_push(&reader.buf, '\x1b');
        key_press_result result = key_press_read(&reader);
        test_assert(result.needs_more_input);

        ring_buf_push(&reader.buf, '[');
        result = key_press_read(&reader);
        test_assert(result.needs_more_input);

        ring_buf_push(&reader.buf, 'A');
        result = key_press_read(&reader);
        test_assert(!result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_ARROW_UP);
        test_assert_eq_char(result.press.ch, '\0');
    }

    test_case("unknown input") {
        key_press_reader reader = {0};

        ring_buf_push(&reader.buf, '\xff');
        key_press_result result = key_press_read(&reader);

        // Unknown inputs are ignored and more key presses are required
        test_assert(result.needs_more_input);
        test_assert_eq_enum(result.press.kind, KEY_KIND_NO_KEY);
        test_assert_eq_char(result.press.ch, '\0');
    }

    return test_summary();
}
