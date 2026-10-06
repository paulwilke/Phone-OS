#pragma once

// A UTC date and time as Unix time, without <ctime> and without the host's time zone: the satellite
// receiver's sentences carry a date and a time and nothing else.

#include <cstdint>

namespace civil {

// Days since 1970-01-01 (the algorithm of Howard Hinnant).
inline int64_t daysFromCivil(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int yoe = year - era * 400;
  const int doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + doe - 719468;
}

// Unix time of a UTC date and time; 0 for values that cannot exist. A second of 60 is a leap second.
inline uint32_t toEpoch(int year, int month, int day, int hour, int minute, int second) {
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 ||
      minute > 59 || second < 0 || second > 60)
    return 0;
  const int64_t t = daysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
  return t < 0 || t > 0xFFFFFFFFLL ? 0 : (uint32_t)t;
}

}  // namespace civil
