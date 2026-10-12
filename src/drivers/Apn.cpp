#include "ApnTable.h"

#include <cstring>

namespace apn {

namespace {

const Entry* lookup(unsigned mcc, unsigned mnc, unsigned digits) {
  size_t low = 0, high = kTableSize;
  while (low < high) {
    const size_t mid = (low + high) / 2;
    const Entry& e = kTable[mid];
    if (e.mcc == mcc && e.mnc == mnc && e.mncDigits == digits) return &e;
    const bool before = e.mcc < mcc || (e.mcc == mcc && (e.mnc < mnc || (e.mnc == mnc && e.mncDigits < digits)));
    if (before) low = mid + 1;
    else high = mid;
  }
  return nullptr;
}

}  // namespace

const Entry* find(const char* imsi) {
  if (!imsi) return nullptr;
  const size_t len = strlen(imsi);
  if (len < 6 || len > 15) return nullptr;
  for (size_t i = 0; i < len; ++i)
    if (imsi[i] < '0' || imsi[i] > '9') return nullptr;
  const auto digit = [&](size_t i) { return (unsigned)(imsi[i] - '0'); };
  const unsigned mcc = digit(0) * 100 + digit(1) * 10 + digit(2);
  if (len >= 7) {
    const Entry* three = lookup(mcc, digit(3) * 100 + digit(4) * 10 + digit(5), 3);
    if (three) return three;
  }
  return lookup(mcc, digit(3) * 10 + digit(4), 2);
}

}  // namespace apn
