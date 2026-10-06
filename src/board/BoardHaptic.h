#pragma once

// The Haptic capability on the board: the pattern player of src/hal/HapticPattern.h firing effects of the
// DRV2605. The motor gets its supply only for the time it is used: the first pulse switches the rail on and
// starts the driver, and two seconds after the last pulse the rail is switched off again, because the
// driver draws current while it idles and the motor is used a few times a day.

#include "../hal/Clock.h"
#include "../hal/Haptic.h"
#include "../hal/HapticPattern.h"
#include "Delay.h"
#include "Drv2605.h"
#include "Xl9555.h"

namespace board {

constexpr uint32_t kHapticSupplyIdleMs = 2000;
// The driver needs a moment after its supply came up before it answers.
constexpr uint32_t kHapticStartMs = 5;

class BoardHaptic : public Haptic {
public:
  BoardHaptic(Drv2605& driver, Xl9555& expander, Clock& clock, Delay& delay)
      : driver_(driver), expander_(expander), clock_(clock), delay_(delay) {}

  void play(HapticPattern pattern) override;
  void service() override;
  bool busy() const override { return player_.busy(); }

private:
  bool powerUp();
  void powerDown();

  Drv2605& driver_;
  Xl9555& expander_;
  Clock& clock_;
  Delay& delay_;
  HapticPlayer player_;
  bool powered_ = false;
  uint64_t lastPulseMs_ = 0;
};

}  // namespace board
