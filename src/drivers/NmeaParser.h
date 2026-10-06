#pragma once

// The reader of the satellite receiver's NMEA sentences. Plain C++: bytes in, position out.
//
// Two kinds of sentence are used, whatever the constellation ($GP, $GN, $GL, $GA …):
//   RMC  the date, the time, whether the position is valid, the position  -> makes the fix
//   GGA  the number of satellites in use and the HDOP                      -> the quality of the fix
// A sentence with a wrong checksum is thrown away, and a fix is valid only when RMC says so.
//
// What the capability promises (hal/GnssFix.h) is kept here, at the source, so that nothing above has to
// check it again: a valid fix has a latitude within ±90, a longitude within ±180, finite numbers and an
// HDOP that is finite and not negative; an invalid fix carries no numbers at all. An HDOP that is missing,
// not a number ("nan"), infinite, zero or negative is *unknown* and reads 99, the worst value, never NaN:
// a NaN compares false with everything, and a fix whose quality cannot be compared is silently dropped
// further up.

#include <cstddef>
#include <cstdint>

#include "../hal/GnssFix.h"

class NmeaParser {
public:
  static constexpr size_t kMaxSentence = 100;
  static constexpr float kUnknownHdop = 99.0f;

  // Feeds one byte. True when it completed a sentence with a correct checksum (not necessarily one that
  // carries a position).
  bool feed(char c);

  // The last known position; `valid` is false until there was one and whenever the receiver says it has
  // none, and the numbers are then at their defaults.
  const GnssFix& fix() const { return fix_; }

  // How many RMC sentences with a correct checksum have been read since reset(), valid or not. The
  // receiver sends one a second, so a counter that stands still means it is silent.
  uint32_t positionSentences() const { return rmcCount_; }

  // Sentences thrown away (checksum, too long, a position that cannot exist).
  uint32_t rejected() const { return rejected_; }

  void reset();

private:
  void handleSentence(char* sentence);
  void handleRmc(char* fields[], int count);
  void handleGga(char* fields[], int count);

  char buf_[kMaxSentence + 1] = {};
  size_t len_ = 0;
  bool collecting_ = false;
  bool overflow_ = false;
  uint32_t rejected_ = 0;
  uint32_t rmcCount_ = 0;

  GnssFix fix_;
  uint8_t sats_ = 0;
  float hdop_ = kUnknownHdop;
};
