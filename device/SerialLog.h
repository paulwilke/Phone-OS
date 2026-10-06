#pragma once

// The event log of the firmware until the card's log is wired in: one line to the USB console. A line that
// nobody reads is dropped by the USB stack, so this works without a computer attached.

#include <Arduino.h>

#include <hal/EventLog.h>

class SerialLog : public EventLog {
public:
  void log(std::string_view line) override {
    Serial.print("[log] ");
    Serial.write(reinterpret_cast<const uint8_t*>(line.data()), line.size());
    Serial.println();
  }
};
