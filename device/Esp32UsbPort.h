#pragma once

// The processor's own USB port (the console) behind the UartPort seam. Output that nobody takes is dropped
// (`Serial.setTxTimeoutMs(0)` in setup), so the firmware never waits for a computer.

#include <Arduino.h>

#include <board/UartPort.h>

class Esp32UsbPort : public UartPort {
public:
  size_t write(const uint8_t* data, size_t len) override { return Serial.write(data, len); }
  int read() override { return Serial.available() ? Serial.read() : -1; }
};
