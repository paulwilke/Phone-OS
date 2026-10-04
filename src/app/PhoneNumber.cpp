#include "PhoneNumber.h"

#include <cstring>

namespace phone {
namespace {

constexpr size_t kMaxInputDigits = 2 * kMaxDigits;  // generous: "00", a "(0)" and slack

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isSeparator(char c) {
  return c == ' ' || c == '-' || c == '/' || c == '(' || c == ')' || c == '.';
}

// Validates a configured country code and returns its digits ("49"), or nullptr.
const char* countryDigits(const char* countryCode, char* buffer, size_t bufferSize) {
  const char* cc = (countryCode && *countryCode) ? countryCode : kDefaultCountryCode;
  if (*cc == '+') ++cc;
  const size_t len = strlen(cc);
  if (len == 0 || len > 3 || len + 1 > bufferSize || cc[0] == '0') return nullptr;
  for (size_t i = 0; i < len; ++i) {
    if (!isDigit(cc[i])) return nullptr;
  }
  memcpy(buffer, cc, len + 1);
  return buffer;
}

}  // namespace

bool normalize(const char* input, const char* countryCode, char* out, size_t outSize) {
  if (!out || outSize == 0) return false;
  out[0] = '\0';
  if (!input) return false;

  char digits[kMaxInputDigits + 1];
  size_t n = 0;
  bool plus = false;
  for (const char* p = input; *p; ++p) {
    if (*p == '+') {
      if (plus || n > 0) return false;
      plus = true;
    } else if (isDigit(*p)) {
      if (n == kMaxInputDigits) return false;
      digits[n++] = *p;
    } else if (*p == '(' && strncmp(p, "(0)", 3) == 0 && n > 0 &&
               (plus ? n <= 3 : (n <= 5 && strncmp(digits, "00", 2) == 0))) {
      // "+49 (0)170 ...": the 0 in brackets is the national prefix that does not
      // belong to the international form. It can only appear right after the
      // country code (at most three digits, after "00" five), so it is ignored
      // only there.
      p += 2;
    } else if (!isSeparator(*p)) {
      return false;  // letters: a sender name or a service number, not a phone number
    }
  }
  digits[n] = '\0';
  if (n == 0) return false;

  char result[kNormalizedSize + 1];
  size_t len = 0;
  bool international = true;
  const char* subscriber;  // digits after the prefix handling
  if (plus) {
    result[len++] = '+';
    subscriber = digits;
  } else if (n >= 2 && digits[0] == '0' && digits[1] == '0') {
    result[len++] = '+';
    subscriber = digits + 2;
  } else if (digits[0] == '0') {
    // National form: the leading 0 is replaced by the country code.
    char cc[8];
    const char* country = countryDigits(countryCode, cc, sizeof(cc));
    if (!country) return false;
    result[len++] = '+';
    const size_t ccLen = strlen(country);
    memcpy(result + len, country, ccLen);
    len += ccLen;
    subscriber = digits + 1;
  } else {
    // No prefix at all: a short code such as 110 or 112 stays as it is.
    international = false;
    subscriber = digits;
  }

  const size_t rest = strlen(subscriber);
  if (rest == 0) return false;
  // Country codes never start with 0, so "+0..." and "000..." are not numbers.
  if (international && len == 1 && subscriber[0] == '0') return false;
  if (len - (international ? 1 : 0) + rest > kMaxDigits) return false;
  if (len + rest + 1 > outSize) return false;

  memcpy(result + len, subscriber, rest + 1);
  memcpy(out, result, len + rest + 1);
  return true;
}

bool same(const char* a, const char* b, const char* countryCode) {
  char na[kNormalizedSize], nb[kNormalizedSize];
  if (!normalize(a, countryCode, na, sizeof(na))) return false;
  if (!normalize(b, countryCode, nb, sizeof(nb))) return false;
  return strcmp(na, nb) == 0;
}

}  // namespace phone
