#pragma once

// A clock that only moves when the test says so.
//
// Scripting: advance() / advanceSeconds() move the uptime. The epoch is whatever the
// software set through setEpochUtc() and advances with the uptime; a test that wants
// the time known from the start calls setEpochUtc() itself.

#include <hal/Clock.h>

class FakeClock : public Clock {
public:
  uint64_t uptimeMillis() override { return ms_; }
  uint32_t epochUtc() override { return known_ ? (uint32_t)(epochAtSet_ + (ms_ - msAtSet_) / 1000) : 0; }
  int tzOffsetMinutes() override { return tz_; }

  bool setEpochUtc(uint32_t epoch) override {
    if (epoch == 0) {
      known_ = false;
      return true;
    }
    if (epoch < kEarliestEpoch) return false;
    known_ = true;
    epochAtSet_ = epoch;
    msAtSet_ = ms_;
    return true;
  }
  bool setTzOffsetMinutes(int minutes) override {
    if (minutes < kMinOffsetMinutes || minutes > kMaxOffsetMinutes) return false;
    tz_ = minutes;
    return true;
  }

  // Scripting.
  void advance(uint64_t ms) { ms_ += ms; }
  void advanceSeconds(uint32_t seconds) { ms_ += (uint64_t)seconds * 1000; }

private:
  uint64_t ms_ = 0;
  bool known_ = false;
  uint32_t epochAtSet_ = 0;
  uint64_t msAtSet_ = 0;
  int tz_ = 0;
};
