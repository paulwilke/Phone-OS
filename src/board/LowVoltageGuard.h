#pragma once

// Switches the device off before the cell is damaged (decision D-5). The cell is a 3.7 V lithium-polymer cell
// and nothing in the repository shows a protection circuit, so the firmware does it: when the voltage on the
// battery stays below kLowVoltageMv for kLowVoltageHoldMs the guard says "switch off". The hold is a minute
// because the voltage dips while the modem sends and a dip must not switch the device off; any reading at or
// above the limit, a cable, or a reading that is not known starts the count again.
//
// Pure logic: the caller samples (every few seconds) and carries out the switch-off.

#include <cstdint>

#include "../hal/Power.h"

namespace board {

constexpr int kLowVoltageMv = kBatteryEmptyMv;
constexpr uint32_t kLowVoltageHoldMs = 60000;

// The word under the picture when the device switches off: "Battery empty" when the guard asked for it.
inline const char* switchOffWordKey(bool becauseOfTheBattery) {
  return becauseOfTheBattery ? "power.battery-empty" : "power.switched-off";
}

class LowVoltageGuard {
public:
  // `millivolts` 0 means unknown. True when the device should be switched off now.
  bool update(uint32_t nowMs, int millivolts, bool onBattery) {
    if (!onBattery || millivolts <= 0 || millivolts >= kLowVoltageMv) {
      below_ = false;
      return false;
    }
    if (!below_) {
      below_ = true;
      since_ = nowMs;
    }
    return (uint32_t)(nowMs - since_) >= kLowVoltageHoldMs;
  }

private:
  bool below_ = false;
  uint32_t since_ = 0;
};

}  // namespace board
