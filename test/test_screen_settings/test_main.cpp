// Host tests for the options page: which settings it offers to whom, what each one does, the
// values it shows, the touch, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/OptionRows.h>
#include <ui/PageTitle.h>
#include <ui/Text.h>
#include <ui/screens/SettingsScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

Profile parents() { return ProfileBuilder().childName("Alex").parentTools(true).build(); }

struct Page {
  DeviceRig rig;
  SettingsScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    rig.poll();
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  void down(int times = 1) {
    for (int i = 0; i < times; ++i) key(Key::Down);
  }
  // A box: its question in the small font, its answer in the text font; the one in focus is inverted.
  bool says(const std::string& label, const std::string& value, bool focused = false) {
    return shows::text(rig.frame, label, FontSize::Small, kContent, focused) && shows::text(rig.frame, value, FontSize::Text, kContent, focused);
  }
  // The centre of box `index` as drawn at the top of the list.
  Input tap(size_t index) {
    OptionRows probe;
    probe.setRows({{"a", "b", false}});
    const int header = pageTitleHeight(kContent, rig.translator.get("settings.title"));
    const Rect box = probe.rowRect({kContent.x, kContent.y + header, kContent.w, kContent.h - header}, index);
    return Input::touch(box.x + box.w / 2, box.y + box.h / 2);
  }
};

size_t played(const DeviceRig& rig, HapticPattern pattern) {
  size_t n = 0;
  for (HapticPattern p : rig.haptic.played()) n += p == pattern ? 1 : 0;
  return n;
}

}  // namespace

// ---- what is offered ------------------------------------------------------------------------------

void test_the_child_sees_volume_vibration_and_language() {
  Page page;
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Options", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(page.says("Volume in calls", "4 of 7", true));        // the first box has the focus
  TEST_ASSERT_TRUE(page.says("Message vibration", "on"));
  TEST_ASSERT_TRUE(page.says("Language", "English"));
  TEST_ASSERT_FALSE(shows::text(page.rig.frame, "Save mode", FontSize::Small, kContent));   // removed with version 4.0
  TEST_ASSERT_FALSE(shows::text(page.rig.frame, "Wi-Fi", FontSize::Small, kContent));     // the parents' tool
}

void test_the_parents_tools_add_wifi_and_say_whether_it_runs() {
  Page page(parents());
  TEST_ASSERT_TRUE(page.says("Wi-Fi", "off"));
  TEST_ASSERT_TRUE(page.rig.core.setWifiMode(WifiMode::AccessPoint));
  page.rig.run(2);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Wi-Fi", "on"));
}

void test_a_device_without_wifi_offers_none_even_to_the_parents() {
  DeviceRig rig(parents());
  AppCore bare(rig.clock, rig.power, rig.modem, rig.gnss, rig.haptic, rig.log, rig.translator, parents());   // no setWifi
  bare.start();
  SettingsScreen screen(ScreenContext{bare, rig.translator, rig.nav, rig.session});
  screen.onEnter();
  screen.render(rig.frame);
  TEST_ASSERT_TRUE(shows::text(rig.frame, "Language", FontSize::Small, kContent));
  TEST_ASSERT_FALSE(shows::text(rig.frame, "Wi-Fi", FontSize::Small, kContent));
}

void test_a_modem_without_a_volume_control_has_no_volume_box() {
  Page page;
  page.rig.modem.setVolumeSupported(false);
  page.draw();
  TEST_ASSERT_FALSE(shows::text(page.rig.frame, "Volume in calls", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(page.says("Message vibration", "on", true));     // the first box is now the vibration
  page.key(Key::Select);                                                // and the keys act on what is shown
  TEST_ASSERT_FALSE(page.rig.core.messageVibration());
}

// ---- what each one does -----------------------------------------------------------------------------

void test_the_volume_goes_round_from_one_to_seven_and_never_to_silence() {
  Page page;
  int expected[] = {5, 6, 7, 1, 2};
  for (int level : expected) {
    page.key(Key::Select);
    TEST_ASSERT_EQUAL(level, page.rig.modem.callVolume());
    TEST_ASSERT_TRUE(page.says("Volume in calls", std::to_string(level) + " of 7", true));
  }
}

void test_a_silent_volume_goes_to_one_not_to_two() {
  Page page;
  page.rig.modem.setCallVolume(0);                                      // set by somebody else, e.g. a call
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(1, page.rig.modem.callVolume());
}

void test_message_vibration_switches_and_buzzes_once_when_it_goes_on() {
  Page page;
  page.down();
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.rig.core.messageVibration());
  TEST_ASSERT_TRUE(page.says("Message vibration", "off", true));
  TEST_ASSERT_EQUAL(0, played(page.rig, HapticPattern::Message));       // turning it off is silent
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.messageVibration());
  TEST_ASSERT_EQUAL(1, played(page.rig, HapticPattern::Message));       // turning it on lets it be felt
  TEST_ASSERT_TRUE(page.says("Message vibration", "on", true));
}

void test_the_wifi_and_language_boxes_open_their_pages() {
  Page page(parents());
  page.down(2);
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kWifi));
  page.down();
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kLanguage));
}

