// Host tests for the Runtime: the same wiring the device and the simulator run, on the fakes.
// A boot, an incoming message, a call, an SOS, and a change of language at run time.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <memory>
#include <string>

#include <FrameBuffer.h>
#include <Languages.h>
#include <SimPanel.h>
#include <boot/Runtime.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeConfigStore.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakeMotion.h>
#include <fakes/FakePower.h>
#include <fakes/FakeRadioLink.h>
#include <fakes/FakeTcpServer.h>
#include <fakes/FakeWifi.h>

void setUp() {}
void tearDown() {}

namespace {

constexpr uint32_t kTuesdayAfternoon = 1789475520;  // 2026-09-15 12:32 UTC, 14:32 at +02:00
constexpr const char* kParentOne = "+491701111111";

class PageAssets : public WebAssets {
public:
  const char* indexHtml(size_t& length) override {
    length = sizeof(kPage) - 1;
    return kPage;
  }
private:
  static constexpr char kPage[] = "<html>test page</html>";
};

std::string settings(const std::string& language = "en") {
  return "[child]\nname = \"Alex\"\n[interface]\nlanguage = \"" + language + "\"\n[sms]\nlanguage = \"en\"\n"
         "[time]\ntimezone = \"Europe/Berlin\"\n"
         "[[tiles]]\nlabel = \"Parent One\"\naction = \"call\"\ntarget = \"+491701111111\"\n";
}

// A device with a card that has the three files, the network up and the time known, as the
// firmware finds it a minute after switching on. `display` is where the frames go.
struct World {
  FakeClock clock;
  FakePower power{&clock};
  FakeModem modem{clock};
  FakeGnss gnss{power, clock};
  FakeHaptic haptic{clock};
  FakeEventLog log{&clock};
  FakeConfigStore store;
  FakeWifi wifi;
  FakeTcpServer tcp;
  FakeRadioLink radio;
  FakeMotion motion;
  PageAssets assets;
  i18n::Translator translator = languages::loaded();
  std::unique_ptr<boot::Runtime> runtime;
  uint32_t ms = 0;
  PowerDecision decision;  // what the last step said about sleeping

  World() {
    clock.setTzOffsetMinutes(120);
    clock.advanceSeconds(100);
    clock.setEpochUtc(kTuesdayAfternoon);
    power.setEnabled(PowerDomain::Cellular, true);
    modem.begin();
    modem.setNetwork("Example Mobile", 4);
    gnss.fixAfter(2, 48.13745, 11.5755, 1.5);
    store.put("/config/settings.toml", settings());
    store.put("/config/contacts.toml", std::string("[[contact]]\nnumber = \"") + kParentOne + "\"\nname = \"Parent One\"\nrole = \"parent\"\n");
    store.put("/config/secrets.toml", "web_password = \"test-password-1\"\n");
  }

  void boot(ui::Display& display) {
    ms = static_cast<uint32_t>(clock.uptimeMillis());
    runtime = std::make_unique<boot::Runtime>(
        boot::Capabilities{clock, power, modem, gnss, haptic, log, store, wifi, tcp, assets, display,
                           [] { return 12345678u; }, "0.0.0-test", &radio, &motion},
        translator, ms);
  }

  // Seconds pass, the loop turns once a second.
  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      clock.advanceSeconds(1);
      ms = static_cast<uint32_t>(clock.uptimeMillis());
      decision = runtime->step(ms);
    }
  }
  AppCore::State state() { return runtime->core().state(); }
};

}  // namespace

// ---- boot --------------------------------------------------------------------------------------------

void test_it_boots_on_the_fakes_and_shows_the_home_screen_in_the_configured_language() {
  World w;
  w.store.put("/config/settings.toml", settings("de"));
  FrameBuffer fb;
  w.boot(fb);
  w.runtime->step(w.ms);
  TEST_ASSERT_TRUE(w.runtime->diagnostics().empty());
  TEST_ASSERT_EQUAL_STRING("de", w.translator.language().c_str());
  TEST_ASSERT_EQUAL(ui::screen::kHome, w.runtime->stack().visibleId());
  TEST_ASSERT_TRUE(w.state().registered);
  TEST_ASSERT_TRUE(w.state().positioningActive);   // start() asked for the start-up fix
  TEST_ASSERT_TRUE(fb.blackPixels() > 500);      // the home screen is on the display
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
}

