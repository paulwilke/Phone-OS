#pragma once

// The fakes every test of the application core needs, wired as on a device that has just
// started: the modem is powered and registered ("Example Mobile"), the clock knows the time (a
// Tuesday afternoon, 14:32 local, in a zone two hours east of UTC) and has run for 100 seconds,
// the radio exists but is down. A test adds the component it tests and changes what differs.

#include <ProfileBuilder.h>
#include <app/Logger.h>
#include <app/MessageStore.h>
#include <app/Notices.h>
#include <app/SchoolTime.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakePower.h>
#include <fakes/FakeRadioLink.h>

namespace coretest {
constexpr uint32_t kTuesdayAfternoon = 1789475520;   // 2026-09-15 12:32 UTC
constexpr int kZoneMinutes = 120;
}  // namespace coretest

struct CoreFakes {
  FakeClock clock;
  FakePower power{&clock};
  FakeModem modem{clock};
  FakeGnss gnss{power, clock};
  FakeRadioLink radio;
  FakeHaptic haptic{clock};
  FakeEventLog log{&clock};
  Profile profile;
  Logger logger{log, profile};
  MessageStore store;
  SchoolTime school{clock, profile, logger};
  Notices notices{clock};

  explicit CoreFakes(Profile p = ProfileBuilder().build()) : profile(std::move(p)) {
    clock.setTzOffsetMinutes(coretest::kZoneMinutes);
    clock.advanceSeconds(100);
    clock.setEpochUtc(coretest::kTuesdayAfternoon);
    power.setEnabled(PowerDomain::Cellular, true);
    modem.begin();
    modem.setNetwork("Example Mobile", 4);
  }

  // The network goes away and comes back.
  void networkLost() { modem.setNetwork("", 0); }
  void networkBack() { modem.setNetwork("Example Mobile", 4); }
  // The radio comes up with one contact.
  void radioUp(const std::string& peer = "Node One") {
    radio.setUp(true);
    radio.addPeer(peer, true, 0);
  }

  size_t played(HapticPattern pattern) const {
    size_t n = 0;
    for (HapticPattern p : haptic.played()) n += p == pattern ? 1 : 0;
    return n;
  }
  void advance(uint32_t seconds) { clock.advanceSeconds(seconds); }
};
