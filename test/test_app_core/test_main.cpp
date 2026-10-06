// Scenarios for the application core as a whole: the facade with all the fakes, the way the device
// lives a day. The rules of each component are pinned in its own tests; here it is the
// composition that is tested — the order of a pass, who is told what, and that the parts do not
// get in each other's way.

#include <unity.h>

#include <fstream>
#include <sstream>

#include <CoreFakes.h>
#include <app/AppCore.h>
#include <app/SystemEntry.h>
#include <fakes/FakeWifi.h>

using namespace people;

void setUp() {}
void tearDown() {}

namespace {

constexpr uint32_t kJanuaryTuesday = 1799760720;  // 2027-01-12 13:32 UTC

uint32_t fixedRandom() { return 12345678u; }

struct Hardware {
  FakeClock clock;
  FakePower power{&clock};
  FakeModem modem{clock};
  FakeGnss gnss{power, clock};
  FakeRadioLink radio;
  FakeHaptic haptic{clock};
  FakeEventLog log{&clock};
  FakeWifi wifi;
  i18n::Translator translator;

  Hardware() {
    for (const char* file : {"en.toml", "de.toml", "fr.toml"}) {
      std::ifstream in(std::string("lang/") + file);
      std::stringstream ss;
      ss << in.rdbuf();
      const std::string text = ss.str();
      i18n::Catalog catalog;
      TEST_ASSERT_TRUE_MESSAGE(!text.empty() && catalog.parse(text.data(), text.size()), file);
      translator.add(catalog);
    }
    clock.setTzOffsetMinutes(coretest::kZoneMinutes);
    clock.advanceSeconds(100);
    clock.setEpochUtc(coretest::kTuesdayAfternoon);
    power.setEnabled(PowerDomain::Cellular, true);
    modem.begin();
    modem.setNetwork("Example Mobile", 4);
    gnss.fixAfter(2, 48.1, 11.5, 1.5);
  }
};

// A history file in memory, which outlives the device that wrote it.
struct ListLog : MessageLog {
  std::vector<std::string> lines;
  bool append(const char* line) override {
    lines.push_back(line);
    return true;
  }
  bool readAll(Visitor& visitor) override {
    for (const std::string& l : lines) visitor.line(l.c_str());
    return true;
  }
};

// The device at 14:32 on a Tuesday, started.
struct Device : Hardware {
  AppCore core;

  explicit Device(Profile p = ProfileBuilder().childName("Alex").build(), bool radioRuns = false,
                  MessageLog* messageLog = nullptr)
      : core(clock, power, modem, gnss, haptic, log, translator, std::move(p)) {
    core.setWifi(wifi, fixedRandom);
    core.attachMessageLog(messageLog);
    if (radioRuns) {
      radio.setUp(true);
      radio.addPeer("Node One", true, 0);
      core.setRadio(&radio);
    }
    core.start();
  }

  AppCore::State poll(bool displayOn = false) {
    core.poll(displayOn, 10000);
    return core.state();
  }
  // Seconds pass, polled every second as the device does.
  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      clock.advanceSeconds(1);
      core.poll(false, 10000);
    }
  }
  size_t played(HapticPattern pattern) const {
    size_t n = 0;
    for (HapticPattern p : haptic.played()) n += p == pattern ? 1 : 0;
    return n;
  }
  size_t sent() const { return modem.sent().size(); }
  bool logged(const std::string& part) const { return log.contains(part); }
};

// Texts received as messages: a query is none.
size_t receivedTexts(Device& d) {
  size_t n = 0;
  for (size_t i = 0; i < d.core.history().count(); ++i) {
    const Message& m = d.core.history().at(i);
    n += m.source == MsgSource::Sms && m.dir == MsgDir::In ? 1 : 0;
  }
  return n;
}

Profile inSchool() {
  return ProfileBuilder().childName("Alex").schoolTime("tue", "14:00", "15:00").radio(true).build();
}

}  // namespace

// ---- one pass -----------------------------------------------------------------------------------

