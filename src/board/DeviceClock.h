#pragma once

// The Clock capability on the device. The board has no real-time clock: after a start without a network the time
// is unknown, and it becomes known when the modem delivers the network time or the receiver a position's
// time (board/TimeSync.h decides which one to believe).
//
// Uptime is the processor's 32-bit millisecond counter extended to 64 bits: it never goes back and does not
// wrap in any lifetime. The extension takes the difference to the last reading as a signed 32-bit number, so a
// wrap is an ordinary step, and a counter that is read less often than every 24 days would take a long
// gap for a step backwards (the main loop reads it continuously). A reading that is smaller than the last by
// that rule holds the uptime where it was.
//
// The epoch is 0 while it is not known and is never guessed; a date before 2020 is refused (a modem whose own
// clock never synchronised reports 1970 or 2004). Once set it advances with the uptime, and setting it does not
// move the uptime.

#include <cstdint>

#include "../hal/Clock.h"
#include "MillisSource.h"

namespace board {

class DeviceClock : public Clock {
public:
  explicit DeviceClock(MillisSource& source) : source_(source), last32_(source.millis32()) {}

  uint64_t uptimeMillis() override;
  uint32_t epochUtc() override;
  int tzOffsetMinutes() override { return tz_; }
  bool setEpochUtc(uint32_t epoch) override;
  bool setTzOffsetMinutes(int minutes) override;

private:
  MillisSource& source_;
  uint32_t last32_;
  uint64_t uptime_ = 0;
  bool known_ = false;
  uint32_t epochAtSet_ = 0;
  uint64_t uptimeAtSet_ = 0;
  int tz_ = 0;
};

}  // namespace board
