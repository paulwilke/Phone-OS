#pragma once

// The wire to the panel: SPI for the bytes, and the four lines around it: chip select, data/command (low is a
// command byte, high is data), reset and busy. All the panel's traffic goes through here, so a test records
// it (testing/fakes/FakeSpiPort.h) and compares it with the documented sequence.

#include <cstdint>
#include <vector>

#include "Delay.h"
#include "Gpio.h"
#include "SpiPort.h"

namespace board {

class EpdWire {
public:
  EpdWire(SpiPort& spi, Gpio& gpio, Delay& delay) : spi_(spi), gpio_(gpio), delay_(delay) {}

  // Makes the lines outputs/inputs and releases the chip select.
  void begin();
  // Pulses the reset line: low, then high.
  void reset(uint32_t lowMs, uint32_t highMs);
  void command(uint8_t code);
  void data(const uint8_t* bytes, size_t len);
  void data(const std::vector<uint8_t>& bytes) { data(bytes.data(), bytes.size()); }
  // Waits until the panel is no longer busy; false when `timeoutMs` ran out (a panel that hangs must not hang
  // the device with it).
  bool waitIdle(uint32_t timeoutMs, bool busyIsHigh);

private:
  SpiPort& spi_;
  Gpio& gpio_;
  Delay& delay_;
};

}  // namespace board
