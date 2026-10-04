#include "MapLink.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace maplink {
namespace {

constexpr double kMaxMagnitude = 1e9;  // keeps value * 1e5 inside a 64-bit integer
constexpr size_t kDegreesSize = 20;    // "-1000000000.00000" and the terminator

}  // namespace

bool formatDegrees(double value, char* out, size_t size) {
  if (!out || size == 0) return false;
  out[0] = '\0';
  if (!std::isfinite(value) || std::fabs(value) > kMaxMagnitude) return false;

  const bool negative = value < 0;
  const uint64_t scaled = (uint64_t)((negative ? -value : value) * 100000.0 + 0.5);
  char digits[kDegreesSize];
  size_t n = 0;
  // Least significant digit first: five decimals, the point, then the integer part.
  uint64_t rest = scaled;
  for (int i = 0; i < 5; ++i) {
    digits[n++] = (char)('0' + rest % 10);
    rest /= 10;
  }
  digits[n++] = '.';
  do {
    digits[n++] = (char)('0' + rest % 10);
    rest /= 10;
  } while (rest > 0);
  if (negative && scaled != 0) digits[n++] = '-';

  if (n + 1 > size) return false;
  for (size_t i = 0; i < n; ++i) out[i] = digits[n - 1 - i];
  out[n] = '\0';
  return true;
}

Result build(const char* tmpl, double lat, double lon, char* out, size_t size) {
  if (!out || size == 0) return Result::BufferTooSmall;
  out[0] = '\0';
  if (!tmpl || !*tmpl) return Result::NoTemplate;
  if (!std::isfinite(lat) || !std::isfinite(lon) || std::fabs(lat) > 90.0 || std::fabs(lon) > 180.0) {
    return Result::InvalidCoordinates;
  }

  char latText[kDegreesSize], lonText[kDegreesSize];
  formatDegrees(lat, latText, sizeof(latText));
  formatDegrees(lon, lonText, sizeof(lonText));

  size_t n = 0;
  for (const char* p = tmpl; *p;) {
    const char* insert = nullptr;
    if (strncmp(p, "{lat}", 5) == 0) insert = latText;
    else if (strncmp(p, "{lon}", 5) == 0) insert = lonText;
    const size_t len = insert ? strlen(insert) : 1;
    if (n + len + 1 > size) {
      out[0] = '\0';
      return Result::BufferTooSmall;
    }
    if (insert) {
      memcpy(out + n, insert, len);
      p += 5;
    } else {
      out[n] = *p++;
    }
    n += len;
  }
  out[n] = '\0';
  return Result::Ok;
}

const char* defaultTemplate() { return "https://maps.google.com/?q={lat},{lon}"; }

}  // namespace maplink
