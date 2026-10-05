#pragma once

// Who wants the satellite receiver on. **The receiver does not run all the time**: it costs
// battery, and a device that locates in secret is what this one must not be. There are exactly
// six reasons to switch it on, and each is its own switch:
//
//   StartupFix    once after start: the time, when the network gives none, and a first position
//   SosChoice     the SOS button was pressed and the choice is open: the search starts at once
//   SosPing       an SOS ping is searching
//   RemoteQuery   a parent's location query is being answered
//   PositionPage  the position page is open
//   TestPage      the self-test page is open
//
// The receiver is on while **any** reason holds. That is the point of keeping them apart: two
// sources that wanted it once shared one switch, and whichever finished first took it from
// the other (the position page closing while a parent's query was still waiting for a fix).
//
// While the receiver runs, every valid fix is remembered with the time it was seen, whoever
// asked for it, so the last known position outlives the receiver. The start-up fix ends on
// its own: with the first usable fix (HDOP up to kUsableHdop), with any valid fix after
// kAcceptAnySeconds, or without a result after kStartupFixSeconds.

#include <cstdint>

#include "../hal/Clock.h"
#include "../hal/GnssReceiver.h"
#include "../hal/Power.h"
#include "Logger.h"

class Positioning {
public:
  enum class Reason : uint8_t { StartupFix, SosChoice, SosPing, RemoteQuery, PositionPage, TestPage, Count };

  static constexpr uint32_t kStartupFixSeconds = 180;
  static constexpr uint32_t kAcceptAnySeconds = 60;
  static constexpr float kUsableHdop = 5.0f;

  Positioning(Clock& clock, Power& power, GnssReceiver& receiver, Logger& log)
      : clock_(clock), power_(power), receiver_(receiver), log_(log) {}

  // Holds or releases a reason. Holding it twice is the same as once. (StartupFix is started
  // with startStartupFix() and ends by itself.)
  void want(Reason reason, bool on);
  bool wants(Reason reason) const { return wanted_[static_cast<size_t>(reason)]; }

  // Asks for the one-time start-up fix.
  void startStartupFix();

  // Is the receiver powered? (The question is put to the power capability, not remembered.)
  bool active();

  // Call from the main loop: remembers fixes, ends the start-up fix, and tries again to
  // switch the receiver when a switch failed.
  void poll();

  // The last valid fix, even when the receiver is off long since. `ageSeconds` is how long ago it
  // was seen; false while there has been none.
  bool lastKnownFix(GnssFix& out, uint32_t* ageSeconds = nullptr);

  // The receiver's current answer, for the SOS ping (false when it is off).
  bool readFix(GnssFix& out) { return receiver_.readFix(out); }

private:
  void apply();

  Clock& clock_;
  Power& power_;
  GnssReceiver& receiver_;
  Logger& log_;
  bool wanted_[static_cast<size_t>(Reason::Count)] = {};
  uint32_t startupBegan_ = 0;
  bool haveFix_ = false;
  GnssFix last_;
  uint32_t lastAt_ = 0;
};
