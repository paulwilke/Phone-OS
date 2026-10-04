#include "OperatorLabel.h"

#include <cstring>

#include "Utf8.h"

namespace oplabel {
namespace {

bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// Brands whose first letter is a real part of the name and not a country mark.
bool brandPrefix(const char* name) { return strncmp(name, "T-", 2) == 0 || strncmp(name, "E-", 2) == 0; }

}  // namespace

void shortName(const char* name, char* out, size_t size) {
  if (!out || size == 0) return;
  out[0] = '\0';
  if (!name) return;

  while (*name == ' ') ++name;
  if (isLetter(name[0]) && name[1] == '-' && !brandPrefix(name)) name += 2;
  else if (isLetter(name[0]) && isLetter(name[1]) && name[2] == '-') name += 3;

  size_t length = 0;
  while (name[length] && name[length] != ' ' && name[length] != '.') ++length;
  length = utf8::prefixBytes(name, length, kMaxCharacters);
  utf8::copy(out, size, name, name + length);

  const size_t n = strlen(out);
  if (utf8::count(out, n) <= 3) return;
  // Capitals without a single lower-case letter are hard to read on a small
  // display; the first character keeps its case.
  for (size_t i = 0; i < n; ++i) {
    if (out[i] >= 'a' && out[i] <= 'z') return;
  }
  for (size_t i = 1; i < n; ++i) {
    if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
  }
}

}  // namespace oplabel