void test_the_frames_go_through_the_refresh_policy() {
  World w;
  sim::SimPanel panel([&] { return w.ms; });
  w.boot(panel);
  w.run(10);                                     // the start-up fix comes and goes: the picture changes
  TEST_ASSERT_TRUE(panel.refreshes()[0].full);   // the first picture is a full refresh
  const size_t settled = panel.refreshes().size();
  const uint32_t frames = panel.stats().frames;
  w.run(60);                                     // then nothing changes for a minute
  TEST_ASSERT_EQUAL(settled, panel.refreshes().size());
  TEST_ASSERT_EQUAL(frames + 60, panel.stats().frames);  // although it was drawn again every second
}

void test_a_card_with_nothing_on_it_boots_with_defaults_and_says_what_is_missing() {
  World w;
  FakeConfigStore empty;
  w.store = empty;
  FrameBuffer fb;
  w.boot(fb);
  w.runtime->step(w.ms);
  TEST_ASSERT_EQUAL_STRING("en", w.translator.language().c_str());
  size_t missing = 0;
  for (const config::Diagnostic& d : w.runtime->diagnostics()) missing += d.code == config::Diagnostic::Code::MissingFile;
  TEST_ASSERT_EQUAL(3, static_cast<int>(missing));
  TEST_ASSERT_TRUE(w.log.contains("config: missing-file"));
}

void test_an_unknown_interface_language_falls_back_to_english_with_a_diagnostic() {
  World w;
  w.store.put("/config/settings.toml", settings("xx"));
  FrameBuffer fb;
  w.boot(fb);
  TEST_ASSERT_EQUAL_STRING("en", w.translator.language().c_str());
  TEST_ASSERT_EQUAL(1, static_cast<int>(w.runtime->diagnostics().size()));
  const config::Diagnostic& d = w.runtime->diagnostics()[0];
  TEST_ASSERT_TRUE(d.code == config::Diagnostic::Code::BadCatalogCode);
  TEST_ASSERT_EQUAL_STRING("interface.language", d.args[1].c_str());
  TEST_ASSERT_EQUAL_STRING("xx", d.args[2].c_str());
}

void test_a_user_catalog_on_the_card_can_be_the_language_of_the_device() {
  // The catalog is a copy of English under another code: the point is that /lang/ is read before
  // the configuration asks for its language.
  World w;
  std::string text;
  {
    FILE* f = std::fopen("lang/en.toml", "rb");
    TEST_ASSERT_NOT_NULL(f);
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);
  }
  const std::string codeLine = "code   = \"en\"";
  const size_t at = text.find(codeLine);
  TEST_ASSERT_TRUE_MESSAGE(at != std::string::npos, "lang/en.toml has no code line this test can rename");
  text.replace(at, codeLine.size(), "code   = \"xx\"");
  w.store.put("/lang/xx.toml", text);
  w.store.put("/config/settings.toml", settings("xx"));
  FrameBuffer fb;
  w.boot(fb);
  size_t catalogs = 0;
  for (const config::Diagnostic& d : w.runtime->diagnostics()) catalogs += d.code == config::Diagnostic::Code::UserCatalogError;
  TEST_ASSERT_EQUAL_MESSAGE(0, static_cast<int>(catalogs), "the user catalog was refused");
  TEST_ASSERT_EQUAL_STRING("xx", w.translator.language().c_str());
}

// ---- the world reaches the screen ---------------------------------------------------------------------

void test_an_incoming_message_is_counted_and_changes_the_next_picture() {
  World w;
  sim::SimPanel panel([&] { return w.ms; });
  w.boot(panel);
  w.run(3);
  const size_t before = panel.refreshes().size();
  w.modem.injectSms(kParentOne, "Hello Alex", kTuesdayAfternoon);
  w.run(2);
  TEST_ASSERT_EQUAL(1, static_cast<int>(w.state().unread));
  TEST_ASSERT_EQUAL(before + 1, panel.refreshes().size());
  TEST_ASSERT_FALSE(panel.refreshes().back().full);   // the badge on the tile: a partial refresh
}

void test_an_incoming_call_takes_the_screen_and_gives_it_back() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(2);
  w.modem.injectIncomingCall(kParentOne);
  w.run(1);
  TEST_ASSERT_EQUAL(ui::screen::kCall, w.runtime->stack().visibleId());
  w.modem.remoteHangup();
  w.run(1);
  TEST_ASSERT_EQUAL(ui::screen::kHome, w.runtime->stack().visibleId());
}

