// Host tests for the Wi-Fi page: the three buttons and which one is marked, what somebody needs to
// join, why it did not work, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/OptionRows.h>
#include <ui/PageTitle.h>
#include <ui/Text.h>
#include <ui/screens/WifiScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

Profile withHomeNetwork() {
  Profile profile = ProfileBuilder().childName("Alex").parentTools(true).build();
  profile.wifi.addNetwork("Home", "password1");
  return profile;
}

struct Page {
  DeviceRig rig;
  WifiScreen screen;
  Page() : rig(withHomeNetwork()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    rig.poll();
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void run(uint32_t seconds) {
    rig.run(seconds);
    draw();
  }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  // Moves the focus to button `index` and presses it.
  void choose(int index) {
    for (int i = 0; i < 2; ++i) screen.handle(Input::pressed(Key::Up));
    for (int i = 0; i < index; ++i) screen.handle(Input::pressed(Key::Down));
    key(Key::Select);
  }
  bool says(const std::string& words, FontSize size = FontSize::Small) { return shows::text(rig.frame, words, size, kContent); }
  // A sentence that wraps under the buttons: every line of it.
  bool saysWrapped(const std::string& words) {
    const auto lines = text::wrap(font(FontSize::Small), words, kContent.w - 12);
    for (std::string_view line : lines)
      if (!says(std::string(line))) return false;
    return !lines.empty();
  }
  Rect button(size_t index) {
    OptionRows probe;
    probe.setRows({{"a", "", false}});
    const int header = pageTitleHeight(kContent, rig.translator.get("wifi.title"));
    return probe.rowRect({kContent.x, kContent.y + header, kContent.w, kContent.h - header}, index);
  }
  // Has the button the second outline of "what runs now"? Its pixel is black on white, white on the focus.
  bool marked(size_t index, bool focused = false) {
    const Rect box = button(index);
    return rig.frame.pixel(box.x + 3, box.y + box.h / 2) == !focused;
  }
};

}  // namespace

// ---- the buttons ----------------------------------------------------------------------------------------

void test_the_page_has_three_buttons_and_marks_the_one_that_runs() {
  Page page;
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Wi-Fi", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Home network", FontSize::Text, kContent, true));   // the focus
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Access point", FontSize::Text, kContent));
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Off", FontSize::Text, kContent));
  TEST_ASSERT_FALSE(page.marked(0, true));
  TEST_ASSERT_FALSE(page.marked(1));
  TEST_ASSERT_TRUE(page.marked(2));                                    // Wi-Fi is off
}

void test_the_access_point_shows_what_somebody_needs_to_join_it() {
  Page page;
  page.choose(1);
  page.run(2);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::AccessPoint);
  TEST_ASSERT_TRUE(page.marked(1, true));
  TEST_ASSERT_FALSE(page.marked(2));
  TEST_ASSERT_TRUE(page.says("Network: phone-os"));
  TEST_ASSERT_TRUE(page.says("Password: 12345678"));                    // only the device's screen shows it
  TEST_ASSERT_TRUE(page.says("Page: http://192.168.4.1"));
  TEST_ASSERT_TRUE(page.says("Off in 10 min"));
  page.run(61);
  TEST_ASSERT_TRUE(page.says("Off in 9 min"));                          // a started minute counts as a whole one
}

void test_the_home_network_is_shown_while_it_connects_and_when_it_has_connected_without_a_password() {
  Page page;
  page.choose(0);
  page.run(1);
  TEST_ASSERT_TRUE(page.says("Connecting to Home ..."));
  TEST_ASSERT_TRUE(page.marked(0, true));
  page.rig.wifi.finishConnecting(true);
  page.run(2);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Connected);
  TEST_ASSERT_TRUE(page.says("Network: Home"));
  TEST_ASSERT_TRUE(page.says("Page: http://192.168.1.50"));
  TEST_ASSERT_FALSE(page.says("Password: password1"));                  // a home network's password is never shown
  TEST_ASSERT_FALSE(page.says("Password:"));                            // and there is no line for one
  TEST_ASSERT_TRUE(page.says("Off in 10 min"));
}

void test_a_home_network_that_cannot_be_reached_says_so_and_points_to_the_access_point() {
  Page page;
  page.choose(0);
  page.run(1);
  page.rig.wifi.finishConnecting(false);
  page.run(3);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Failed);
  TEST_ASSERT_TRUE(page.says("Home network not reachable"));
  TEST_ASSERT_TRUE(page.says("The access point still works."));
  TEST_ASSERT_TRUE(page.marked(2));                                    // nothing runs
}

