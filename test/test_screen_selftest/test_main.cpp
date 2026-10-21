// Host tests for the self-test page: the eight checks, what each shows and does, the key and touch tests with
// their idle end, the receiver held and given back, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/SelfTestScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  SelfTestScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) {
    rig.power.setBattery(80);
    rig.run(200);                                                       // the start-up fix is over
    rig.gnss.fixAfter(3, 48.13745, 11.5755, 1.2f);
    rig.gnss.setSatellites(9);
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    screen.onTick();
    draw();
  }
  void letter(uint32_t c) {
    screen.handle(Input::text(c));
    draw();
  }
  void down(int times) {
    for (int i = 0; i < times; ++i) key(Key::Down);
  }
  // Runs test number `index` of the grid (0 = vibration ... 7 = network).
  void run(int index) {
    screen.onEnter();
    down(index);
    key(Key::Select);
  }
  void seconds(uint32_t n) {
    rig.run(n);
    screen.onTick();
    draw();
  }
  // The menu is up, not one of the checks.
  bool inMenu() { return screen.softKeys().action == "Test"; }
  bool says(const std::string& words, FontSize size = FontSize::Small, bool onBlack = false) {
    return shows::text(rig.frame, words, size, kContent, onBlack);
  }
  size_t played(HapticPattern pattern) const {
    size_t n = 0;
    for (HapticPattern p : rig.haptic.played()) n += p == pattern ? 1 : 0;
    return n;
  }
};

// The row of the rule under the header: the first row of the content that is black across its whole width.
int ruleRow(const FrameBuffer& fb) {
  for (int y = kContent.y; y < kContent.bottom(); ++y)
    if (fb.blackPixels({kContent.x, y, kContent.w, 1}) == kContent.w) return y;
  return -1;
}

}  // namespace

// ---- the menu --------------------------------------------------------------------------------------------

void test_the_menu_has_eight_checks_and_the_state_of_light_receiver_and_battery() {
  Page page;
  TEST_ASSERT_TRUE(page.says("Vibration", FontSize::Small, true));      // the focus
  for (const char* name : {"Light", "Display", "Keys", "Touch", "GPS", "Motion", "Network"})
    TEST_ASSERT_TRUE_MESSAGE(page.says(name), name);
  TEST_ASSERT_TRUE(page.says("Light off \xC2\xB7 GPS off"));
  TEST_ASSERT_TRUE(page.says("Battery 80%"));
  TEST_ASSERT_EQUAL_STRING("Test", page.screen.softKeys().action.c_str());
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
}

void test_the_arrows_wrap_around_the_eight_and_a_tap_runs_a_check() {
  Page page;
  page.key(Key::Up);
  TEST_ASSERT_TRUE(page.says("Network", FontSize::Small, true));
  page.key(Key::Down);
  TEST_ASSERT_TRUE(page.says("Vibration", FontSize::Small, true));
  const Rect display = shows::where(page.rig.frame, "Display", FontSize::Small, kContent).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(display.x + 3, display.y + 3)));
  page.draw();
  TEST_ASSERT_EQUAL_STRING("Done", page.screen.softKeys().back.c_str());   // the display check began
}

void test_back_leaves() {
  Page page;
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

// ---- vibration and light --------------------------------------------------------------------------------

void test_vibration_plays_the_five_patterns_in_turn_and_names_the_one_played() {
  Page page;
  const HapticPattern order[] = {HapticPattern::Click, HapticPattern::Ring, HapticPattern::Message, HapticPattern::Sent, HapticPattern::Failed};
  const char* names[] = {"click", "ring", "message", "sent", "failed"};
  for (int round = 0; round < 6; ++round) {
    page.key(Key::Select);
    const int i = round % 5;
    TEST_ASSERT_TRUE_MESSAGE(page.rig.haptic.played().back() == order[i], names[i]);
    TEST_ASSERT_TRUE_MESSAGE(page.says(std::string("Vibration: ") + names[i]), names[i]);
    TEST_ASSERT_TRUE(page.says("Felt it?"));
  }
  TEST_ASSERT_EQUAL(6, page.rig.haptic.played().size());
}

void test_a_result_is_worded_again_when_the_language_changes() {
  Page page;
  page.key(Key::Select);
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.says("Vibration: Klick"));
  TEST_ASSERT_TRUE(page.says("Gesp\xC3\xBCrt?"));
}

void test_light_switches_and_the_state_line_and_the_result_follow() {
  Page page;
  page.run(1);
  TEST_ASSERT_TRUE(page.rig.power.isEnabled(PowerDomain::Frontlight));
  page.draw();
  TEST_ASSERT_TRUE(page.says("Light on"));
  TEST_ASSERT_TRUE(page.says("Brighter?"));
  TEST_ASSERT_TRUE(page.says("Light on \xC2\xB7 GPS off"));
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.rig.power.isEnabled(PowerDomain::Frontlight));
  TEST_ASSERT_TRUE(page.says("Light off"));
}

