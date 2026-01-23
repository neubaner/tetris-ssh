#include <assert.h>
#include <stdbool.h>
#include <ctype.h>

#include "input.h"
#include "ring_buf.h"

static const char *keys_kind_str_table[] = {
#define KEY_KIND_TABLE(code, str) [code] = str,
    KEY_KIND_X_MACRO(KEY_KIND_TABLE)
#undef KEY_KIND_TABLE
};

const char *key_kind_str(key_kind kind) {
    assert(kind >= KEY_KIND_NO_KEY && kind < KEY_KIND_COUNT);
    return keys_kind_str_table[kind];
}

// References:
// [xterm]: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
// [ansi escape sequences]: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
key_press_result key_press_read(key_press_reader *reader)
{
    for(;;) {
        if (ring_buf_is_empty(&reader->buf)) {
            return (key_press_result){ .needs_more_input = true, };
        }

        char top = ring_buf_shift(&reader->buf);

        switch(reader->state) {
        case KEY_PRESS_READER_STATE_INITIAL: {
            switch(top) {
            case '\x1b':
                reader->state = KEY_PRESS_READER_STATE_ESCAPE;
                break;
            case '\x9b':
                reader->state = KEY_PRESS_READER_STATE_CSI;
                break;
            case '\r':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ENTER,
                };
            default:
                if (top >= ('A' & 0x1f) && top <= ('Z' & 0x1f)) {
                    return (key_press_result) {
                        .press.ch = top + 'A' - 1,
                        .press.kind = KEY_KIND_CTRL,
                    };
                } else if (isprint(top)) {
                    // TODO: Handle UTF-8 characters
                    return (key_press_result) {
                        .press.ch = top,
                        .press.kind = KEY_KIND_CHAR,
                    };
                } else {
                    // XTerm resets their state machine to the initial state when finding unexcepted charaters.
                    reader->state = KEY_PRESS_READER_STATE_INITIAL;
                }
            }
        } break;
        case KEY_PRESS_READER_STATE_ESCAPE: {
            // TODO: There must be some timeout to handle the ESC key as a separate key
            if (top == '[') {
                reader->state = KEY_PRESS_READER_STATE_CSI;
            } else {
                // TODO: Should we handle other escape sequences?
                reader->state = KEY_PRESS_READER_STATE_INITIAL;
            }
        } break;
        case KEY_PRESS_READER_STATE_CSI: {
            reader->state = KEY_PRESS_READER_STATE_INITIAL;

            // TODO: Cursor movement might have a parameter
            switch (top) {
            case 'A':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_UP,
                };
            case 'B':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_DOWN,
                };
            case 'C':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_RIGHT,
                };
            case 'D':
                return (key_press_result) {
                    .press.kind = KEY_KIND_ARROW_LEFT,
                };
            }
        }
        break;
        }
    }
}
