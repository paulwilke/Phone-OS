#pragma once

#include <Arduino.h>

#include <board/Delay.h>

class Esp32Delay : public Delay {
public:
  void delayMs(uint32_t ms) override { delay(ms); }
  void delayUs(uint32_t us) override { delayMicroseconds(us); }
};
