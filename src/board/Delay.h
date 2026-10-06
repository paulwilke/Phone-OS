#pragma once

// Waiting, as a seam: a start-up sequence that has to hold a reset line for 20 ms must be testable
// without waiting, and a test must be able to read how long the sequence held it.

#include <cstdint>

class Delay {
public:
  virtual ~Delay() = default;
  virtual void delayMs(uint32_t ms) = 0;
};