void test_poll_drains_everything() {
  Device d(inSchool(), true);
  d.core.markAllRead();
  d.modem.failNextSms("+CMS ERROR: 304");
  d.core.send(kParentOne, "Out");                                    // accepted now, refused later
  d.modem.injectSms(kParentOne, "Hello", 1);
  d.radio.deliver("Node One", "Hi by radio", 1);
  d.modem.injectIncomingCall(kParentTwo);
  d.core.beginSos();                                                 // a search that waits to be advanced

  const AppCore::State s = d.poll();                                 // all of it in one pass
  // The SMS, the radio message, and the two system entries (the call missed in school time and the
  // refused text) are all unread.
  TEST_ASSERT_EQUAL(4, s.unread);
  TEST_ASSERT_TRUE(s.notice.kind == NoticeKind::CallerInSchoolTime); // the call: rejected in school time
  TEST_ASSERT_TRUE(d.logged("SMS to Parent One rejected: +CMS ERROR: 304"));
  TEST_ASSERT_TRUE(s.sosPhase == SosFlow::Phase::QuietConfirm);
  TEST_ASSERT_TRUE(s.positioningActive);
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Message));            // a parent's message, also in school time
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Failed));
}

void test_the_state_offers_what_the_interface_draws() {
  Device d(inSchool());
  d.modem.setNetwork("F-Operator Telecom", 3);
  d.power.plugIn(ChargeState::Charging);
  d.power.setBattery(64);
  AppCore::State s = d.poll();
  TEST_ASSERT_TRUE(Connectivity::NetworkLabel::Kind::Operator == s.network.kind);
  TEST_ASSERT_EQUAL_STRING("Operator", s.network.name.c_str());
  TEST_ASSERT_TRUE(s.registered);
  TEST_ASSERT_EQUAL(3, s.signalBars);
  TEST_ASSERT_TRUE(s.schoolTime);
  TEST_ASSERT_EQUAL(15 * 60, s.schoolTimeEndMinute);
  TEST_ASSERT_EQUAL(64, s.batteryPercent);
  TEST_ASSERT_TRUE(ChargeState::Charging == s.charge);
  TEST_ASSERT_TRUE(s.cellularEnabled);
  TEST_ASSERT_FALSE(s.saveMode);
  TEST_ASSERT_FALSE(s.callActive);
  TEST_ASSERT_TRUE(s.pingStage == SosPing::Stage::Idle);
  TEST_ASSERT_TRUE(s.sosPhase == SosFlow::Phase::Idle);

  d.core.beginSos();
  s = d.core.state();
  TEST_ASSERT_TRUE(s.sosPhase == SosFlow::Phase::QuietConfirm);
  TEST_ASSERT_EQUAL(SosFlow::kOverrideTaps, s.sosTapsLeft);

  d.modem.setSimStatus(SimStatus::Missing);                          // a locked or missing SIM is named, not "no network"
  s = d.poll();
  TEST_ASSERT_TRUE(SimStatus::Missing == s.sim);
  TEST_ASSERT_FALSE(s.registered);

  Device e;                                                          // outside school time: the countdown
  e.core.beginSos();
  TEST_ASSERT_EQUAL_UINT32(40, e.core.state().sosSecondsLeft);
  TEST_ASSERT_FALSE(e.core.state().schoolTime);
}

// ---- scenarios ----------------------------------------------------------------------------------

void test_a_full_day() {
  Device d(inSchool());
  // The start: the receiver looks for the time and a first position, and gives up the receiver
  // when it has them.
  TEST_ASSERT_TRUE(d.poll().positioningActive);
  d.run(4);
  TEST_ASSERT_FALSE(d.core.state().positioningActive);
  GnssFix fix;
  TEST_ASSERT_TRUE(d.core.lastKnownFix(fix));

  // School time: a parent's call is rejected without a sound, the child cannot dial.
  d.modem.injectIncomingCall(kParentOne);
  d.run(2);
  TEST_ASSERT_EQUAL(0, d.played(HapticPattern::Ring));
  TEST_ASSERT_FALSE(d.core.dial(kParentOne));
  TEST_ASSERT_TRUE(d.core.state().notice.kind == NoticeKind::SchoolTimeUntil);
  // A message from Grandma arrives silently, one from a parent is felt.
  d.modem.injectSms(kGrandma, "Dinner at six", 1);
  d.run(1);
  TEST_ASSERT_EQUAL(0, d.played(HapticPattern::Message));
  d.modem.injectSms(kParentOne, "Call me", 2);
  d.run(1);
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Message));
  TEST_ASSERT_EQUAL(3, d.core.state().unread);                       // both messages and the missed call
  d.core.markAllRead();
  TEST_ASSERT_EQUAL(0, d.core.state().unread);

  // School time ends at 15:00; the same call now rings and can be answered, then the child calls.
  d.run(28 * 60);
  TEST_ASSERT_FALSE(d.core.state().schoolTime);
  d.modem.injectIncomingCall(kParentTwo);
  d.run(1);
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Ring));
  TEST_ASSERT_TRUE(d.core.answer());
  d.run(5);
  TEST_ASSERT_TRUE(d.core.state().callState == ModemState::InCall);
  TEST_ASSERT_EQUAL_STRING(kParentTwo, d.core.state().callPeer.c_str());
  TEST_ASSERT_TRUE(d.core.hangup());
  d.run(1);
  TEST_ASSERT_TRUE(d.core.dial(kParentOne));
  d.modem.remoteBusy();
  d.run(1);
  TEST_ASSERT_TRUE(d.core.state().notice.kind == NoticeKind::CallBusy);
  // The history holds the missed call from school time with the number.
  size_t missed = 0;
  for (size_t i = 0; i < d.core.history().count(); ++i) {
    sysentry::Entry e;
    if (sysentry::parse(d.core.history().at(i), e) && e.kind == sysentry::kMissedCall) ++missed;
  }
  TEST_ASSERT_EQUAL(1, missed);
}

