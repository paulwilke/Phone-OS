#pragma once

// A satellite receiver that follows a scenario. It reads the power domain, so
// switching positioning off makes it silent and switching it on starts the scenario
// again, without the position of the earlier run.
//
// Scripting (one scenario at a time):
//   fixAfter(seconds, lat, lon, hdop)   no position until `seconds` after switch-on,
//                                       then this one
//   never()                             it runs but never finds a position
//   degradeAfter(seconds, hdop)         after `seconds` since the switch-on the
//                                       accuracy changes to `hdop` (larger = worse);
//                                       may be called several times
//   setSatellites(n)
// Time comes from the FakeClock the test advances.

#include <vector>

#include <hal/Clock.h>
#include <hal/GnssReceiver.h>

#include "FakePower.h"

class FakeGnss : public GnssReceiver {
public:
  FakeGnss(FakePower& power, Clock& clock) : power_(power), clock_(clock) {}

  bool readFix(GnssFix& out) override {
    const bool on = power_.isEnabled(PowerDomain::Positioning);
    const int restarts = power_.switchOnCount(PowerDomain::Positioning);
    if (!on) {
      wasOn_ = false;
      return false;
    }
    // A new run starts when the domain went from off to on; a switch-off and
    // switch-on between two reads is seen through the switch-on count.
    if (!wasOn_ || restarts != seenSwitchOns_) {
      onSince_ = clock_.uptimeMillis();
      seenSwitchOns_ = restarts;
      wasOn_ = true;
    }
    const double elapsed = (double)(clock_.uptimeMillis() - onSince_) / 1000.0;
    GnssFix fix;
    if (hasFix_ && elapsed >= fixDelay_) {
      fix.valid = true;
      fix.lat = lat_;
      fix.lon = lon_;
      fix.hdop = hdop_;
      for (const Change& c : changes_) {
        if (elapsed >= c.afterSeconds) fix.hdop = c.hdop;
      }
      fix.sats = sats_;
      fix.utc = clock_.epochUtc();
    }
    out = fix;
    return true;
  }

  // Scripting.
  void fixAfter(double seconds, double lat, double lon, float hdop) {
    hasFix_ = true;
    fixDelay_ = seconds;
    lat_ = lat;
    lon_ = lon;
    hdop_ = hdop;
    changes_.clear();
  }
  void never() { hasFix_ = false; }
  void degradeAfter(double seconds, float hdop) { changes_.push_back({seconds, hdop}); }
  void setSatellites(uint8_t n) { sats_ = n; }

private:
  struct Change {
    double afterSeconds;
    float hdop;
  };
  FakePower& power_;
  Clock& clock_;
  bool hasFix_ = false;
  double fixDelay_ = 0, lat_ = 0, lon_ = 0;
  float hdop_ = 99.0f;
  uint8_t sats_ = 7;
  std::vector<Change> changes_;
  bool wasOn_ = false;
  int seenSwitchOns_ = 0;
  uint64_t onSince_ = 0;
};