void test_the_language_box_names_the_language_in_use_by_its_own_name() {
  Page page;
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Sprache", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Deutsch", FontSize::Text, kContent));
  page.rig.language("fr");
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Fran\xC3\xA7" "ais", FontSize::Text, kContent));
}

// ---- keys and touch -----------------------------------------------------------------------------------

void test_the_arrows_move_the_focus_and_stop_at_the_ends() {
  Page page;
  page.key(Key::Up);
  TEST_ASSERT_TRUE(page.says("Volume in calls", "4 of 7", true));
  page.down(10);
  TEST_ASSERT_TRUE(page.says("Switch off", "press twice", true));    // the last box
}

// Owner, 2026-10-08: switching off was only on the parents' status page, and a device that is off uses no power.
// The last box switches off with two presses; anything else takes the first back.
void test_the_last_box_switches_off_with_two_presses() {
  Page page;
  page.down(10);
  TEST_ASSERT_TRUE(page.says("Switch off", "press twice", true));
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
  TEST_ASSERT_TRUE(page.says("Switch off", "press again: off", true));
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.powerOffRequested());

  Page other;
  other.down(10);
  other.key(Key::Select);
  other.key(Key::Up);                                                // another box: the first press is taken back
  other.key(Key::Down);
  TEST_ASSERT_TRUE(other.says("Switch off", "press twice", true));
  other.key(Key::Select);
  TEST_ASSERT_FALSE(other.rig.core.powerOffRequested());
  other.screen.onLeave();                                            // leaving the page takes it back too
  other.screen.onEnter();
  other.down(10);
  other.key(Key::Select);
  TEST_ASSERT_FALSE(other.rig.core.powerOffRequested());
}

// On the cable the board stays on whatever the device does: the box says to unplug and nothing is asked.
void test_on_the_cable_the_box_says_unplug_first_and_does_nothing() {
  Page page;
  page.rig.power.plugIn(ChargeState::Charging);
  page.rig.poll();
  page.down(10);
  TEST_ASSERT_TRUE(page.says("Switch off", "unplug USB first", true));
  page.key(Key::Select);
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_back_goes_back_and_the_soft_keys_say_what_they_do() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Change", page.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_a_tap_on_a_box_changes_it() {
  Page page;
  TEST_ASSERT_TRUE(page.screen.handle(page.tap(1)));
  TEST_ASSERT_FALSE(page.rig.core.messageVibration());                  // the second box: vibration
  TEST_ASSERT_TRUE(page.screen.handle(page.tap(0)));
  TEST_ASSERT_EQUAL(5, page.rig.modem.callVolume());
  TEST_ASSERT_FALSE(page.screen.handle(Input::touch(120, kContent.y + 2)));     // the title
}

void test_the_pages_open_afresh_with_the_focus_on_the_first_box() {
  Page page;
  page.down(3);
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Volume in calls", "4 of 7", true));
}

// ---- layout -----------------------------------------------------------------------------------------------

void test_every_page_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int variant = 0; variant < 2; ++variant) {
      Page page(variant == 0 ? ProfileBuilder().childName("Alex").build() : parents());
      page.rig.language(language);
      for (int focus = 0; focus < 4; ++focus) {
        page.draw();
        FrameBuffer fb;
        page.screen.render(fb);
        // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
        // into the soft-key bar there (see docs/ui/screen-settings.md), the page itself may not.
        bool outside = false;
        for (const auto& e : fb.clipEvents())
          if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
        TEST_ASSERT_FALSE_MESSAGE(outside, (language + " variant " + std::to_string(variant) + " focus " + std::to_string(focus) + ":\n" + fb.describeClips()).c_str());
        page.down();
      }
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page(parents());
    page.rig.language(language);
    page.draw();
    golden::expect(page.rig.frame, std::string("settings_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_child_sees_volume_vibration_and_language);
  RUN_TEST(test_the_parents_tools_add_wifi_and_say_whether_it_runs);
  RUN_TEST(test_a_device_without_wifi_offers_none_even_to_the_parents);
  RUN_TEST(test_a_modem_without_a_volume_control_has_no_volume_box);
  RUN_TEST(test_the_volume_goes_round_from_one_to_seven_and_never_to_silence);
  RUN_TEST(test_a_silent_volume_goes_to_one_not_to_two);
  RUN_TEST(test_message_vibration_switches_and_buzzes_once_when_it_goes_on);
  RUN_TEST(test_the_wifi_and_language_boxes_open_their_pages);
  RUN_TEST(test_the_language_box_names_the_language_in_use_by_its_own_name);
  RUN_TEST(test_the_arrows_move_the_focus_and_stop_at_the_ends);
  RUN_TEST(test_the_last_box_switches_off_with_two_presses);
  RUN_TEST(test_on_the_cable_the_box_says_unplug_first_and_does_nothing);
  RUN_TEST(test_back_goes_back_and_the_soft_keys_say_what_they_do);
  RUN_TEST(test_a_tap_on_a_box_changes_it);
  RUN_TEST(test_the_pages_open_afresh_with_the_focus_on_the_first_box);
  RUN_TEST(test_every_page_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