void test_an_sos_in_school_time_with_the_override() {
  Device d(inSchool());
  d.core.markAllRead();
  TEST_ASSERT_TRUE(d.core.beginSos());
  TEST_ASSERT_TRUE(d.poll().positioningActive);                      // the search started with the press
  for (int i = 0; i < SosFlow::kOverrideTaps - 1; ++i) d.core.tapSos();
  TEST_ASSERT_TRUE(d.core.state().schoolTime);                       // four taps do nothing
  TEST_ASSERT_TRUE(d.core.tapSos());
  TEST_ASSERT_FALSE(d.core.state().schoolTime);                      // lifted until 15:00
  d.run(5);
  TEST_ASSERT_TRUE(d.core.state().pingStage == SosPing::Stage::Refine);
  TEST_ASSERT_EQUAL(2, d.core.state().pingProgress);
  d.run(21);
  TEST_ASSERT_EQUAL(1, d.sent());
  TEST_ASSERT_EQUAL_STRING(kParentOne, d.modem.sent()[0].phone.c_str());
  TEST_ASSERT_TRUE(d.modem.sent()[0].body.find("Alex sends SOS") != std::string::npos);
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Sent));
  TEST_ASSERT_FALSE(d.core.state().positioningActive);
  // The child can be called back: the lock is lifted for the rest of this school time.
  d.modem.injectIncomingCall(kParentOne);
  d.run(1);
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Ring));
  TEST_ASSERT_TRUE(d.core.answer());
  // And the next window locks again.
  d.core.hangup();
  d.run(60);
  TEST_ASSERT_TRUE(d.logged("Quiet hours lifted by emergency until 15:00"));
}

void test_a_remote_query_during_an_sos() {
  Device d;
  d.core.markAllRead();
  d.core.beginSos();
  d.core.chooseSos(SosPing::Kind::Emergency);
  d.run(5);
  d.modem.injectSms(kParentTwo, "#WhereAreYou", 1);                  // the other parent asks meanwhile
  d.run(1);
  TEST_ASSERT_TRUE(d.logged("Remote query from"));
  TEST_ASSERT_TRUE(d.logged("busy"));
  TEST_ASSERT_TRUE(d.core.state().emergencyRunning);                 // the emergency is not disturbed
  d.run(30);
  TEST_ASSERT_EQUAL(1, d.sent());                                    // one text: the emergency, to the first contact
  TEST_ASSERT_EQUAL_STRING(kParentOne, d.modem.sent()[0].phone.c_str());
  // The query is in the history as a system entry, not as a message.
  bool found = false;
  for (size_t i = 0; i < d.core.history().count(); ++i) {
    sysentry::Entry e;
    if (sysentry::parse(d.core.history().at(i), e) && e.kind == sysentry::kLocationQueried) found = true;
  }
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL(1, d.core.state().unread);                       // the system entry, and no message
}

void test_the_state_tells_how_the_sos_search_is_going_and_to_whom() {
  Device d;
  d.core.beginSos();
  d.core.chooseSos(SosPing::Kind::Emergency);
  AppCore::State s = d.core.state();
  TEST_ASSERT_FALSE(s.pingHasFix);
  TEST_ASSERT_EQUAL(0, s.pingAccuracyMeters);
  TEST_ASSERT_EQUAL_STRING(kParentOne, s.pingPeer.c_str());
  d.run(5);
  s = d.core.state();
  TEST_ASSERT_EQUAL(5, s.pingElapsedSeconds);
  TEST_ASSERT_TRUE(s.pingHasFix);                                    // a fix after two seconds, HDOP 1.5
  TEST_ASSERT_EQUAL(SosPing::accuracyMeters(1.5f), s.pingAccuracyMeters);
  d.run(30);                                                         // sent: the search is over
  s = d.core.state();
  TEST_ASSERT_TRUE(s.pingStage == SosPing::Stage::Done);
  TEST_ASSERT_EQUAL(0, s.pingElapsedSeconds);
  TEST_ASSERT_EQUAL_STRING(kParentOne, s.pingPeer.c_str());          // still says whom it went to
}

