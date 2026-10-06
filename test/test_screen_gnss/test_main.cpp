// Host tests for the position page: it holds the receiver while it is open, the four values of a current
// fix, what it says without one, the last known position with its age, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Formats.h>
#include <ui/Text.h>
#include <ui/screens/GnssScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  GnssScreen screen;
  // `neverAFix`: not even the start-up fix finds a position, so nothing is known.
  explicit Page(bool neverAFix = false) : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) {
    if (neverAFix) rig.gnss.never();
    rig.run(200);                                                       // the start-up fix is over, the receiver is off
    rig.gnss.fixAfter(3, 48.13745, 11.5755, 1.2f);
    rig.gnss.setSatellites(9);
    if (neverAFix) rig.gnss.never();
  }
  void enter() {
    screen.onEnter();
    rig.poll();
    draw();
  }
  void leave() {
    screen.onLeave();
    rig.poll();
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
  bool says(const std::string& words, FontSize size) { return shows::text(rig.frame, words, size, kContent); }
  // A paragraph of the text block: every line of it.
  bool saysWrapped(const std::string& words) {
    const auto lines = text::wrap(font(FontSize::Small), words, kContent.w - 8);
    for (std::string_view line : lines)
      if (!lines.empty() && !line.empty() && !says(std::string(line), FontSize::Small)) return false;
    return !lines.empty();
  }
};

}  // namespace

// ---- the receiver ---------------------------------------------------------------------------------------

void test_the_page_holds_the_receiver_while_it_is_open_and_lets_go_when_it_is_left() {
  Page page;
  TEST_ASSERT_FALSE(page.rig.core.state().positioningActive);
  page.enter();
  TEST_ASSERT_TRUE(page.rig.core.state().positioningActive);
  page.leave();
  TEST_ASSERT_FALSE(page.rig.core.state().positioningActive);
}

void test_leaving_through_a_real_stack_lets_go_by_every_way() {
  DeviceRig rig;
  rig.run(200);
  ScreenStack stack(screen::kHome);
  const ScreenContext context = rig.context(stack);
  GnssScreen gnss(context);
  struct Blank : Screen {
    bool handle(const Input&) override { return false; }
    void render(Display&) override {}
  } home, lock;
  stack.add(screen::kHome, home);
  stack.add(screen::kGnss, gnss);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.begin(0);
  for (int how = 0; how < 3; ++how) {
    stack.open(screen::kGnss);
    rig.poll();
    TEST_ASSERT_TRUE_MESSAGE(rig.core.state().positioningActive, std::to_string(how).c_str());
    if (how == 0) stack.back();
    if (how == 1) stack.home();
    if (how == 2) stack.lock();
    rig.poll();
    TEST_ASSERT_FALSE_MESSAGE(rig.core.state().positioningActive, std::to_string(how).c_str());
    stack.unlock();
  }
}

// ---- without a position ---------------------------------------------------------------------------------

