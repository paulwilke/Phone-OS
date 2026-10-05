#pragma once

// A whole device for screen tests: the application core on the fakes, with all catalogs loaded
// (and the pseudo-language) and the clock at a Tuesday afternoon, 14:32 local, in a zone two
// hours east of UTC. A screen under test is given `context()` and a recording Navigation; the
// test scripts the world through the fakes and reads what the screen did from `nav` and `core`.
//
//   DeviceRig rig;
//   HomeScreen home(rig.context());
//   rig.modem.setNetwork("Example Mobile", 4);
//   rig.poll();
//   home.render(rig.frame);
//
// The numbers and names are invented (ProfileBuilder.h).

#include <string>
#include <vector>

#include <Languages.h>
#include <ProfileBuilder.h>
#include <app/AppCore.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakePower.h>
#include <fakes/FakeRadioLink.h>
#include <fakes/FakeWifi.h>
#include <ui/AppScreen.h>

namespace rig {
constexpr uint32_t kTuesdayAfternoon = 1789475520;  // 2026-09-15 12:32 UTC, 14:32 in the rig's zone
constexpr int kZoneMinutes = 120;
inline uint32_t fixedRandom() { return 12345678u; }
}  // namespace rig

// What a screen asked the stack to do, in order. "open 3", "back", "home", "lock", "unlock".
class RecordingNavigation : public ui::Navigation {
public:
  void open(ui::ScreenId id) override { calls.push_back("open " + std::to_string(id)); }
  void back() override { calls.push_back("back"); }
  void home() override { calls.push_back("home"); }
  void lock() override { calls.push_back("lock"); }
  void unlock() override { calls.push_back("unlock"); }

  bool opened(ui::ScreenId id) const { return last() == "open " + std::to_string(id); }
  std::string last() const { return calls.empty() ? std::string() : calls.back(); }
  void clear() { calls.clear(); }

  std::vector<std::string> calls;
};

struct DeviceRig {
  FakeClock clock;
  FakePower power{&clock};
  FakeModem modem{clock};
  FakeGnss gnss{power, clock};
  FakeRadioLink radio;
  FakeHaptic haptic{clock};
  FakeEventLog log{&clock};
  FakeWifi wifi;
  i18n::Translator translator = languages::loaded();
  AppCore core;
  RecordingNavigation nav;
  FrameBuffer frame;

  explicit DeviceRig(Profile profile = ProfileBuilder().childName("Alex").build(), bool withWifi = true)
      : core((prepare(), clock), power, modem, gnss, haptic, log, translator, std::move(profile)) {
    if (withWifi) core.setWifi(wifi, rig::fixedRandom);
    core.start();
  }

  ui::ScreenContext context() { return ui::ScreenContext{core, translator, nav}; }

  void poll(bool displayOn = true) { core.poll(displayOn, 10000); }
  // Seconds pass, polled every second as the device does.
  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      clock.advanceSeconds(1);
      core.poll(true, 10000);
    }
  }
  void language(const std::string& code) { TEST_ASSERT_TRUE_MESSAGE(translator.select(code), code.c_str()); }

private:
  // The world before the core starts: the network is there and the time is known.
  void prepare() {
    clock.setTzOffsetMinutes(rig::kZoneMinutes);
    clock.advanceSeconds(100);
    clock.setEpochUtc(rig::kTuesdayAfternoon);
    power.setEnabled(PowerDomain::Cellular, true);
    modem.begin();
    modem.setNetwork("Example Mobile", 4);
    gnss.fixAfter(2, 48.1, 11.5, 1.5);
  }
};