void test_a_remote_query_alone_is_answered_without_the_child_noticing() {
  Device d;
  d.modem.injectSms(kParentTwo, "#WhereAreYou", 1);
  d.run(40);
  TEST_ASSERT_EQUAL(1, d.sent());
  TEST_ASSERT_EQUAL_STRING(kParentTwo, d.modem.sent()[0].phone.c_str());
  TEST_ASSERT_TRUE(d.modem.sent()[0].body.find("Alex is here") != std::string::npos);
  TEST_ASSERT_EQUAL(0, d.haptic.played().size());
  TEST_ASSERT_FALSE(d.core.state().positioningActive);
}

void test_the_network_dropping_and_returning() {
  Device d;
  d.core.markAllRead();
  d.run(120);                                                        // quiet: the modem sleeps
  TEST_ASSERT_TRUE(d.core.state().modemAsleep);
  d.modem.setNetwork("", 0);                                         // the network drops
  d.run(1);
  AppCore::State s = d.core.state();
  TEST_ASSERT_FALSE(s.registered);
  TEST_ASSERT_TRUE(Connectivity::NetworkLabel::Kind::None == s.network.kind);
  TEST_ASSERT_FALSE(d.core.dial(kParentOne));
  TEST_ASSERT_TRUE(d.core.state().notice.kind == NoticeKind::NoNetwork);

  TEST_ASSERT_TRUE(d.core.send(kParentOne, "I am late"));            // it waits, and the device stays awake for it
  s = d.core.state();
  TEST_ASSERT_EQUAL(1, s.queued);
  TEST_ASSERT_TRUE(s.notice.kind == NoticeKind::MessageWaiting);
  d.run(5);
  TEST_ASSERT_FALSE(d.core.state().modemAsleep);
  TEST_ASSERT_EQUAL(0, d.sent());

  d.modem.setNetwork("Example Mobile", 4);                           // and it returns
  d.run(1);
  s = d.core.state();
  TEST_ASSERT_EQUAL(0, s.queued);
  TEST_ASSERT_EQUAL(1, d.sent());
  TEST_ASSERT_EQUAL_STRING("I am late", d.modem.sent()[0].body.c_str());
  TEST_ASSERT_TRUE(s.notice.kind == NoticeKind::MessageSent);
  TEST_ASSERT_TRUE(s.registered);
}

void test_save_mode_and_the_cell_probe() {
  Device d(ProfileBuilder().childName("Alex").radio(true).build(), false);
  TEST_ASSERT_FALSE(d.core.setSaveMode(true));                       // without the radio there would be no way out
  TEST_ASSERT_TRUE(d.core.state().notice.kind == NoticeKind::SaveModeNeedsRadio);
  TEST_ASSERT_TRUE(d.core.requestRadioStart());                      // the host starts the radio on request
  TEST_ASSERT_TRUE(d.core.takeRadioStartRequest());
  d.radio.setUp(true);
  d.radio.addPeer("Node One", true, 0);
  d.core.setRadio(&d.radio);
  TEST_ASSERT_EQUAL(1, d.core.radioPeerCount());

  d.core.setWifiMode(WifiMode::AccessPoint);
  TEST_ASSERT_TRUE(d.core.setSaveMode(true));
  AppCore::State s = d.poll();
  TEST_ASSERT_TRUE(s.saveMode);
  TEST_ASSERT_FALSE(s.cellularEnabled);
  TEST_ASSERT_TRUE(s.wifiPhase == WifiPhase::Off);
  d.modem.shutdown();                                                // what switching the power off does to the modem
  TEST_ASSERT_TRUE(Connectivity::NetworkLabel::Kind::RadioOnly == d.core.state().network.kind);
  TEST_ASSERT_FALSE(d.core.setWifiMode(WifiMode::Station));          // no Wi-Fi in save mode
  // With cellular off a message goes by radio.
  TEST_ASSERT_TRUE(d.core.send(kParentOne, "By radio"));
  TEST_ASSERT_EQUAL(1, d.radio.sent().size());

  TEST_ASSERT_TRUE(d.core.probeCellular());                          // look for the mobile network for a while
  s = d.poll();
  TEST_ASSERT_TRUE(s.cellularEnabled);
  TEST_ASSERT_EQUAL_UINT32(Connectivity::kProbeSeconds, s.probeSecondsLeft);
  d.modem.begin();
  d.modem.injectSms(kParentOne, "Are you ok?", 1);
  d.run(10);
  TEST_ASSERT_EQUAL(1, d.core.state().unread);                       // it arrived during the probe
  d.run(Connectivity::kProbeSeconds);
  s = d.core.state();
  TEST_ASSERT_FALSE(s.cellularEnabled);
  TEST_ASSERT_TRUE(s.saveMode);
  TEST_ASSERT_TRUE(d.core.setSaveMode(false));
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);
}