void test_while_searching_it_says_so_and_that_indoors_there_is_often_no_reception() {
  Page page;
  page.rig.gnss.never();
  page.enter();
  TEST_ASSERT_TRUE(page.says("GPS", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysWrapped("Searching ... in the open this takes up to two minutes."));
  TEST_ASSERT_TRUE(page.saysWrapped("Indoors there is often no reception. Go to a window or outside."));
  TEST_ASSERT_FALSE(page.says("Latitude", FontSize::Small));
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
}

void test_with_the_receiver_off_and_no_position_ever_it_says_positioning_is_off() {
  Page page(true);
  page.draw();                                                          // never opened: nobody holds the receiver
  TEST_ASSERT_TRUE(page.saysWrapped("Positioning is off."));
  TEST_ASSERT_FALSE(page.saysWrapped("Last known position:"));
}

// ---- with a position --------------------------------------------------------------------------------------

void test_a_current_fix_gives_four_values_under_small_labels() {
  Page page;
  page.enter();
  page.run(5);
  TEST_ASSERT_TRUE(page.rig.core.state().positioningActive);
  TEST_ASSERT_TRUE(page.says("Latitude", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("48.13745", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Longitude", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("11.57550", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Accuracy", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("about " + std::to_string(SosPing::accuracyMeters(1.2f)) + " m", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Satellites", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("9", FontSize::Text));
  TEST_ASSERT_FALSE(page.saysWrapped("Searching ... in the open this takes up to two minutes."));
}

void test_the_values_are_worded_in_the_language_of_the_moment() {
  Page page;
  page.enter();
  page.run(5);
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.says("Breite", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("L\xC3\xA4nge", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Genauigkeit", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Satelliten", FontSize::Small));
}

void test_a_southern_and_western_position_is_written_with_its_signs() {
  Page page;
  page.rig.gnss.fixAfter(3, -48.13745, -11.5755, 1.0f);
  page.enter();
  page.run(5);
  TEST_ASSERT_TRUE(page.says("-48.13745", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("-11.57550", FontSize::Text));
}

void test_the_arrows_do_nothing_while_the_values_fit_and_back_goes_back() {
  Page page;
  page.enter();
  page.run(5);
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Down)));
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

// ---- the last known position ------------------------------------------------------------------------------

void test_after_the_page_is_left_the_last_position_is_shown_with_its_age_and_never_as_the_present_one() {
  Page page;
  page.enter();
  page.run(5);
  page.leave();
  page.run(180);                                                        // three minutes
  TEST_ASSERT_FALSE(page.says("Latitude", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysWrapped("Positioning is off."));
  TEST_ASSERT_TRUE(page.saysWrapped("Last known position:"));
  TEST_ASSERT_TRUE(page.says("3 min ago", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("48.13745", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("11.57550", FontSize::Small));
}

void test_a_fix_that_stops_coming_turns_into_the_last_known_one_and_the_search_goes_on() {
  Page page;
  page.enter();
  page.run(5);
  TEST_ASSERT_TRUE(page.says("Latitude", FontSize::Small));
  page.rig.gnss.never();                                                // the receiver loses the sky
  page.run(30);
  TEST_ASSERT_FALSE(page.says("Latitude", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysWrapped("Searching ... in the open this takes up to two minutes."));
  TEST_ASSERT_TRUE(page.saysWrapped("Last known position:"));
  TEST_ASSERT_TRUE(page.says("just now", FontSize::Small));            // under a minute and a half
}

void test_an_age_in_german() {
  Page page;
  page.enter();
  page.run(5);
  page.leave();
  page.run(300);
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.says("vor 5 Min.", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysWrapped("Zuletzt bekannte Position:"));
}

// ---- scrolling ------------------------------------------------------------------------------------------------

void test_a_text_longer_than_the_page_scrolls_and_the_soft_keys_say_so() {
  Page page;
  page.enter();
  page.run(5);
  page.leave();
  page.rig.gnss.never();
  page.enter();
  page.run(30);                                                         // searching again, with the last known position
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);                    // English fits
  page.rig.language("qps-ploc");                                        // 40 % longer: it does not
  page.draw();
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  const std::string top = page.rig.frame.toAscii(kContent);
  page.key(Key::Down);
  TEST_ASSERT_TRUE(top != page.rig.frame.toAscii(kContent));
  page.key(Key::Up);
  TEST_ASSERT_TRUE(top == page.rig.frame.toAscii(kContent));
}

// ---- layout ------------------------------------------------------------------------------------------------------

void test_every_state_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 4; ++state) {
      Page page;
      page.rig.language(language);
      if (state == 0) page.draw();                                      // off, nothing known
      if (state == 1) { page.rig.gnss.never(); page.enter(); page.run(5); }   // searching
      if (state == 2) { page.enter(); page.run(5); }                    // current
      if (state == 3) { page.enter(); page.run(5); page.leave(); page.rig.gnss.never(); page.enter(); page.run(30); }   // searching, last known
      FrameBuffer fb;
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/screen-gnss.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " state " + std::to_string(state) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page live;
    live.rig.language(language);
    live.enter();
    live.run(5);
    golden::expect(live.rig.frame, std::string("gnss_fix_") + language);
    Page known;
    known.rig.language(language);
    known.enter();
    known.run(5);
    known.leave();
    known.run(180);
    golden::expect(known.rig.frame, std::string("gnss_last_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_page_holds_the_receiver_while_it_is_open_and_lets_go_when_it_is_left);
  RUN_TEST(test_leaving_through_a_real_stack_lets_go_by_every_way);
  RUN_TEST(test_while_searching_it_says_so_and_that_indoors_there_is_often_no_reception);
  RUN_TEST(test_with_the_receiver_off_and_no_position_ever_it_says_positioning_is_off);
  RUN_TEST(test_a_current_fix_gives_four_values_under_small_labels);
  RUN_TEST(test_the_values_are_worded_in_the_language_of_the_moment);
  RUN_TEST(test_a_southern_and_western_position_is_written_with_its_signs);
  RUN_TEST(test_the_arrows_do_nothing_while_the_values_fit_and_back_goes_back);
  RUN_TEST(test_after_the_page_is_left_the_last_position_is_shown_with_its_age_and_never_as_the_present_one);
  RUN_TEST(test_a_fix_that_stops_coming_turns_into_the_last_known_one_and_the_search_goes_on);
  RUN_TEST(test_an_age_in_german);
  RUN_TEST(test_a_text_longer_than_the_page_scrolls_and_the_soft_keys_say_so);
  RUN_TEST(test_every_state_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
