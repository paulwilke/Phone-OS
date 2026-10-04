#include "SosFlow.h"

bool SosFlow::begin(const std::string& target, bool quietHours) {
  if (target.empty()) return false;
  if (host_.pingActive() || phase_ != Phase::Idle) return true;
  target_ = target;
  choiceStart_ = host_.uptimeMillis();
  host_.startSearch();
  if (quietHours) {
    phase_ = Phase::QuietConfirm;
    tapsLeft_ = kOverrideTaps;
  } else {
    phase_ = Phase::Choosing;
  }
  return true;
}

uint32_t SosFlow::autoLeft() {
  if (phase_ != Phase::Choosing) return 0;
  const uint64_t now = host_.uptimeMillis();
  const uint64_t gone = now >= choiceStart_ ? now - choiceStart_ : 0;
  const uint64_t total = (uint64_t)kAutoSeconds * 1000;
  // Rounded up: "1" is shown until the last millisecond, "0" only when it is time.
  return gone >= total ? 0 : (uint32_t)((total - gone + 999) / 1000);
}

// A pause that long between two taps ends the series. A clock that went back counts
// as no pause: when in doubt, the taps made so far stand.
bool SosFlow::tapWindowPassed() {
  const uint64_t now = host_.uptimeMillis();
  return tapsLeft_ < kOverrideTaps && now >= lastTap_ && now - lastTap_ >= kTapWindowMs;
}

void SosFlow::tick() {
  if (phase_ == Phase::Choosing && autoLeft() == 0) choose(SosPing::Kind::Emergency);
  if (phase_ == Phase::QuietConfirm && tapWindowPassed()) tapsLeft_ = kOverrideTaps;
}

// Closes the flow and hands the receiver over: the ping holds it from now on, so
// the flow lets go of its own claim only after the ping has started.
void SosFlow::close() {
  phase_ = Phase::Idle;
  host_.stopSearch();
}

bool SosFlow::choose(SosPing::Kind kind) {
  if (phase_ != Phase::Choosing || kind == SosPing::Kind::Query) return false;
  const bool started = host_.startPing(target_, kind);
  close();
  return started;
}

bool SosFlow::tap() {
  if (phase_ != Phase::QuietConfirm) return false;
  if (tapWindowPassed()) tapsLeft_ = kOverrideTaps;
  lastTap_ = host_.uptimeMillis();
  if (--tapsLeft_ > 0) return false;
  host_.liftQuietLock();
  const bool started = host_.startPing(target_, SosPing::Kind::Emergency);
  close();
  return started;
}

void SosFlow::leave() {
  if (phase_ == Phase::Idle) return;
  close();
}
