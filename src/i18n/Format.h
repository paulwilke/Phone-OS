#pragma once

// Message formatting: `{name}` placeholders and nothing else.
//
// Deliberately smaller than printf. A translator can move a placeholder anywhere
// in the sentence (word order differs between languages), cannot crash the device
// with a wrong conversion specifier, and every placeholder is named, so a missing
// or misspelled one is found by a check instead of printing garbage.
//
//   "{name} is busy"   with name = "Papa"   ->  "Papa is busy"
//   "100 {{percent}}"                       ->  "100 {percent}"      ({{ and }} are literal braces)
//
// A placeholder without a matching argument is left as written ("{name}") and
// reported through `missing`, so tests and the catalog check see it.

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cstdio>
#include <cstring>

namespace i18n {

struct Arg {
  const char* name;
  // Either text or a number; numbers are printed in decimal.
  const char* text = nullptr;
  int64_t number = 0;

  Arg(const char* n, const char* t) : name(n), text(t) {}
  // Any integer type. One template instead of an overload for each type: size_t is the same type as
  // unsigned int on a 32-bit processor, and two overloads of one type do not compile there.
  template <class T, class = typename std::enable_if<std::is_integral<T>::value>::type>
  Arg(const char* n, T v) : name(n), number((int64_t)v) {}
};

// Writes the formatted message into `out` (always NUL-terminated, truncated at
// `size - 1`). Returns the number of placeholders that had no argument.
inline size_t format(const char* pattern, const Arg* args, size_t argCount, char* out, size_t size) {
  size_t o = 0, missing = 0;
  auto put = [&](char c) { if (o + 1 < size) out[o++] = c; };
  auto putText = [&](const char* s) { for (; s && *s; ++s) put(*s); };
  for (const char* p = pattern; *p; ++p) {
    if (p[0] == '{' && p[1] == '{') { put('{'); ++p; continue; }
    if (p[0] == '}' && p[1] == '}') { put('}'); ++p; continue; }
    if (*p != '{') { put(*p); continue; }
    const char* end = strchr(p, '}');
    if (!end) { put(*p); continue; }  // unterminated brace: written as it is
    const size_t len = (size_t)(end - p - 1);
    const Arg* hit = nullptr;
    for (size_t i = 0; i < argCount; ++i) {
      if (strlen(args[i].name) == len && strncmp(args[i].name, p + 1, len) == 0) { hit = &args[i]; break; }
    }
    if (!hit) {
      ++missing;
      for (const char* q = p; q <= end; ++q) put(*q);
    } else if (hit->text) {
      putText(hit->text);
    } else {
      char digits[24];
      snprintf(digits, sizeof(digits), "%lld", (long long)hit->number);
      putText(digits);
    }
    p = end;
  }
  if (size) out[o] = '\0';
  return missing;
}

}  // namespace i18n
