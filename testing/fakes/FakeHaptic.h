#pragma once

// Vibration that records instead of vibrating. It uses the real pattern player on the
// clock the test controls, so the rhythm is the real one.
//
// Reading back: played() (every play() call, in order), fired() (every pulse that
// service() fired, with the uptime in milliseconds and the effect number).

#include <vector>

#include <hal/Clock.h>
#include <hal/Haptic.h>

class FakeHaptic : public Haptic {
public:
  struct Pulse {
    uint64_t atMs;
    uint8_t effect;
  };

  explicit FakeHaptic(Clock& clock) : clock_(clock) {}

  void play(HapticPattern pattern) override {
    played_.push_back(pattern);
    player_.start(pattern, (uint32_t)clock_.uptimeMillis());
  }
  void service() override {
    uint8_t effect;
    while (player_.poll((uint32_t)clock_.uptimeMillis(), effect)) fired_.push_back({clock_.uptimeMillis(), effect});
  }
  bool busy() const override { return player_.busy(); }

  const std::vector<HapticPattern>& played() const { return played_; }
  const std::vector<Pulse>& fired() const { return fired_; }

private:
  Clock& clock_;
  HapticPlayer player_;
  std::vector<HapticPattern> played_;
  std::vector<Pulse> fired_;
};
