#pragma once

// The I2C seam on the processor's bus. 100 kHz, which every chip on the board tolerates (set by the caller
// that starts Wire). A failed transfer is false, never a hang: a missing chip is an answer.

#include <Wire.h>

#include <board/I2cBus.h>

class Esp32I2cBus : public I2cBus {
public:
  bool write(uint8_t address, const uint8_t* data, size_t len) override {
    Wire.beginTransmission(address);
    if (len) Wire.write(data, len);
    return Wire.endTransmission(true) == 0;
  }

  bool read(uint8_t address, uint8_t* out, size_t len) override { return receive(address, out, len); }

  bool writeRead(uint8_t address, const uint8_t* data, size_t len, uint8_t* out, size_t outLen) override {
    Wire.beginTransmission(address);
    if (len) Wire.write(data, len);
    if (Wire.endTransmission(false) != 0) return false;
    return receive(address, out, outLen);
  }

private:
  static bool receive(uint8_t address, uint8_t* out, size_t len) {
    if (Wire.requestFrom((uint8_t)address, (size_t)len) != len) return false;
    for (size_t i = 0; i < len; ++i) out[i] = (uint8_t)Wire.read();
    return true;
  }
};
