#pragma once

// A position from the satellite receiver. Plain data, so that the logic modules can
// use the type without the driver that fills it.

#include <cstdint>

struct GnssFix {
  bool valid = false;
  double lat = 0, lon = 0;
  float hdop = 99.0f;  // horizontal dilution of precision: smaller is better
  uint8_t sats = 0;
  uint32_t utc = 0;  // Unix time of the fix, 0 = unknown
};
