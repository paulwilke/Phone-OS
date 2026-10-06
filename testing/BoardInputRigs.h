#pragma once

// The rigs that run the Haptic and MotionSensor contract suites against the board classes on scripted chips.
//
// BoardHapticRig: a DRV2605 on a scripted bus, the expander behind it, and the clock the test advances. The
// effects and times the contract asks for are read off the bus: each write of 1 to the GO register is a
// pulse, and the effect is what the waveform register held, stamped with the clock.
// BoardMotionRig: the sensor hub fake behind the board class; the rail leg is watched.

#include <vector>

#include <board/BoardHaptic.h>
#include <board/BoardMotion.h>
#include <board/PinMap.h>
#include <fakes/FakeAccelerometerHub.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeI2cBus.h>

// A scripted bus that notes every pulse the haptic chip is told to fire.
class PulseRecordingBus : public FakeI2cBus {
public:
  struct Pulse {
    uint64_t atMs;
    uint8_t effect;
  };

  PulseRecordingBus(Clock& clock, const FakeDelay* time) : FakeI2cBus(time), clock_(clock) {}

  bool write(uint8_t address, const uint8_t* data, size_t len) override {
    const bool ok = FakeI2cBus::write(address, data, len);
    if (ok && address == board::i2c::kHaptic && len == 2 && data[0] == 0x0C && data[1] == 1)
      pulses_.push_back({clock_.uptimeMillis(), reg(address, 0x04)});
    return ok;
  }
  const std::vector<Pulse>& pulses() const { return pulses_; }

private:
  Clock& clock_;
  std::vector<Pulse> pulses_;
};

struct BoardHapticRig {
  FakeClock clockImpl;
  FakeDelay delay;
  PulseRecordingBus bus{clockImpl, &delay};
  board::Xl9555 expander{bus};
  board::Drv2605 driver{bus};
  board::BoardHaptic impl{driver, expander, clockImpl, delay};

  BoardHapticRig() {
    bus.addChip(board::i2c::kExpander);
    bus.addChip(board::i2c::kHaptic);
    expander.begin();
  }
  Haptic& haptic() { return impl; }
  void advance(uint32_t ms) { clockImpl.advance(ms); }
  std::vector<uint8_t> fired() {
    std::vector<uint8_t> out;
    for (const auto& p : bus.pulses()) out.push_back(p.effect);
    return out;
  }
  std::vector<uint64_t> firedAt() {
    std::vector<uint64_t> out;
    for (const auto& p : bus.pulses()) out.push_back(p.atMs);
    return out;
  }
};

struct BoardMotionRig {
  FakeDelay delay;
  FakeI2cBus bus{&delay};
  board::Xl9555 expander{bus};
  FakeAccelerometerHub hub;
  board::BoardMotion impl{expander, hub, delay};

  BoardMotionRig() {
    bus.addChip(board::i2c::kExpander);
    expander.begin();
    hub.watchRail(expander, board::leg::kMotionRail);
    impl.begin();
  }
  MotionSensor& sensor() { return impl; }
  // The sensor is on the board whenever the expander answers; it cannot be made absent from outside.
  bool setAvailable(bool available) { return available; }
  bool failNextStart() {
    hub.failNextStart();
    return true;
  }
  bool tilt(float x, float y, float z) {
    hub.tilt(x, y, z);
    return true;
  }
};