void test_without_a_stored_network_the_reason_is_told_and_nothing_runs() {
  Page page(ProfileBuilder().childName("Alex").parentTools(true).build());
  page.choose(0);
  TEST_ASSERT_FALSE(page.rig.core.state().wifiPhase != WifiPhase::Off && page.rig.core.state().wifiPhase != WifiPhase::Failed);
  TEST_ASSERT_TRUE(page.says("No home network saved"));
  TEST_ASSERT_TRUE(page.marked(2));
}

void test_a_wifi_that_switched_itself_off_says_why_instead_of_the_explanation() {
  Page page;
  page.choose(1);
  page.run(11 * 60);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Off);
  TEST_ASSERT_TRUE(page.says("Switched off: not used"));
  TEST_ASSERT_FALSE(page.saysWrapped("Wi-Fi is off. With it the parents' page works: contacts and settings."));
}

void test_off_explains_what_wifi_is_for() {
  Page page;
  TEST_ASSERT_TRUE(page.saysWrapped("Wi-Fi is off. With it the parents' page works: contacts and settings."));
}

void test_a_failing_access_point_gives_its_reason() {
  Page page;
  page.rig.wifi.failAccessPoint(true);
  page.choose(1);
  TEST_ASSERT_TRUE(page.says("Access point does not start"));
  TEST_ASSERT_TRUE(page.marked(2));
}

void test_the_off_button_switches_it_off_and_the_core_refuses_a_low_battery_in_the_notice() {
  Page page;
  page.choose(1);
  page.run(2);
  page.choose(2);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Off);
  TEST_ASSERT_TRUE(page.marked(2, true));
  page.rig.power.setBattery(10);
  page.choose(1);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Off);
  TEST_ASSERT_EQUAL_STRING("Battery too low for Wi-Fi", page.screen.noticeLine().c_str());
}

// ---- keys and touch -----------------------------------------------------------------------------------------

void test_the_soft_keys_and_back() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_a_tap_on_a_button_chooses_it() {
  Page page;
  const Rect ap = page.button(1);
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(ap.x + ap.w / 2, ap.y + ap.h / 2)));
  page.rig.run(2);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::AccessPoint);
}

void test_the_arrows_stop_at_both_ends() {
  Page page;
  page.key(Key::Up);
  page.key(Key::Select);                                                // still the first: the home network
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Connecting);
  for (int i = 0; i < 5; ++i) page.key(Key::Down);
  page.key(Key::Select);                                                // the last: off
  page.run(1);
  TEST_ASSERT_TRUE(page.rig.core.state().wifiPhase == WifiPhase::Off);
}

void test_the_page_opens_afresh_with_the_focus_on_the_first_button() {
  Page page;
  page.key(Key::Down);
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Home network", FontSize::Text, kContent, true));
}

// ---- layout ----------------------------------------------------------------------------------------------------

void test_every_state_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 5; ++state) {
      Page page;
      page.rig.language(language);
      if (state == 1) { page.choose(1); page.run(2); }
      if (state == 2) { page.choose(0); page.run(1); }
      if (state == 3) { page.choose(0); page.run(1); page.rig.wifi.finishConnecting(false); page.run(3); }
      if (state == 4) { page.choose(1); page.run(11 * 60); }
      page.draw();
      FrameBuffer fb;
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/ui/screen-wifi.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " state " + std::to_string(state) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page off;
    off.rig.language(language);
    off.draw();
    golden::expect(off.rig.frame, std::string("wifi_off_") + language);
    Page ap;
    ap.rig.language(language);
    ap.choose(1);
    ap.run(2);
    golden::expect(ap.rig.frame, std::string("wifi_ap_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_page_has_three_buttons_and_marks_the_one_that_runs);
  RUN_TEST(test_the_access_point_shows_what_somebody_needs_to_join_it);
  RUN_TEST(test_the_home_network_is_shown_while_it_connects_and_when_it_has_connected_without_a_password);
  RUN_TEST(test_a_home_network_that_cannot_be_reached_says_so_and_points_to_the_access_point);
  RUN_TEST(test_without_a_stored_network_the_reason_is_told_and_nothing_runs);
  RUN_TEST(test_a_wifi_that_switched_itself_off_says_why_instead_of_the_explanation);
  RUN_TEST(test_off_explains_what_wifi_is_for);
  RUN_TEST(test_a_failing_access_point_gives_its_reason);
  RUN_TEST(test_the_off_button_switches_it_off_and_the_core_refuses_a_low_battery_in_the_notice);
  RUN_TEST(test_the_soft_keys_and_back);
  RUN_TEST(test_a_tap_on_a_button_chooses_it);
  RUN_TEST(test_the_arrows_stop_at_both_ends);
  RUN_TEST(test_the_page_opens_afresh_with_the_focus_on_the_first_button);
  RUN_TEST(test_every_state_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
