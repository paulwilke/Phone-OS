#pragma once

// The vibration driver DRV2605 at 0x5A, with an eccentric-mass motor (ERM) in open loop and the first
// effect library. Its supply is gated by the expander (leg kHapticPower), and **that must be on before
// begin(), or the chip does not answer**: the most common mistake with this part.
//
// The register sequence is the one of the chip's own library (Adafruit_DRV2605::begin() followed by
// selectLibrary(1) and the internal-trigger mode), checked against the data sheet's register map: 13
// register addresses and the two read-modify-write bits (ERM instead of LRA in the feedback register,
// open loop in control 3).

#include <cstdint>

#include "I2cBus.h"

namespace board {

class Drv2605 {
public:
  explicit Drv2605(I2cBus& bus) : bus_(bus) {}

  // False when the chip does not answer (its supply is off) or a write is refused.
  bool begin();
  // Fires one effect of the library (1 is a short strong click, 15 an alarm of 750 ms).
  bool fire(uint8_t effect);

private:
  I2cBus& bus_;
};

}  // namespace board
