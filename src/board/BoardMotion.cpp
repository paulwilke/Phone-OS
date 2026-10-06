#include "BoardMotion.h"

#include "PinMap.h"

namespace board {

void BoardMotion::begin() { expander_.set(leg::kMotionRail, false); }

bool BoardMotion::start() {
  if (running_) return true;
  if (!expander_.ready() || !expander_.set(leg::kMotionRail, true)) return false;
  delay_.delayMs(kMotionRailSettleMs);
  if (!hub_.start()) {
    expander_.set(leg::kMotionRail, false);
    return false;
  }
  running_ = true;
  return true;
}

void BoardMotion::stop() {
  if (!running_) return;
  hub_.stop();
  // The firmware lives in the chip's RAM and is lost with the rail; the next start loads it again.
  expander_.set(leg::kMotionRail, false);
  running_ = false;
}

bool BoardMotion::read(MotionSample& out) {
  if (!running_) return false;
  float x, y, z;
  if (!hub_.read(x, y, z)) return false;
  out.ax = x;
  out.ay = y;
  out.az = z;
  out.valid = true;
  return true;
}

}  // namespace board