void test_run_time_settings_change_the_profile() {
  Device d;
  d.core.markAllRead();
  d.core.setMessageVibration(false);
  TEST_ASSERT_FALSE(d.core.messageVibration());
  d.modem.injectSms(kParentOne, "Hello", 1);
  d.run(1);
  TEST_ASSERT_EQUAL(0, d.played(HapticPattern::Message));
  d.core.setMessageVibration(true);
  d.modem.injectSms(kParentOne, "Hello again", 2);
  d.run(1);
  TEST_ASSERT_EQUAL(1, d.played(HapticPattern::Message));

  // How visible a remote query is follows the setting at once.
  d.core.setCommandNotice(CommandNotice::Notify);
  d.modem.injectSms(kParentTwo, "#WhereAreYou", 3);
  d.run(1);
  TEST_ASSERT_TRUE(d.core.state().notice.kind == NoticeKind::LocationQueried);
  d.run(40);
  const size_t before = d.core.history().count();
  d.core.setCommandNotice(CommandNotice::Silent);
  d.modem.injectSms(kParentTwo, "#WhereAreYou", 4);
  d.run(1);
  TEST_ASSERT_EQUAL(before, d.core.history().count());               // silent: the log only
  TEST_ASSERT_TRUE(d.core.commandNotice() == CommandNotice::Silent);
  TEST_ASSERT_TRUE(d.core.profile().commandNotice == CommandNotice::Silent);
}

void test_the_trigger_words_come_from_every_language() {
  Device d(ProfileBuilder().childName("Alex").extraTrigger("where r u").build());
  d.modem.injectSms(kParentTwo, "  #wherearyou ", 1);                 // wrong word: an ordinary message
  d.run(1);
  TEST_ASSERT_EQUAL(1, receivedTexts(d));
  for (const std::string& word : d.translator.textsInAllLanguages("sms.command.where")) {
    Device e;
    e.modem.injectSms(kParentTwo, word, 1);
    e.run(1);
    TEST_ASSERT_EQUAL_MESSAGE(0, receivedTexts(e), word.c_str());       // taken as a query in each language
    TEST_ASSERT_TRUE_MESSAGE(e.logged("Remote query from"), word.c_str());
  }
  Device f(ProfileBuilder().childName("Alex").extraTrigger("where r u").build());
  f.modem.injectSms(kParentTwo, "WHERE R U", 1);
  f.run(1);
  TEST_ASSERT_TRUE(f.logged("Remote query from"));
}

void test_the_clock_follows_the_time_zone() {
  Device d(ProfileBuilder().schoolTime("tue", "14:00", "15:00").timeZone("Europe/Berlin").build());
  d.poll();
  TEST_ASSERT_EQUAL(120, d.clock.tzOffsetMinutes());                 // summer time
  TEST_ASSERT_TRUE(d.core.state().schoolTime);                       // 12:32 UTC is 14:32 on the wall
  d.clock.setEpochUtc(kJanuaryTuesday);                              // 13:32 UTC in January: also 14:32 on the wall
  d.poll();
  TEST_ASSERT_EQUAL(60, d.clock.tzOffsetMinutes());
  TEST_ASSERT_TRUE(d.core.state().schoolTime);
  d.clock.setEpochUtc(kJanuaryTuesday + 31 * 60);                    // 15:03: over
  d.poll();
  TEST_ASSERT_FALSE(d.core.state().schoolTime);

  Device e(ProfileBuilder().schoolTime("tue", "14:00", "15:00").build());   // no zone: the offset stays as it is
  e.poll();
  TEST_ASSERT_EQUAL(coretest::kZoneMinutes, e.clock.tzOffsetMinutes());
}

