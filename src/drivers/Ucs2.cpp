#include "Ucs2.h"

#include <cstdint>

namespace text {
namespace {

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

uint32_t unitAt(const char* hex, size_t i) {
  return (uint32_t)((hexVal(hex[i]) << 12) | (hexVal(hex[i + 1]) << 8) | (hexVal(hex[i + 2]) << 4) |
                    hexVal(hex[i + 3]));
}

size_t utf8Len(uint32_t cp) { return cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4; }

void putUtf8(uint32_t cp, char* out) {
  if (cp < 0x80) {
    out[0] = (char)cp;
  } else if (cp < 0x800) {
    out[0] = (char)(0xC0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
  } else {
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
  }
}

const char kHex[] = "0123456789ABCDEF";

}  // namespace

bool isUcs2Hex(const char* s, size_t len) {
  if (len % 4 != 0) return false;
  for (size_t i = 0; i < len; ++i)
    if (hexVal(s[i]) < 0) return false;
  return true;
}

bool ucs2HexToUtf8(const char* hex, size_t hexLen, char* out, size_t outSize, bool* truncated) {
  if (truncated) *truncated = false;
  if (outSize == 0) return false;
  out[0] = '\0';
  if (!isUcs2Hex(hex, hexLen)) return false;

  size_t o = 0;
  for (size_t i = 0; i < hexLen; i += 4) {
    uint32_t cp = unitAt(hex, i);
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 4 < hexLen) {
      const uint32_t lo = unitAt(hex, i + 4);
      if (lo >= 0xDC00 && lo <= 0xDFFF) {
        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        i += 4;
      } else {
        cp = 0xFFFD;
      }
    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
      cp = 0xFFFD;
    }
    if (cp == 0) continue;

    const size_t n = utf8Len(cp);
    if (o + n >= outSize) {
      if (truncated) *truncated = true;
      break;
    }
    putUtf8(cp, out + o);
    o += n;
  }
  out[o] = '\0';
  return true;
}

int utf8ToUcs2Hex(const char* utf8, char* out, size_t outSize) {
  if (outSize == 0) return -1;
  out[0] = '\0';
  const unsigned char* p = (const unsigned char*)utf8;
  size_t o = 0;
  int units = 0;

  while (*p) {
    uint32_t cp;
    size_t n;
    if (*p < 0x80) { cp = *p; n = 1; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; n = 2; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; n = 3; }
    else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; n = 4; }
    else { out[0] = '\0'; return -1; }
    for (size_t k = 1; k < n; ++k) {
      if ((p[k] & 0xC0) != 0x80) { out[0] = '\0'; return -1; }  // also catches the end of the string
      cp = (cp << 6) | (p[k] & 0x3F);
    }
    // An overlong form encodes a small value in too many bytes: refused, as are surrogates.
    const uint32_t smallest = n == 1 ? 0 : n == 2 ? 0x80 : n == 3 ? 0x800 : 0x10000;
    if (cp < smallest || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { out[0] = '\0'; return -1; }
    p += n;

    uint32_t u[2];
    int count;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      u[0] = 0xD800 + (cp >> 10);
      u[1] = 0xDC00 + (cp & 0x3FF);
      count = 2;
    } else {
      u[0] = cp;
      count = 1;
    }
    if (o + 4 * (size_t)count >= outSize) { out[0] = '\0'; return -1; }
    for (int k = 0; k < count; ++k) {
      out[o++] = kHex[(u[k] >> 12) & 0xF];
      out[o++] = kHex[(u[k] >> 8) & 0xF];
      out[o++] = kHex[(u[k] >> 4) & 0xF];
      out[o++] = kHex[u[k] & 0xF];
    }
    units += count;
  }
  out[o] = '\0';
  return units;
}

}  // namespace text
