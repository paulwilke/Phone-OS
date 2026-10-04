#pragma once

// Vibration patterns as data. By the rhythm in a trouser pocket the child should tell
// **what** happened without looking: a call, a message, SOS sent, an error.
//
// A pattern is a list of steps: fire an effect, then wait. The pattern is played
// without blocking by HapticPlayer::poll(); the driver only has to be able to fire one
// single effect. The patterns can thus be tested without a device.
//
// The effect numbers are those of the effect library of the motor driver on the device
// (1 = a short strong click, 15 = an alarm of 750 ms). **Not felt on the device yet**,
// only taken from the data sheet; whether the rhythms can be told apart through a
// jacket pocket only everyday use will show. A driver for other hardware maps the two
// effects "short" and "long" to what it has.

#include <cstdint>

enum class HapticPattern : uint8_t {
  Click,    // one short pulse: test page, confirmation
  Ring,     // one long pulse; the application repeats it while it rings
  Message,  // two short: a message has arrived
  Sent,     // three short: the SOS ping is out
  Failed    // short, then long: something went wrong (call, SOS, SMS)
};

struct HapticStep {
  uint8_t effect;    // effect number
  uint16_t afterMs;  // wait this long before the next step
};

struct HapticSequence {
  const HapticStep* steps;
  uint8_t count;
};

inline HapticSequence hapticSequence(HapticPattern p) {
  // Pauses of 150 ms: with less, two pulses blur into one, and the motor needs about
  // 100 ms to come to a standstill.
  static const HapticStep kClick[] = {{1, 0}};
  static const HapticStep kRing[] = {{15, 0}};
  static const HapticStep kMessage[] = {{1, 150}, {1, 0}};
  static const HapticStep kSent[] = {{1, 150}, {1, 150}, {1, 0}};
  static const HapticStep kFailed[] = {{1, 250}, {15, 0}};
  switch (p) {
    case HapticPattern::Ring:    return {kRing, 1};
    case HapticPattern::Message: return {kMessage, 2};
    case HapticPattern::Sent:    return {kSent, 3};
    case HapticPattern::Failed:  return {kFailed, 2};
    case HapticPattern::Click:
    default:                     return {kClick, 1};
  }
}

// Plays a pattern step by step. A new pattern replaces a running one: the newest is
// what the child needs to know.
class HapticPlayer {
public:
  void start(HapticPattern p, uint32_t nowMs) {
    seq_ = hapticSequence(p);
    next_ = 0;
    due_ = nowMs;
    active_ = true;
  }

  // True: fire `effect` now. Comparing differences keeps it right when the millisecond
  // counter wraps (after 49 days).
  bool poll(uint32_t nowMs, uint8_t& effect) {
    if (!active_ || (int32_t)(nowMs - due_) < 0) return false;
    const HapticStep& s = seq_.steps[next_++];
    effect = s.effect;
    due_ = nowMs + s.afterMs;
    if (next_ >= seq_.count) active_ = false;
    return true;
  }

  // Is something still to come? While it is, the processor must not sleep: the pauses
  // between the pulses are shorter than any sleep.
  bool busy() const { return active_; }

private:
  HapticSequence seq_ = {nullptr, 0};
  uint8_t next_ = 0;
  uint32_t due_ = 0;
  bool active_ = false;
};
