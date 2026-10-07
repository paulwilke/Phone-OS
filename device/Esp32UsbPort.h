#pragma once

// The processor's own USB port (the console) behind the UartPort seam. Output that nobody takes is dropped
// (`Serial.setTxTimeoutMs(0)` in setup), so the firmware never waits for a computer: write() returns what fitted
// and hostConnected() tells the console whether waiting for the rest can ever help.

#include <Arduino.h>

#include <board/UartPort.h>

class Esp32UsbPort : public UartPort {
public:
  // Never more than fits: with the transmit timeout at zero, HWCDC::write takes a chunk larger than the free room,
  // reports it all as written, silently drops the rest and marks the port as not connected (seen on the device
  // 2026-10-07: a log answer cut off after about 3.6 KB). Offering only the free room keeps it on its one plain path,
  // so a short count is honest and Console::say can wait for the rest.
  size_t write(const uint8_t* data, size_t len) override {
    const int room = Serial.availableForWrite();
    if (room <= 0) return 0;
    return Serial.write(data, len < (size_t)room ? len : (size_t)room);
  }
  // True only while a computer has the port open (the HWCDC connection state), not merely while a cable is plugged.
  bool hostConnected() override { return (bool)Serial; }
  int read() override { return Serial.available() ? Serial.read() : -1; }
};
