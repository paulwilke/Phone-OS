#pragma once

// The XL9555 I/O expander: it switches the supply of the modem, the radio, the satellite receiver, the motion
// sensor's rail, the vibration driver and the amplifier, and holds the resets of touch and keyboard. Without
// it half the board does not answer, so it is the first chip started (board/PowerUp.h).
//
// The chip keeps a copy of the output levels here (its interrupt line is not connected and the outputs are
// never read back), so changing one leg writes the whole port again from that copy.

#include <cstdint>

#include "I2cBus.h"

namespace board {

class Xl9555 {
public:
  explicit Xl9555(I2cBus& bus) : bus_(bus) {}

  // Looks for the chip, makes every leg an output and puts the legs into the boot state of the pin map.
  // False when the chip is silent or a write is refused; the expander is then not ready and set() fails.
  bool begin();
  bool ready() const { return ready_; }

  // Sets one leg (see board/PinMap.h, namespace leg). False when the expander is not ready, the leg does
  // not exist or the write was refused; the remembered level is unchanged then.
  bool set(uint8_t leg, bool high);
  // The level last written to a leg; false for an unknown leg.
  bool level(uint8_t leg) const;

private:
  I2cBus& bus_;
  bool ready_ = false;
  uint8_t port_[2] = {0, 0};
};

}  // namespace board
