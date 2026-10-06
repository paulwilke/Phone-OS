#include "BoardHaptic.h"

#include "PinMap.h"

namespace board {

void BoardHaptic::play(HapticPattern pattern) { player_.start(pattern, (uint32_t)clock_.uptimeMillis()); }

bool BoardHaptic::powerUp() {
  if (powered_) return true;
  if (!expander_.set(leg::kHapticPower, true)) return false;
  delay_.delayMs(kHapticStartMs);
  if (!driver_.begin()) {
    expander_.set(leg::kHapticPower, false);
    return false;
  }
  powered_ = true;
  return true;
}

void BoardHaptic::powerDown() {
  expander_.set(leg::kHapticPower, false);
  powered_ = false;
}

void BoardHaptic::service() {
  const uint64_t now = clock_.uptimeMillis();
  uint8_t effect;
  // A pulse that is due is fired, however late the loop came back; a motor that cannot be started loses
  // that pulse and the pattern carries on.
  while (player_.poll((uint32_t)now, effect)) {
    if (!powerUp()) continue;
    driver_.fire(effect);
    lastPulseMs_ = now;
  }
  if (powered_ && !player_.busy() && now - lastPulseMs_ >= kHapticSupplyIdleMs) powerDown();
}

}  // namespace board
