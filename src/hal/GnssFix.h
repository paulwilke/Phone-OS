#pragma once

// A position from the satellite receiver. Plain data, so that the logic modules can
// use the type without the driver that fills it.
//
// A fix is `valid` only when the receiver reports a position solution. A valid fix
// has latitude within ±90 and longitude within ±180 (finite numbers) and a `hdop`
// that is not negative. Of two valid fixes the one with the smaller `hdop` is the
// more accurate; the number of satellites is information for the display and says
// nothing about accuracy by itself. An invalid fix carries no usable numbers at all.

#include <cstdint>

struct GnssFix {
  bool valid = false;
  double lat = 0, lon = 0;
  float hdop = 99.0f;  // horizontal dilution of precision: the smaller, the better (about 5 m per unit)
  uint8_t sats = 0;    // satellites in use
  uint32_t utc = 0;    // Unix time of the fix, 0 = unknown
};
