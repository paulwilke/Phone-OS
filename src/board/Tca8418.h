#pragma once

// The keyboard matrix controller TCA8418 at 0x34. It scans the matrix itself and queues the key events in a
// FIFO; this class starts it and hands the events out, one at a time. What a scan code means is the business
// of board/KeyboardDecoder.h.
//
// The chip stays powered across a reset of the processor and its reset line is on the expander, so the
// scanner may still run from the earlier session: the start stops it first, empties what it queued, sets
// the matrix and only then lets it scan again. The matrix is 8 rows by 10 columns, 35 positions in use.

#include <cstdint>

#include "Delay.h"
#include "I2cBus.h"

namespace board {

struct KeyEvent {
  uint8_t code = 0;      // the scan code, 1..80 (the chip numbers the matrix from 1)
  bool pressed = false;  // false: released
};

class Tca8418 {
public:
  Tca8418(I2cBus& bus, Delay& delay) : bus_(bus), delay_(delay) {}

  // False when the chip does not answer or refuses a register write. A keyboard that is not there is
  // not an error of the device: the touch screen and the keys under it still work.
  bool begin();
  bool ready() const { return ready_; }

  // The oldest queued event; false when none is waiting. Clears the interrupt flags after each one, as
  // the chip wants.
  bool next(KeyEvent& out);

private:
  bool drain();

  I2cBus& bus_;
  Delay& delay_;
  bool ready_ = false;
};

}  // namespace board
