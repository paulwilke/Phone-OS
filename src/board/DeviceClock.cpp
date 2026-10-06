#include "DeviceClock.h"

namespace board {

uint64_t DeviceClock::uptimeMillis() {
  const uint32_t now = source_.millis32();
  const int32_t step = (int32_t)(now - last32_);
  if (step > 0) {
    uptime_ += (uint32_t)step;
    last32_ = now;
  }
  // A step of zero changes nothing, and a negative one is a counter that went back: the uptime holds, and the
  // counter's own value is not taken over, so the next forward step counts from the highest reading.
  return uptime_;
}

uint32_t DeviceClock::epochUtc() {
  if (!known_) return 0;
  return (uint32_t)(epochAtSet_ + (uptimeMillis() - uptimeAtSet_) / 1000);
}

bool DeviceClock::setEpochUtc(uint32_t epoch) {
  if (epoch == 0) {
    known_ = false;
    return true;
  }
  if (epoch < kEarliestEpoch) return false;
  known_ = true;
  epochAtSet_ = epoch;
  uptimeAtSet_ = uptimeMillis();
  return true;
}

bool DeviceClock::setTzOffsetMinutes(int minutes) {
  if (minutes < kMinOffsetMinutes || minutes > kMaxOffsetMinutes) return false;
  tz_ = minutes;
  return true;
}

}  // namespace board
