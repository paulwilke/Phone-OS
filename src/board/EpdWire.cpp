#include "EpdWire.h"

#include "PinMap.h"

namespace board {

void EpdWire::begin() {
  gpio_.setMode(pin::kDisplayCs, Gpio::Mode::Output);
  gpio_.write(pin::kDisplayCs, true);
  gpio_.setMode(pin::kDisplayDc, Gpio::Mode::Output);
  gpio_.setMode(pin::kDisplayReset, Gpio::Mode::Output);
  gpio_.write(pin::kDisplayReset, true);
  gpio_.setMode(pin::kDisplayBusy, Gpio::Mode::Input);
}

void EpdWire::reset(uint32_t lowMs, uint32_t highMs) {
  gpio_.write(pin::kDisplayReset, false);
  delay_.delayMs(lowMs);
  gpio_.write(pin::kDisplayReset, true);
  delay_.delayMs(highMs);
}

void EpdWire::command(uint8_t code) {
  gpio_.write(pin::kDisplayDc, false);
  gpio_.write(pin::kDisplayCs, false);
  spi_.write(&code, 1);
  gpio_.write(pin::kDisplayCs, true);
}

void EpdWire::data(const uint8_t* bytes, size_t len) {
  if (len == 0) return;
  gpio_.write(pin::kDisplayDc, true);
  gpio_.write(pin::kDisplayCs, false);
  spi_.write(bytes, len);
  gpio_.write(pin::kDisplayCs, true);
}

bool EpdWire::waitIdle(uint32_t timeoutMs, bool busyIsHigh) {
  for (uint32_t waited = 0; waited < timeoutMs; ++waited) {
    if (gpio_.read(pin::kDisplayBusy) != busyIsHigh) return true;
    delay_.delayMs(1);
  }
  return gpio_.read(pin::kDisplayBusy) != busyIsHigh;
}

}  // namespace board
