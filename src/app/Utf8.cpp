#include "Utf8.h"

#include <cstring>

namespace utf8 {
namespace {

bool isContinuation(char c) { return ((unsigned char)c & 0xC0) == 0x80; }

}  // namespace

size_t fit(const char* text, size_t length, size_t maxBytes) {
  if (length <= maxBytes) return length;
  // `text[maxBytes]` starts the part that is cut off; if it is a continuation byte
  // the character began earlier and has to go entirely.
  size_t n = maxBytes;
  while (n > 0 && isContinuation(text[n])) --n;
  return n;
}

void copy(char* dst, size_t size, const char* begin, const char* end) {
  if (size == 0) return;
  const size_t n = fit(begin, (size_t)(end - begin), size - 1);
  memcpy(dst, begin, n);
  dst[n] = '\0';
}

size_t count(const char* text, size_t length) {
  size_t n = 0;
  for (size_t i = 0; i < length; ++i) {
    if (!isContinuation(text[i])) ++n;
  }
  return n;
}

size_t prefixBytes(const char* text, size_t length, size_t characters) {
  size_t seen = 0;
  for (size_t i = 0; i < length; ++i) {
    if (!isContinuation(text[i])) {
      if (seen == characters) return i;
      ++seen;
    }
  }
  return length;
}

}  // namespace utf8
