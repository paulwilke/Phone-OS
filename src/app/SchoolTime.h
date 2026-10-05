#pragma once

// School time as the device experiences it: the profile's windows on the clock, plus the one
// exception — an emergency lifts the lock until the end of the school time that is running.
//
// Why the exception exists: a child who sends an emergency from a locked phone could neither be
// called back nor call. So the last tap of the SOS override (Emergency, SosFlow) lifts the lock
// for the rest of that school time, and only that: the next window locks again.
//
// Without a known time of day there is no school time (see Profile::schoolTime), and so no lift.

#include <cstdint>

#include "../hal/Clock.h"
#include "Logger.h"
#include "Profile.h"

class SchoolTime {
public:
  SchoolTime(Clock& clock, const Profile& profile, Logger& log) : clock_(clock), profile_(profile), log_(log) {}

  // Is the lock on right now? With `endMinute` the minute of the day school time ends. False
  // while an emergency has lifted the lock; `endMinute` is then left alone.
  bool active(int* endMinute = nullptr);

  // Lifts the lock until the end of the school time now running. False, and nothing changes, when
  // no school time is running (there is nothing to lift). Logged: it must be provable afterwards.
  bool lift();

  // Epoch until which the lock is lifted; 0 = not lifted.
  uint32_t liftedUntil() const { return liftedUntil_; }

private:
  Clock& clock_;
  const Profile& profile_;
  Logger& log_;
  uint32_t liftedUntil_ = 0;
};
