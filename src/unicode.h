#ifndef TETRIS_SSH_UNICODE_H
#define TETRIS_SSH_UNICODE_H

#include <assert.h>
#include <stddef.h>
#include <string.h>

// Represents an encoded unicode codepoint in UTF-8
typedef struct utf8_char {
    unsigned char bytes[4];
} utf8_char;

const utf8_char BOX_LIGHT_DOWN_RIGHT = { .bytes = { 0xe2, 0x94, 0x8c } };
const utf8_char BOX_LIGHT_DOWN_LEFT = { .bytes = { 0xe2, 0x94, 0x90 } };
const utf8_char BOX_LIGHT_UP_RIGHT = { .bytes = { 0xe2, 0x94, 0x94 } };
const utf8_char BOX_LIGHT_UP_LEFT = { .bytes = { 0xe2, 0x94, 0x98 } };
const utf8_char BOX_LIGHT_HORIZONTAL = { .bytes = { 0xe2, 0x94, 0x80 } };
const utf8_char BOX_LIGHT_VERTICAL = { .bytes = { 0xe2, 0x94, 0x82 } };

int utf8_char_length(utf8_char character) {
    unsigned char b = character.bytes[0];

    // Check the first byte to determine the size of the utf-8 encoded code point
    // Reference: https://en.wikipedia.org/wiki/UTF-8#Description
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
