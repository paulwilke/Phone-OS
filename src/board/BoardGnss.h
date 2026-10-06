#pragma once

// The GnssReceiver capability on the board: the NMEA stream of the receiver's UART through the parser. The
// receiver's supply is the Positioning domain of BoardPower; this class follows it.
//
//   * switched off: readFix() is false and leaves the caller's fix alone;
//   * every switch-on, seen or not (the switch counter of BoardPower says), starts afresh: the parser is
//     reset and what the UART still held from the earlier run is thrown away, so a position from yesterday is
//     never returned as if it were current;
//   * a fix is only as old as its last position sentence: the receiver sends one a second, and when none has
//     come for ten seconds (it is silent, or the wire is) the fix is reported as not valid.
//
// Never blocks: it takes what is waiting at the UART, at most a few thousand bytes a call.

#include <cstdint>

#include "../drivers/NmeaParser.h"
#include "../hal/Clock.h"
#include "../hal/GnssReceiver.h"
#include "BoardPower.h"
#include "UartPort.h"

namespace board {

constexpr uint32_t kFixStaleMs = 10000;
constexpr int kMaxBytesPerRead = 4096;

class BoardGnss : public GnssReceiver {
public:
  BoardGnss(BoardPower& power, UartPort& uart, Clock& clock) : power_(power), uart_(uart), clock_(clock) {}

  bool readFix(GnssFix& out) override;

private:
  BoardPower& power_;
  UartPort& uart_;
  Clock& clock_;
  NmeaParser parser_;
  bool wasOn_ = false;
  uint32_t seenSwitchOns_ = 0;
  uint32_t seenSentences_ = 0;
  uint64_t lastSentenceAtMs_ = 0;
};

}  // namespace board
