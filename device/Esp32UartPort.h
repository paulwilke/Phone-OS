#pragma once

// A serial port of the processor behind the UartPort seam. The pins are given from the processor's side (the
// receive pin is the one the other chip transmits on); board/PinMap.h names them that way for both uses.

#include <Arduino.h>

#include <board/UartPort.h>

class Esp32UartPort : public UartPort {
public:
  Esp32UartPort(HardwareSerial& serial, uint32_t baud, uint8_t rxPin, uint8_t txPin)
      : serial_(serial), baud_(baud), rx_(rxPin), tx_(txPin) {}

  void begin() { serial_.begin(baud_, SERIAL_8N1, rx_, tx_); }

  size_t write(const uint8_t* data, size_t len) override { return serial_.write(data, len); }
  int read() override { return serial_.available() ? serial_.read() : -1; }

private:
  HardwareSerial& serial_;
  uint32_t baud_;
  uint8_t rx_;
  uint8_t tx_;
};