void test_without_a_known_time_the_zone_is_left_alone() {
  Device d(ProfileBuilder().timeZone("Europe/Berlin").build());
  d.clock.setEpochUtc(0);
  d.clock.setTzOffsetMinutes(0);
  d.poll();
  TEST_ASSERT_EQUAL(0, d.clock.tzOffsetMinutes());
}

void test_the_state_has_the_wall_clock_as_numbers() {
  Device d;
  AppCore::State::WallClock w = d.core.state().wall;
  TEST_ASSERT_TRUE(w.known);
  TEST_ASSERT_EQUAL(2026, w.year);
  TEST_ASSERT_EQUAL(9, w.month);
  TEST_ASSERT_EQUAL(15, w.day);
  TEST_ASSERT_EQUAL(2, w.weekday);                                   // a Tuesday; Sunday is 0
  TEST_ASSERT_EQUAL(14 * 60 + 32, w.minuteOfDay);
  // Ten hours later it is the next day on the wall clock, in the zone and not in UTC.
  d.clock.setEpochUtc(coretest::kTuesdayAfternoon + 10 * 3600);
  w = d.core.state().wall;
  TEST_ASSERT_EQUAL(16, w.day);
  TEST_ASSERT_EQUAL(3, w.weekday);
  TEST_ASSERT_EQUAL(32, w.minuteOfDay);
  // The year changes with the wall clock, not with UTC: 23:30 UTC on New Year's Eve is already
  // 2027 two hours east of it.
  d.clock.setEpochUtc(1798759800);                                    // 2026-12-31 23:30 UTC
  w = d.core.state().wall;
  TEST_ASSERT_EQUAL(2027, w.year);
  TEST_ASSERT_EQUAL(1, w.month);
  TEST_ASSERT_EQUAL(1, w.day);
  // A Sunday is 0, not 7.
  d.clock.setEpochUtc(coretest::kTuesdayAfternoon + 5 * 86400);
  TEST_ASSERT_EQUAL(0, d.core.state().wall.weekday);
  // Without a known time there are no numbers to draw.
  d.clock.setEpochUtc(0);
  TEST_ASSERT_FALSE(d.core.state().wall.known);
}

void test_a_stored_time_is_shown_on_the_wall_clock_of_the_zone() {
  Device d;
  // The evening before at 23:32 UTC is 01:32 today two hours east.
  const AppCore::State::WallClock w = d.core.wallClockOf(coretest::kTuesdayAfternoon - 13 * 3600);
  TEST_ASSERT_TRUE(w.known);
  TEST_ASSERT_EQUAL(15, w.day);
  TEST_ASSERT_EQUAL(92, w.minuteOfDay);
  TEST_ASSERT_FALSE(d.core.wallClockOf(0).known);              // no time was known when it arrived
  // The same numbers as the state gives for the present moment.
  const AppCore::State::WallClock now = d.core.wallClockOf(coretest::kTuesdayAfternoon);
  TEST_ASSERT_EQUAL(d.core.state().wall.minuteOfDay, now.minuteOfDay);
  TEST_ASSERT_EQUAL(d.core.state().wall.day, now.day);
}

void test_the_core_produces_only_codes() {
  Device d(inSchool(), true);
  d.core.markAllRead();
  d.modem.injectIncomingCall(kParentOne);                            // missed in school time
  d.run(2);
  d.modem.injectSms(kParentOne, "#WhereAreYou", 1);                  // a query
  d.modem.injectSms(kStranger, "#WhereAreYou", 2);                   // a refused query
  d.run(1);
  d.modem.failNextSms("+CMS ERROR: 304");
  d.core.send(kParentOne, "x");                                      // a refused text
  d.run(1);
  d.run(30 * 60);                                                    // school time over
  d.core.dial(kParentOne);
  d.modem.failDial("busy");                                          // a failed call
  d.run(1);
  size_t system = 0;
  for (size_t i = 0; i < d.core.history().count(); ++i) {
    const Message& m = d.core.history().at(i);
    if (m.source != MsgSource::System) continue;
    ++system;
    sysentry::Entry e;
    TEST_ASSERT_TRUE_MESSAGE(sysentry::parse(m, e), m.text);         // a kind and an argument, never a sentence
    TEST_ASSERT_EQUAL('@', m.text[0]);
  }
  TEST_ASSERT_TRUE(system >= 4);
  // The notice is a code with arguments by its type; its wording lives in the catalogs.
  const Notice notice = d.core.state().notice;
  TEST_ASSERT_TRUE(notice.kind < NoticeKind::Count);
  TEST_ASSERT_TRUE(d.translator.get(noticeWording(notice.kind).key) != noticeWording(notice.kind).key);
}

