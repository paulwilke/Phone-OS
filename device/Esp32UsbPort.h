#pragma once

// The processor's own USB port (the console) behind the UartPort seam. Output that nobody takes is dropped
// (`Serial.setTxTimeoutMs(0)` in setup), so the firmware never waits for a computer: write() returns what fitted
// and hostConnected() tells the console whether waiting for the rest can ever help.

#include <Arduino.h>

#include <board/UartPort.h>

class Esp32UsbPort : public UartPort {
public:
  size_t write(const uint8_t* data, size_t len) override { return Serial.write(data, len); }
  // True only while a computer has the port open (the HWCDC connection state), not merely while a cable is plugged.
  bool hostConnected() override { return (bool)Serial; }
  int read() override { return Serial.available() ? Serial.read() : -1; }
};
