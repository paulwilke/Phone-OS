#pragma once

#include <Arduino.h>

#include <board/MillisSource.h>

class Esp32Millis : public MillisSource {
public:
  uint32_t millis32() override { return millis(); }
};
