#include "SafePath.h"

#include <cstring>

namespace safepath {
namespace {

char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

bool sameIgnoringCase(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

bool allowed(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
         c == '_' || c == '-';
}

}  // namespace

Result check(const char* path) {
  if (!path || path[0] != '/') return Result::NotAbsolute;
  const size_t n = strlen(path);
  if (n > kMaxLength) return Result::TooLong;
  if (n == 1) return Result::Ok;  // the root

  // One name at a time. An empty name is only fine as the end ("/dir/"), and a name
  // of dots only is a parent or current directory in disguise.
  size_t i = 1;
  const char* lastName = nullptr;
  size_t lastLength = 0;
  while (i <= n) {
    size_t j = i;
    bool dotsOnly = true;
    while (j < n && path[j] != '/') {
      if (!allowed(path[j])) return Result::BadCharacter;
      if (path[j] != '.') dotsOnly = false;
      ++j;
    }
    const size_t length = j - i;
    if (length == 0) {
      if (i != n) return Result::EmptySegment;
      break;
    }
    if (dotsOnly) return Result::DotSegment;
    if (length > kMaxSegment) return Result::SegmentTooLong;
    lastName = path + i;
    lastLength = length;
    i = j + 1;
  }
  const size_t suffix = strlen(kTempSuffix);
  if (lastName && lastLength >= suffix && sameIgnoringCase(lastName + lastLength - suffix, kTempSuffix, suffix)) {
    return Result::Reserved;
  }
  return Result::Ok;
}

bool inConfigDir(const char* path) {
  return path && strlen(path) >= 8 && sameIgnoringCase(path, "/config/", 8);
}

bool endsWith(const char* s, const char* suffix) {
  if (!s || !suffix) return false;
  const size_t a = strlen(s), b = strlen(suffix);
  return a >= b && sameIgnoringCase(s + a - b, suffix, b);
}

bool sameFile(const char* a, const char* b) {
  if (!a || !b) return false;
  const size_t n = strlen(a);
  return n == strlen(b) && sameIgnoringCase(a, b, n);
}

}  // namespace safepath
