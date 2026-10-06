#pragma once

// Capability: the clock. The device keeps a millisecond counter and the network time
// the modem delivers; the simulator uses the computer's clock with a time lapse; tests
// set numbers freely.
//
// Two kinds of time:
//
//   uptime   monotonic since the start. It never goes back and never jumps when the
//            wall-clock time is set. Everything that measures a duration (timeouts,
//            countdowns, idle time) uses it.
//   epoch    Unix time in seconds (UTC), or **0 when it is not known**. The board has
//            no clock of its own: after a start without network the time is unknown
//            until the modem or the receiver delivers it.
//
// **What "unknown" means for every consumer.** Nobody may guess:
//   * quiet hours: no lock (see docs/quiet-hours.md); guessing would be wrong in both
//     directions, and without network the child cannot call anyway;
//   * log lines and messages: written without a time (0), never with 1970;
//   * the time of a position: 0;
//   * the time zone rules do not matter, there is no instant to apply them to.
// A consumer that needs the time and has none carries on without it.

#include <cstdint>

class Clock {
public:
  // The first date a clock may be set to. A modem whose own clock was never
  // synchronised reports 1970 or 2000; setting that would make the device believe it
  // knows the time. Anything earlier than 2020-01-01 is refused.
  static constexpr uint32_t kEarliestEpoch = 1577836800;
  // The first date no source may deliver (2070-01-01): the year of a network time or a satellite sentence has
  // two digits, and a source that counts wrongly lands far beyond any date this device will see. A source
  // that offers such a time is ignored by whoever sets the clock; the clock itself does not check it.
  static constexpr uint32_t kLatestEpoch = 3155760000;
  // Offsets from UTC that exist, in minutes (UTC-12:00 to UTC+14:00).
  static constexpr int kMinOffsetMinutes = -720;
  static constexpr int kMaxOffsetMinutes = 840;

  virtual ~Clock() = default;

  // Milliseconds since the start; 64 bit, so it does not wrap in any lifetime.
  virtual uint64_t uptimeMillis() = 0;
  uint32_t uptimeSeconds() { return (uint32_t)(uptimeMillis() / 1000); }

  // Unix time (UTC); 0 = not known. Once set it advances with the uptime.
  virtual uint32_t epochUtc() = 0;
  // Offset of local time from UTC in minutes, east positive. Always a valid offset;
  // 0 until it is set.
  virtual int tzOffsetMinutes() = 0;

  // The time became known (network time, receiver). Returns false and changes nothing
  // for a value between 1 and kEarliestEpoch - 1. 0 makes the time unknown again.
  virtual bool setEpochUtc(uint32_t epoch) = 0;
  // The zone offset became known. False and unchanged outside the range above.
  virtual bool setTzOffsetMinutes(int minutes) = 0;
};
