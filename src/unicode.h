#ifndef TETRIS_SSH_UNICODE_H
#define TETRIS_SSH_UNICODE_H

#include <assert.h>
#include <stddef.h>
#include <string.h>

// Represents an encoded unicode codepoint in UTF-8
// Reference: https://en.wikipedia.org/wiki/UTF-8#Description
typedef struct utf8_char {
    unsigned char bytes[4];
} utf8_char;

// UTF-8 encodes codepoint ranges in different sizes. We can use the codepoint ranges to encode
// the character size as the size of an array and encode the codepoint differently depending on the size.
#define UTF8(cp) _Generic((char(*)[((cp) >= 0x80) + ((cp) >= 0x800) + ((cp) >= 0x10000) + ((cp) > 0x10FFFF) + 1])0, \
    char(*)[1]: (utf8_char) {{ (cp), 0, 0, 0 }}, \
    char(*)[2]: (utf8_char) {{ 0xC0 | ((cp) >> 6), 0x80 | ((cp) & 0x3F), 0, 0 }}, \
    char(*)[3]: (utf8_char) {{ 0xE0 | ((cp) >> 12), 0x80 | (((cp) >> 6) & 0x3F), 0x80 | ((cp) & 0x3F), 0 }}, \
    char(*)[4]: (utf8_char) {{ 0xF0 | ((cp) >> 18), 0x80 | (((cp) >> 12) & 0x3F), 0x80 | (((cp) >> 6) & 0x3F), 0x80 | ((cp) & 0x3F) }})

#define BOX_LIGHT_DOWN_RIGHT UTF8(0x250C)
#define BOX_LIGHT_DOWN_LEFT UTF8(0x2510)
#define BOX_LIGHT_UP_RIGHT UTF8(0x2514)
#define BOX_LIGHT_UP_LEFT UTF8(0x2518)
#define BOX_LIGHT_HORIZONTAL UTF8(0x2500)
#define BOX_LIGHT_VERTICAL UTF8(0x2502)

int utf8_char_length(utf8_char character) {
    unsigned char b = character.bytes[0];

    // Check the first byte to determine the size of the utf-8 encoded code point
    if ((b & 0x80) == 0x00) { return 1; } // 0xxxxxxx
    if ((b & 0xE0) == 0xC0) { return 2; } // 110xxxxx
    if ((b & 0xF0) == 0xE0) { return 3; } // 1110xxxx
    if ((b & 0xF8) == 0xF0) { return 4; } // 11110xxx

    return -1;
}

size_t utf8_char_write(utf8_char character, char *buffer, size_t size) {
    int character_size = utf8_char_length(character);
    assert(character_size > 0 && size >= (size_t)character_size);
    memcpy(buffer, &character.bytes, character_size);

    return character_size;
}


#endif
