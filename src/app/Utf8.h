#pragma once

// Helpers for fixed-size text fields that hold UTF-8. A byte limit that falls
// inside a character leaves a broken sequence, which the display draws as garbage
// and which breaks the next reader of a file; every cut goes through here.

#include <cstddef>

namespace utf8 {

// The largest length not above `maxBytes` at which `text[0, length)` ends on a
// character boundary. `length` is the number of bytes available.
size_t fit(const char* text, size_t length, size_t maxBytes);

// Copies `[begin, end)` to `dst` (at most `size - 1` bytes, cut on a character
// boundary) and terminates it. Does nothing when `size` is 0.
void copy(char* dst, size_t size, const char* begin, const char* end);

// Number of characters in `[text, text + length)`, counting every byte that is not
// a continuation byte.
size_t count(const char* text, size_t length);

// The length in bytes of the first `characters` characters of `text`.
size_t prefixBytes(const char* text, size_t length, size_t characters);

}  // namespace utf8