void test_a_test_pattern_can_be_played() {
  Device d;
  d.core.playTestPattern(HapticPattern::Click);
  d.core.playTestPattern(HapticPattern::Failed);
  TEST_ASSERT_EQUAL(2, d.haptic.played().size());
  TEST_ASSERT_TRUE(HapticPattern::Failed == d.haptic.played()[1]);
}

void test_the_modem_that_reports_something_is_awake() {
  Device d;
  d.core.markAllRead();
  d.run(120);
  TEST_ASSERT_TRUE(d.core.state().modemAsleep);
  d.modem.injectSms(kParentOne, "Hello", 1);
  d.core.poll(false, 10000);                                         // it woke up to deliver this: it is not asleep
  size_t sleeps = 0;
  for (const std::string& line : d.log.lines()) sleeps += line == "Modem sleeps" ? 1 : 0;
  TEST_ASSERT_EQUAL(2, sleeps);                                      // and goes to sleep again afterwards, once more
}

void test_work_in_hand_keeps_the_device_awake() {
  for (int what = 0; what < 4; ++what) {
    Device d(ProfileBuilder().childName("Alex").radio(true).build(), what == 3);
    d.gnss.never();
    d.run(120);
    TEST_ASSERT_TRUE(d.core.state().modemAsleep);                    // quiet: asleep
    if (what == 0) d.core.beginSos();                                // the choice is open
    if (what == 1) {
      d.core.beginSos();
      d.core.chooseSos(SosPing::Kind::Emergency);                    // a ping is searching
    }
    if (what == 2) {
      d.modem.injectIncomingCall(kParentOne);                        // a call is connected
      d.run(1);
      d.core.answer();
    }
    const PowerDecision decision = d.core.poll(false, 10000);
    if (what == 3) {
      TEST_ASSERT_TRUE(d.power.isEnabled(PowerDomain::Radio));       // the radio is in use: its chip has power
      continue;
    }
    TEST_ASSERT_FALSE_MESSAGE(d.core.state().modemAsleep, std::to_string(what).c_str());
    TEST_ASSERT_FALSE(decision.cpuMaySleep);
  }
  Device e;
  e.poll();
  TEST_ASSERT_FALSE(e.power.isEnabled(PowerDomain::Radio));          // no radio: the chip stays off
}

void test_the_history_and_the_wifi_settings_are_loaded() {
  ListLog file;
  {
    Device first(ProfileBuilder().build(), false, &file);
    first.modem.injectSms(kParentOne, "Remember me", 1);
    first.run(1);
  }
  Profile p = ProfileBuilder().build();
  p.wifi.addNetwork("HomeNet", "secret-pass");
  Device second(p, false, &file);
  TEST_ASSERT_EQUAL(1, second.core.history().count());
  TEST_ASSERT_EQUAL_STRING("Remember me", second.core.history().at(0).text);
  TEST_ASSERT_TRUE(second.core.setWifiMode(WifiMode::Station));
  TEST_ASSERT_EQUAL_STRING("HomeNet", second.wifi.ssid().c_str());   // the profile's networks reached the controller
}

// ---- the host's side ----------------------------------------------------------------------------

void test_the_host_gets_the_decision_and_the_requests() {
  Device d;
  d.core.markAllRead();
  d.core.noteInput();
  PowerDecision decision = d.core.poll(false, 10000);
  TEST_ASSERT_FALSE(decision.cpuMaySleep);                           // the modem is not asleep yet
  d.clock.advanceSeconds(70);
  decision = d.core.poll(false, 800);
  TEST_ASSERT_TRUE(decision.cpuMaySleep);
  TEST_ASSERT_EQUAL_UINT32(800, decision.cpuSleepMs);
  TEST_ASSERT_FALSE(d.core.poll(true, 800).cpuMaySleep);             // a lit display stays awake

  d.core.requestRestart();
  TEST_ASSERT_TRUE(d.core.restartRequested());
  d.core.requestPowerOff();
  d.power.allowPowerOff(false);
  TEST_ASSERT_TRUE(d.core.powerOffRequested());
  d.core.notePowerOffFailed();
  TEST_ASSERT_FALSE(d.core.state().powerOffRequested);
  TEST_ASSERT_TRUE(d.core.state().notice.kind == NoticeKind::PowerOffRefused);
  TEST_ASSERT_TRUE(d.core.toggleFrontlight());
  TEST_ASSERT_TRUE(d.core.state().frontlight);
  d.core.setFrontlight(false);
  TEST_ASSERT_FALSE(d.core.state().frontlight);
}

