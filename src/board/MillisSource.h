#pragma once

// The seam to the processor's millisecond counter: 32 bit, wrapping after about 49 days. DeviceClock makes the
// 64-bit uptime of the Clock capability out of it.

#include <cstdint>

class MillisSource {
public:
  virtual ~MillisSource() = default;
  virtual uint32_t millis32() = 0;
};