void test_an_incoming_call_comes_through_the_lock() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.runtime->stack().lock();
  TEST_ASSERT_TRUE(w.runtime->stack().locked());
  w.modem.injectIncomingCall(kParentOne);
  w.run(1);
  TEST_ASSERT_FALSE(w.runtime->stack().locked());
  TEST_ASSERT_EQUAL(ui::screen::kCall, w.runtime->stack().visibleId());
}

void test_the_device_locks_itself_after_the_auto_lock_time() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(boot::Runtime::kAutoLockSeconds - 1);
  TEST_ASSERT_FALSE(w.runtime->stack().locked());
  w.run(2);
  TEST_ASSERT_TRUE(w.runtime->stack().locked());
}

void test_a_key_press_is_input_for_the_screen_and_the_power_logic() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(1);
  w.runtime->input(ui::Input::pressed(ui::Key::Inbox), w.ms);   // a hardware key: the inbox
  TEST_ASSERT_EQUAL(ui::screen::kMessages, w.runtime->stack().visibleId());
  w.runtime->input(ui::Input::pressed(ui::Key::Home), w.ms);
  TEST_ASSERT_EQUAL(ui::screen::kHome, w.runtime->stack().visibleId());
  w.runtime->input(ui::Input::pressed(ui::Key::Compose), w.ms);
  TEST_ASSERT_EQUAL(ui::screen::kRecipient, w.runtime->stack().visibleId());
}

void test_a_key_press_starts_the_quiet_time_again() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(61);
  TEST_ASSERT_TRUE(w.decision.modemShouldSleep);   // a minute without a key: the modem may sleep
  w.runtime->input(ui::Input::pressed(ui::Key::Down), w.ms);
  w.decision = w.runtime->step(w.ms);
  TEST_ASSERT_FALSE(w.decision.modemShouldSleep);
}

void test_the_processor_may_sleep_only_while_the_device_is_locked() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(61);
  TEST_ASSERT_FALSE(w.decision.cpuMaySleep);       // somebody may be looking
  w.runtime->stack().lock();
  w.run(1);
  TEST_ASSERT_TRUE(w.decision.cpuMaySleep);
  TEST_ASSERT_TRUE(w.decision.cpuSleepMs > 0);
}

void test_input_is_drawn_at_once_not_at_the_next_second() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(1);
  const uint32_t frames = static_cast<uint32_t>(fb.frames());
  w.runtime->input(ui::Input::pressed(ui::Key::Inbox), w.ms);
  w.runtime->step(w.ms + 10);                    // ten milliseconds later, well inside the second
  TEST_ASSERT_EQUAL(frames + 1, static_cast<uint32_t>(fb.frames()));
}

void test_an_sos_runs_through_the_real_wiring_and_reaches_the_parent() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(2);
  TEST_ASSERT_TRUE(w.runtime->core().beginSos());
  w.run(1);
  TEST_ASSERT_TRUE(w.state().sosPhase != SosFlow::Phase::Idle);  // the countdown
  w.run(90);
  TEST_ASSERT_EQUAL(1, static_cast<int>(w.modem.sent().size()));
  TEST_ASSERT_EQUAL_STRING(kParentOne, w.modem.sent()[0].phone.c_str());
  TEST_ASSERT_TRUE(w.modem.sent()[0].body.find("Alex sends SOS") != std::string::npos);
}

// ---- language -----------------------------------------------------------------------------------------

void test_a_change_of_language_at_run_time_changes_the_next_frame_and_nothing_else() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(1);
  const FrameBuffer english = fb;
  const size_t unread = w.state().unread;
  TEST_ASSERT_TRUE(w.runtime->setLanguage("de"));
  w.runtime->step(w.ms + 10);                                       // inside the second: the change is drawn at once
  TEST_ASSERT_FALSE(fb == english);
  TEST_ASSERT_EQUAL(unread, w.state().unread);
  TEST_ASSERT_EQUAL(ui::screen::kHome, w.runtime->stack().visibleId());

  // The picture is the one a device configured in German draws.
  World german;
  german.store.put("/config/settings.toml", settings("de"));
  FrameBuffer reference;
  german.boot(reference);
  german.run(1);
  TEST_ASSERT_TRUE(fb == reference);
}

