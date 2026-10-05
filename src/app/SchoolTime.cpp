#include "SchoolTime.h"

#include <cstdio>

bool SchoolTime::active(int* endMinute) {
  const uint32_t now = clock_.epochUtc();
  if (now < liftedUntil_) return false;
  return profile_.schoolTime(now, clock_.tzOffsetMinutes(), endMinute);
}

bool SchoolTime::lift() {
  const uint32_t now = clock_.epochUtc();
  int endMinute = 0;
  if (!profile_.schoolTime(now, clock_.tzOffsetMinutes(), &endMinute)) return false;

  // Seconds from now to the end minute, counted on the wall clock. The window may end after
  // midnight; the end minute itself is already free, so the lift ends exactly there and not up to
  // a minute later.
  const int64_t local = static_cast<int64_t>(now) + static_cast<int64_t>(clock_.tzOffsetMinutes()) * 60;
  const int64_t secondOfDay = ((local % 86400) + 86400) % 86400;
  int64_t rest = (static_cast<int64_t>(endMinute) * 60 - secondOfDay) % 86400;
  if (rest <= 0) rest += 86400;
  liftedUntil_ = now + static_cast<uint32_t>(rest);

  char line[80];
  snprintf(line, sizeof line, "Quiet hours lifted by emergency until %02d:%02d", endMinute / 60, endMinute % 60);
  log_.log(line);
  return true;
}
