#pragma once

// The order in which the board is brought up. It is code and not a comment because the order matters and
// went wrong before: no expander, no supply for modem, radio, receiver and motor; reset lines that are not
// pulsed leave touch and keyboard in an unknown state.
//
//   1. the expander (direction, then the boot state: resets released, modem off)
//   2. the front light off, so the device starts dark
//   3. the touch reset: low for 20 ms, then released and 50 ms to settle
//   4. the keyboard reset: the same
//   5. the boot button as an input, the SPI's MISO with a pull-up so the shared bus does not float
//
// Without an expander only the pins of the processor are set; the chips behind it are then not touched
// and the caller says so.

#include "Delay.h"
#include "Gpio.h"
#include "Xl9555.h"

namespace board {

constexpr uint32_t kResetLowMs = 20;
constexpr uint32_t kResetSettleMs = 50;

// True when the expander answered and the resets were pulsed.
bool powerUp(Xl9555& expander, Gpio& gpio, Delay& delay);

}  // namespace board