// ---- the display ------------------------------------------------------------------------------------------

void test_the_display_check_fills_the_content_with_stripes_and_a_chequerboard_and_ends_on_a_tap() {
  Page page;
  page.run(2);
  const int top = page.rig.frame.blackPixels({kContent.x, kContent.y, kContent.w, kContent.h / 2});
  const int bottom = page.rig.frame.blackPixels({kContent.x, kContent.y + kContent.h / 2, kContent.w, kContent.h / 2});
  TEST_ASSERT_TRUE(top > kContent.w * (kContent.h / 2) / 4);            // about half of the upper half is ink
  TEST_ASSERT_TRUE(bottom > kContent.w * (kContent.h / 2) / 4);
  TEST_ASSERT_EQUAL_STRING("Done", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(100, 150)));
  page.draw();
  TEST_ASSERT_TRUE(page.inMenu());
}

void test_the_display_check_ends_on_back_and_on_enter_too() {
  Page back;
  back.run(2);
  TEST_ASSERT_TRUE(back.screen.handle(Input::pressed(Key::Back)));
  back.draw();
  TEST_ASSERT_TRUE(back.inMenu());
  TEST_ASSERT_EQUAL(0, back.rig.nav.calls.size());                      // it ended the check, not the page
  Page enter;
  enter.run(2);
  TEST_ASSERT_TRUE(enter.screen.handle(Input::pressed(Key::Select)));
  enter.draw();
  TEST_ASSERT_TRUE(enter.inMenu());
}

// ---- the key test -------------------------------------------------------------------------------------------

void test_the_key_test_names_every_key_and_wants_the_three_under_the_screen() {
  Page page;
  TEST_ASSERT_FALSE(page.screen.wantsHardwareKeys());
  page.run(3);
  TEST_ASSERT_TRUE(page.screen.wantsHardwareKeys());
  TEST_ASSERT_TRUE(page.says("Key test: press every key"));
  TEST_ASSERT_TRUE(page.says("No key yet", FontSize::Text));
  struct Case {
    Key key;
    const char* name;
  } keys[] = {{Key::Up, "Arrow up"}, {Key::Down, "Arrow down"}, {Key::Select, "Enter \xE2\x86\xB5"},
              {Key::Back, "Backspace \xE2\x8C\xAB"}, {Key::Home, "Home key (left)"},
              {Key::Inbox, "Inbox key (middle)"}, {Key::Compose, "Compose key (right)"}};
  for (const Case& c : keys) {
    TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(c.key)));
    page.draw();
    TEST_ASSERT_TRUE_MESSAGE(page.says(c.name, FontSize::Text), c.name);
  }
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());                      // Backspace was only named
}

void test_typed_keys_are_named_by_their_character_and_the_odd_ones_by_their_code() {
  Page page;
  page.run(3);
  page.letter('q');
  TEST_ASSERT_TRUE(page.says("Key 'q'", FontSize::Text));
  page.letter(0xE4);                                                    // a-diaeresis
  TEST_ASSERT_TRUE(page.says("Key '\xC3\xA4'", FontSize::Text));
  page.letter(' ');
  TEST_ASSERT_TRUE(page.says("Space bar", FontSize::Text));
  page.letter(0x08);
  TEST_ASSERT_TRUE(page.says("Code 0x08", FontSize::Text));
  page.letter(0x7F);
  TEST_ASSERT_TRUE(page.says("Code 0x7F", FontSize::Text));
}

void test_the_key_test_has_no_soft_key_and_ends_after_six_seconds_without_input() {
  Page page;
  page.run(3);
  TEST_ASSERT_TRUE(page.screen.softKeys().back.empty());
  TEST_ASSERT_TRUE(page.says("Ends in 6 s without input"));
  page.seconds(4);
  TEST_ASSERT_TRUE(page.says("Ends in 2 s without input"));
  page.letter('a');                                                     // any key starts the wait afresh
  TEST_ASSERT_TRUE(page.says("Ends in 6 s without input"));
  page.seconds(5);
  TEST_ASSERT_TRUE(page.screen.wantsHardwareKeys());
  page.seconds(1);                                                      // six seconds without input
  TEST_ASSERT_FALSE(page.screen.wantsHardwareKeys());
  TEST_ASSERT_TRUE(page.inMenu());
}

// ---- the touch test --------------------------------------------------------------------------------------------