void test_wifi_stays_on_while_it_is_used() {
  Device d;
  d.core.markAllRead();
  TEST_ASSERT_TRUE(d.core.setWifiMode(WifiMode::AccessPoint));
  d.run(9 * 60);
  d.core.wifiController()->noteActivity(d.clock.uptimeSeconds());   // what the web service does for every authenticated request
  d.run(9 * 60);
  TEST_ASSERT_TRUE(d.core.state().wifiPhase == WifiPhase::AccessPoint);
  TEST_ASSERT_FALSE(d.core.poll(false, 10000).cpuMaySleep);          // running Wi-Fi keeps the processor awake
  d.run(2 * 60);
  TEST_ASSERT_TRUE(d.core.state().wifiPhase == WifiPhase::Off);
  TEST_ASSERT_TRUE(d.core.wifi()->reason() == WifiReason::SwitchedOffByItself);
  TEST_ASSERT_TRUE(d.core.state().wifiReason == WifiReason::SwitchedOffByItself);
}

void test_the_position_page_and_the_test_page_hold_the_receiver() {
  Device d;
  d.run(200);                                                        // the start-up fix is over
  TEST_ASSERT_FALSE(d.core.state().positioningActive);
  d.core.setPositionPageOpen(true);
  TEST_ASSERT_TRUE(d.poll().positioningActive);
  d.core.setTestPageOpen(true);
  d.core.setPositionPageOpen(false);
  TEST_ASSERT_TRUE(d.poll().positioningActive);                      // one page closing does not take it from the other
  d.core.setTestPageOpen(false);
  TEST_ASSERT_FALSE(d.poll().positioningActive);
}

void test_radio_contacts_are_listed() {
  Device d(ProfileBuilder().radio(true).build(), true);
  RadioPeer peer;
  TEST_ASSERT_EQUAL(1, d.core.radioPeerCount());
  TEST_ASSERT_TRUE(d.core.radioPeer(0, peer));
  TEST_ASSERT_EQUAL_STRING("Node One", peer.name.c_str());
  TEST_ASSERT_FALSE(d.core.radioPeer(1, peer));
  Device e;                                                          // no radio: none
  TEST_ASSERT_EQUAL(0, e.core.radioPeerCount());
  TEST_ASSERT_FALSE(e.core.radioPeer(0, peer));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_poll_drains_everything);
  RUN_TEST(test_the_state_offers_what_the_interface_draws);
  RUN_TEST(test_a_full_day);
  RUN_TEST(test_an_sos_in_school_time_with_the_override);
  RUN_TEST(test_a_remote_query_during_an_sos);
  RUN_TEST(test_the_state_tells_how_the_sos_search_is_going_and_to_whom);
  RUN_TEST(test_a_remote_query_alone_is_answered_without_the_child_noticing);
  RUN_TEST(test_the_network_dropping_and_returning);
  RUN_TEST(test_save_mode_and_the_cell_probe);
  RUN_TEST(test_run_time_settings_change_the_profile);
  RUN_TEST(test_the_trigger_words_come_from_every_language);
  RUN_TEST(test_the_clock_follows_the_time_zone);
  RUN_TEST(test_the_state_has_the_wall_clock_as_numbers);
  RUN_TEST(test_a_stored_time_is_shown_on_the_wall_clock_of_the_zone);
  RUN_TEST(test_without_a_known_time_the_zone_is_left_alone);
  RUN_TEST(test_the_core_produces_only_codes);
  RUN_TEST(test_a_test_pattern_can_be_played);
  RUN_TEST(test_the_modem_that_reports_something_is_awake);
  RUN_TEST(test_work_in_hand_keeps_the_device_awake);
  RUN_TEST(test_the_history_and_the_wifi_settings_are_loaded);
  RUN_TEST(test_the_host_gets_the_decision_and_the_requests);
  RUN_TEST(test_wifi_stays_on_while_it_is_used);
  RUN_TEST(test_the_position_page_and_the_test_page_hold_the_receiver);
  RUN_TEST(test_radio_contacts_are_listed);
  return UNITY_END();
}
