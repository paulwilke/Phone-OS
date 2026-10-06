#pragma once

// The seam between the chip drivers and the I2C bus: the only way they touch it. The device implements it on
// the processor's bus, the host tests on a scripted bus (testing/fakes/FakeI2cBus.h). Addresses are 7 bit.
// Every call returns false when the chip does not acknowledge, so a missing chip is an answer and not a hang.

#include <cstddef>
#include <cstdint>

class I2cBus {
public:
  virtual ~I2cBus() = default;

  // One write transaction of len bytes (len 0 only addresses the chip, which is how a probe works).
  virtual bool write(uint8_t address, const uint8_t* data, size_t len) = 0;
  // One read transaction of len bytes.
  virtual bool read(uint8_t address, uint8_t* out, size_t len) = 0;
  // A write followed by a read with a repeated start between them (no stop), as register reads need.
  virtual bool writeRead(uint8_t address, const uint8_t* data, size_t len, uint8_t* out, size_t outLen) = 0;

  // Does a chip answer at this address?
  bool probe(uint8_t address) { return write(address, nullptr, 0); }

  // Register access of the common kind: one register byte, then data.
  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    const uint8_t frame[2] = {reg, value};
    return write(address, frame, 2);
  }
  bool readRegister(uint8_t address, uint8_t reg, uint8_t& value) { return writeRead(address, &reg, 1, &value, 1); }
  // Little-endian 16 bit register pair, as the fuel gauge has them.
  bool readRegister16(uint8_t address, uint8_t reg, uint16_t& value) {
    uint8_t b[2];
    if (!writeRead(address, &reg, 1, b, 2)) return false;
    value = (uint16_t)(b[0] | (b[1] << 8));
    return true;
  }
};