void test_an_unknown_language_changes_nothing() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.run(1);
  const FrameBuffer before = fb;
  TEST_ASSERT_FALSE(w.runtime->setLanguage("zz"));
  w.runtime->step(w.ms + 10);
  TEST_ASSERT_TRUE(fb == before);
  TEST_ASSERT_EQUAL_STRING("en", w.translator.language().c_str());
}

void test_the_shipped_languages_draw_the_home_screen_without_cut_off() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    if (language == i18n::kPseudoCode) return;  // longer than any real language on purpose
    World w;
    w.store.put("/config/settings.toml", settings(language));
    FrameBuffer fb;
    w.boot(fb);
    w.run(1);
    TEST_ASSERT_EQUAL_STRING(language.c_str(), w.translator.language().c_str());
    TEST_ASSERT_TRUE_MESSAGE(fb.blackPixels() > 500, language.c_str());
    TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + "\n" + fb.describeClips()).c_str());
  });
}

void test_the_three_languages_draw_three_different_pictures() {
  FrameBuffer pictures[3];
  const char* codes[3] = {"en", "de", "fr"};
  for (int i = 0; i < 3; ++i) {
    World w;
    w.store.put("/config/settings.toml", settings(codes[i]));
    w.boot(pictures[i]);
    w.run(1);
  }
  TEST_ASSERT_FALSE(pictures[0] == pictures[1]);
  TEST_ASSERT_FALSE(pictures[0] == pictures[2]);
  TEST_ASSERT_FALSE(pictures[1] == pictures[2]);
}

// ---- the other things the host does ---------------------------------------------------------------------

void test_the_web_service_is_the_real_one_and_answers_through_the_tcp_fake() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  w.runtime->core().setWifiMode(WifiMode::Station);
  w.run(1);
  TEST_ASSERT_TRUE(w.runtime->web().wifi().request(WifiMode::AccessPoint, w.ms / 1000));
  w.run(2);
  TEST_ASSERT_TRUE(w.tcp.listening());
  FakeTcpConnection* client = w.tcp.connect();
  TEST_ASSERT_NOT_NULL(client);
  client->fromPeer("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
  w.run(2);
  TEST_ASSERT_TRUE(client->sentToPeer().find("test page") != std::string::npos);
}

void test_a_restart_asked_by_the_web_interface_reaches_the_host() {
  World w;
  FrameBuffer fb;
  w.boot(fb);
  TEST_ASSERT_FALSE(w.runtime->restartRequested());
  w.runtime->web().requestRestart();
  TEST_ASSERT_TRUE(w.runtime->restartRequested());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_it_boots_on_the_fakes_and_shows_the_home_screen_in_the_configured_language);
  RUN_TEST(test_the_frames_go_through_the_refresh_policy);
  RUN_TEST(test_a_card_with_nothing_on_it_boots_with_defaults_and_says_what_is_missing);
  RUN_TEST(test_an_unknown_interface_language_falls_back_to_english_with_a_diagnostic);
  RUN_TEST(test_a_user_catalog_on_the_card_can_be_the_language_of_the_device);
  RUN_TEST(test_an_incoming_message_is_counted_and_changes_the_next_picture);
  RUN_TEST(test_an_incoming_call_takes_the_screen_and_gives_it_back);
  RUN_TEST(test_an_incoming_call_comes_through_the_lock);
  RUN_TEST(test_the_device_locks_itself_after_the_auto_lock_time);
  RUN_TEST(test_a_key_press_is_input_for_the_screen_and_the_power_logic);
  RUN_TEST(test_a_key_press_starts_the_quiet_time_again);
  RUN_TEST(test_the_processor_may_sleep_only_while_the_device_is_locked);
  RUN_TEST(test_input_is_drawn_at_once_not_at_the_next_second);
  RUN_TEST(test_an_sos_runs_through_the_real_wiring_and_reaches_the_parent);
  RUN_TEST(test_a_change_of_language_at_run_time_changes_the_next_frame_and_nothing_else);
  RUN_TEST(test_an_unknown_language_changes_nothing);
  RUN_TEST(test_the_shipped_languages_draw_the_home_screen_without_cut_off);
  RUN_TEST(test_the_three_languages_draw_three_different_pictures);
  RUN_TEST(test_the_web_service_is_the_real_one_and_answers_through_the_tcp_fake);
  RUN_TEST(test_a_restart_asked_by_the_web_interface_reaches_the_host);
  return UNITY_END();
}
