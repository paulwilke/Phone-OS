#include "Positioning.h"

void Positioning::want(Reason reason, bool on) {
  wanted_[static_cast<size_t>(reason)] = on;
  apply();
}

void Positioning::startStartupFix() {
  startupBegan_ = clock_.uptimeSeconds();
  log_.log("Positioning: start-up fix requested (time and first position)");
  want(Reason::StartupFix, true);
}

bool Positioning::active() { return power_.isEnabled(PowerDomain::Positioning); }

// The receiver follows the reasons. The switch is asked of the power capability every time it
// differs, so a switch that failed is tried again at the next poll instead of being forgotten.
void Positioning::apply() {
  bool any = false;
  for (bool w : wanted_) any = any || w;
  if (any == power_.isEnabled(PowerDomain::Positioning)) return;
  if (!power_.setEnabled(PowerDomain::Positioning, any)) {
    log_.log(any ? "Positioning cannot be switched on" : "Positioning cannot be switched off");
  }
}

// Fixes are read whether or not the receiver is on: when it is off the read says so, and when a
// failed switch left it off the start-up fix must still run out its time instead of waiting for
// a receiver that never starts.
void Positioning::poll() {
  apply();
  const uint32_t now = clock_.uptimeSeconds();
  GnssFix fix;
  if (receiver_.readFix(fix) && fix.valid) {
    last_ = fix;
    lastAt_ = now;
    haveFix_ = true;
    if (wants(Reason::StartupFix) && (fix.hdop <= kUsableHdop || now - startupBegan_ >= kAcceptAnySeconds)) {
      log_.log("Positioning: start-up fix received, receiver off");
      want(Reason::StartupFix, false);
      return;
    }
  }
  if (wants(Reason::StartupFix) && now - startupBegan_ >= kStartupFixSeconds) {
    log_.log("Positioning: start-up fix without result, receiver off");
    want(Reason::StartupFix, false);
  }
}

bool Positioning::lastKnownFix(GnssFix& out, uint32_t* ageSeconds) {
  if (!haveFix_) return false;
  out = last_;
  if (ageSeconds) *ageSeconds = clock_.uptimeSeconds() - lastAt_;
  return true;
}
