#pragma once

// The pin seam on the Arduino pin functions.

#include <Arduino.h>

#include <board/Gpio.h>

class Esp32Gpio : public Gpio {
public:
  void setMode(uint8_t pin, Mode mode) override {
    pinMode(pin, mode == Mode::Output ? OUTPUT : mode == Mode::InputPullUp ? INPUT_PULLUP : INPUT);
  }
  void write(uint8_t pin, bool high) override { digitalWrite(pin, high ? HIGH : LOW); }
  bool read(uint8_t pin) override { return digitalRead(pin) == HIGH; }
};