void test_the_touch_test_shows_the_place_of_a_tap_with_a_cross_and_ends_on_done_or_idle() {
  Page page;
  page.run(4);
  TEST_ASSERT_TRUE(page.says("Touch test: tap the screen"));
  TEST_ASSERT_TRUE(page.says("No tap yet", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("Done", page.screen.softKeys().back.c_str());
  const int before = page.rig.frame.blackPixels({100, 150, 40, 40});
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(120, 170)));
  page.draw();
  TEST_ASSERT_TRUE(page.says("x = 120   y = 170", FontSize::Text));
  TEST_ASSERT_TRUE(page.rig.frame.pixel(120, 171) && page.rig.frame.pixel(112, 170) && page.rig.frame.pixel(128, 170));   // the cross
  TEST_ASSERT_TRUE(page.rig.frame.blackPixels({100, 150, 40, 40}) > before);
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));      // a tap on "Done" arrives as Back
  page.draw();
  TEST_ASSERT_TRUE(page.inMenu());

  Page idle;
  idle.run(4);
  idle.seconds(6);
  TEST_ASSERT_TRUE(idle.inMenu());
}

void test_a_tap_in_the_touch_test_starts_the_wait_afresh() {
  Page page;
  page.run(4);
  page.seconds(5);
  page.screen.handle(Input::touch(50, 100));
  page.seconds(5);
  TEST_ASSERT_TRUE(page.says("x = 50   y = 100", FontSize::Text));      // still there: only five seconds since the tap
  page.seconds(1);
  TEST_ASSERT_TRUE(page.inMenu());
}

// ---- GPS ---------------------------------------------------------------------------------------------------------

void test_the_gps_check_holds_the_receiver_shows_how_it_does_and_gives_it_back() {
  Page page;
  page.run(5);
  TEST_ASSERT_TRUE(page.rig.core.state().positioningActive);
  page.draw();
  TEST_ASSERT_TRUE(page.says("GPS: searching ..."));
  TEST_ASSERT_TRUE(page.says("No fix yet"));
  TEST_ASSERT_TRUE(page.says("Light off \xC2\xB7 GPS on"));
  page.seconds(5);
  TEST_ASSERT_TRUE(page.says("GPS: fix, 9 satellites"));
  TEST_ASSERT_TRUE(page.says("about " + std::to_string(SosPing::accuracyMeters(1.2f)) + " m, HDOP 1.2"));
  page.key(Key::Select);                                                // again: off
  page.rig.poll();
  TEST_ASSERT_FALSE(page.rig.core.state().positioningActive);
  page.draw();
  TEST_ASSERT_TRUE(page.says("GPS off"));
}

void test_leaving_gives_the_receiver_back_by_every_way_and_never_takes_one_it_does_not_hold() {
  DeviceRig rig;
  rig.run(200);
  ScreenStack stack(screen::kHome);
  SelfTestScreen selftest(rig.context(stack));
  struct Blank : Screen {
    bool handle(const Input&) override { return false; }
    void render(Display&) override {}
  } home, lock;
  stack.add(screen::kHome, home);
  stack.add(screen::kSelfTest, selftest);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.begin(0);
  FrameBuffer fb;
  for (int how = 0; how < 3; ++how) {
    stack.open(screen::kSelfTest);
    for (int i = 0; i < 5; ++i) stack.handle(Input::pressed(Key::Down), 1);   // the sixth: GPS
    stack.handle(Input::pressed(Key::Select), 1);
    rig.poll();
    TEST_ASSERT_TRUE_MESSAGE(rig.core.state().positioningActive, std::to_string(how).c_str());
    if (how == 0) stack.back();
    if (how == 1) stack.home();
    if (how == 2) stack.lock();
    rig.poll();
    TEST_ASSERT_FALSE_MESSAGE(rig.core.state().positioningActive, std::to_string(how).c_str());
    stack.unlock();
  }
  // Another reason holds the receiver (the position page): leaving the self test without a GPS check keeps it.
  rig.core.setPositionPageOpen(true);
  stack.open(screen::kSelfTest);
  stack.back();
  rig.poll();
  TEST_ASSERT_TRUE(rig.core.state().positioningActive);
}

// ---- the other pages -----------------------------------------------------------------------------------------------

void test_the_motion_and_network_checks_open_their_pages() {
  Page motion;
  motion.run(6);
  TEST_ASSERT_TRUE(motion.rig.nav.opened(screen::kMotion));
  Page net;
  net.run(7);
  TEST_ASSERT_TRUE(net.rig.nav.opened(screen::kNet));
}

void test_the_grid_does_not_jump_when_a_check_reports() {
  for (const char* language : {"en", "de", "fr", "qps-ploc"}) {
    Page page;
    page.rig.language(language);
    page.draw();
    const Rect before = shows::where(page.rig.frame, page.rig.translator.get("selftest.touch"), FontSize::Small, kContent).value();
    page.key(Key::Select);                                              // vibration reports
    const Rect afterVibration = shows::where(page.rig.frame, page.rig.translator.get("selftest.touch"), FontSize::Small, kContent).value();
    page.key(Key::Down);
    page.key(Key::Select);                                              // the light reports
    const Rect afterLight = shows::where(page.rig.frame, page.rig.translator.get("selftest.touch"), FontSize::Small, kContent).value();
    TEST_ASSERT_EQUAL_MESSAGE(before.y, afterVibration.y, language);
    TEST_ASSERT_EQUAL_MESSAGE(before.y, afterLight.y, language);
  }
}

