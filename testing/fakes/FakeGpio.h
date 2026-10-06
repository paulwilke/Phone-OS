#pragma once

// The processor's pins, scripted. A test sets the level a pin reads (setInput) and reads back the mode
// and the level the code gave it and the whole history of writes with the time (events()).

#include <cstdint>
#include <map>
#include <vector>

#include <board/Gpio.h>
#include "FakeDelay.h"

class FakeGpio : public Gpio {
public:
  struct Event {
    uint32_t atMs;
    uint8_t pin;
    bool high;
  };

  explicit FakeGpio(const FakeDelay* time = nullptr) : time_(time) {}

  void setMode(uint8_t pin, Mode mode) override { modes_[pin] = mode; }
  void write(uint8_t pin, bool high) override {
    levels_[pin] = high;
    events_.push_back({time_ ? time_->elapsedMs() : 0, pin, high});
  }
  bool read(uint8_t pin) override {
    const auto in = inputs_.find(pin);
    if (in != inputs_.end()) return in->second;
    const auto out = levels_.find(pin);
    return out != levels_.end() && out->second;
  }

  void setInput(uint8_t pin, bool high) { inputs_[pin] = high; }

  bool hasMode(uint8_t pin) const { return modes_.count(pin) != 0; }
  Mode mode(uint8_t pin) const { return modes_.at(pin); }
  bool level(uint8_t pin) const {
    const auto it = levels_.find(pin);
    return it != levels_.end() && it->second;
  }
  const std::vector<Event>& events() const { return events_; }

private:
  const FakeDelay* time_;
  std::map<uint8_t, Mode> modes_;
  std::map<uint8_t, bool> levels_;
  std::map<uint8_t, bool> inputs_;
  std::vector<Event> events_;
};
