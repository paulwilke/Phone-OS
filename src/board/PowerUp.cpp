#include "PowerUp.h"

#include "PinMap.h"

namespace board {

namespace {
bool pulseReset(Xl9555& expander, Delay& delay, uint8_t leg) {
  if (!expander.set(leg, false)) return false;
  delay.delayMs(kResetLowMs);
  if (!expander.set(leg, true)) return false;
  delay.delayMs(kResetSettleMs);
  return true;
}
}  // namespace

bool powerUp(Xl9555& expander, Gpio& gpio, Delay& delay) {
  const bool expanderUp = expander.begin();

  gpio.setMode(pin::kFrontlight, Gpio::Mode::Output);
  gpio.write(pin::kFrontlight, false);

  bool resets = false;
  if (expanderUp) {
    // Both are attempted even when the first fails: a stuck touch chip must not keep the keyboard in reset.
    const bool touch = pulseReset(expander, delay, leg::kTouchReset);
    const bool keyboard = pulseReset(expander, delay, leg::kKeyboardReset);
    resets = touch && keyboard;
  }

  gpio.setMode(pin::kBootButton, Gpio::Mode::Input);
  gpio.setMode(pin::kSpiMiso, Gpio::Mode::InputPullUp);
  return expanderUp && resets;
}

}  // namespace board