// A catalog on the card may word "on" at any length; the header then wraps in the state that says it, and the
// grid must still stay where it was.
void test_a_longer_word_for_on_does_not_move_the_grid() {
  Page page;
  const std::string text =
      "[meta]\ncode = \"de\"\nname = \"German\"\nnative = \"Deutsch\"\nplural = \"one-other\"\n"
      "[strings]\nselftest.on = \"eingeschaltet und bereit zum Test heute\"\n";
  TEST_ASSERT_TRUE(page.rig.translator.addUser(text.data(), text.size(), nullptr));
  page.rig.language("de");
  page.draw();
  const Rect before = shows::where(page.rig.frame, "Touch", FontSize::Small, kContent).value();
  const int ruleBefore = ruleRow(page.rig.frame);
  page.down(1);
  page.key(Key::Select);                                                // the light goes on: the long word is in the header
  TEST_ASSERT_TRUE(page.rig.power.isEnabled(PowerDomain::Frontlight));
  page.draw();
  const Rect after = shows::where(page.rig.frame, "Touch", FontSize::Small, kContent).value();
  TEST_ASSERT_EQUAL(before.y, after.y);
  TEST_ASSERT_EQUAL(ruleBefore, ruleRow(page.rig.frame));                // the rule and the result below it stay put too
  TEST_ASSERT_TRUE(page.says("Akku 80%"));                              // and the header's last line is whole, not under the rule
}

// ---- layout -----------------------------------------------------------------------------------------------------------

void test_every_state_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 9; ++state) {
      Page page;
      page.rig.language(language);
      if (state == 1) page.run(0);                                      // vibration result
      if (state == 2) page.run(1);                                      // light on
      if (state == 3) page.run(2);                                      // display pattern
      if (state == 4) { page.run(3); page.letter(0xE4); }               // keys
      if (state == 5) { page.run(3); page.screen.handle(Input::pressed(Key::Compose)); }
      if (state == 6) { page.run(4); page.screen.handle(Input::touch(200, 250)); }
      if (state == 7) page.run(5);                                      // gps searching
      if (state == 8) { page.run(5); page.seconds(6); }                 // gps fix
      page.draw();
      FrameBuffer fb;
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/ui/screen-selftest.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " state " + std::to_string(state) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page menu;
    menu.rig.language(language);
    menu.key(Key::Select);
    golden::expect(menu.rig.frame, std::string("selftest_menu_") + language);
    Page keys;
    keys.rig.language(language);
    keys.run(3);
    keys.letter('q');
    golden::expect(keys.rig.frame, std::string("selftest_keys_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_menu_has_eight_checks_and_the_state_of_light_receiver_and_battery);
  RUN_TEST(test_the_arrows_wrap_around_the_eight_and_a_tap_runs_a_check);
  RUN_TEST(test_back_leaves);
  RUN_TEST(test_vibration_plays_the_five_patterns_in_turn_and_names_the_one_played);
  RUN_TEST(test_a_result_is_worded_again_when_the_language_changes);
  RUN_TEST(test_light_switches_and_the_state_line_and_the_result_follow);
  RUN_TEST(test_the_display_check_fills_the_content_with_stripes_and_a_chequerboard_and_ends_on_a_tap);
  RUN_TEST(test_the_display_check_ends_on_back_and_on_enter_too);
  RUN_TEST(test_the_key_test_names_every_key_and_wants_the_three_under_the_screen);
  RUN_TEST(test_typed_keys_are_named_by_their_character_and_the_odd_ones_by_their_code);
  RUN_TEST(test_the_key_test_has_no_soft_key_and_ends_after_six_seconds_without_input);
  RUN_TEST(test_the_touch_test_shows_the_place_of_a_tap_with_a_cross_and_ends_on_done_or_idle);
  RUN_TEST(test_a_tap_in_the_touch_test_starts_the_wait_afresh);
  RUN_TEST(test_the_gps_check_holds_the_receiver_shows_how_it_does_and_gives_it_back);
  RUN_TEST(test_leaving_gives_the_receiver_back_by_every_way_and_never_takes_one_it_does_not_hold);
  RUN_TEST(test_the_motion_and_network_checks_open_their_pages);
  RUN_TEST(test_the_grid_does_not_jump_when_a_check_reports);
  RUN_TEST(test_a_longer_word_for_on_does_not_move_the_grid);
  RUN_TEST(test_every_state_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
