#pragma once

// The seam to the processor's own pins: a level out, a level in. Pin numbers are those of board/PinMap.h.
// The expander's legs are not reached through this, but through board/Xl9555.h.

#include <cstdint>

class Gpio {
public:
  enum class Mode : uint8_t { Input, InputPullUp, Output };

  virtual ~Gpio() = default;

  virtual void setMode(uint8_t pin, Mode mode) = 0;
  virtual void write(uint8_t pin, bool high) = 0;
  virtual bool read(uint8_t pin) = 0;
};
