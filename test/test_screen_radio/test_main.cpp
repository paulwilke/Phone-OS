// Host tests for the radio page: the head line, the controls each state offers, the contacts with their way
// and age, writing to one, the save mode and its probe, scrolling, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/RadioScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

Profile allowed(bool freeText = false) { return ProfileBuilder().childName("Alex").radio(true).freeText(freeText).build(); }

struct Page {
  DeviceRig rig;
  RadioScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    rig.poll();
    screen.onEnter();
    draw();
  }
  // The radio runs and knows `Node One`, heard three minutes ago, directly.
  void radioRuns() {
    rig.radio.setUp(true);
    rig.radio.addPeer("Node One", true, rig.clock.epochUtc() - 180);
    rig.core.setRadio(&rig.radio);
    rig.poll();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  void down(int times) {
    for (int i = 0; i < times; ++i) key(Key::Down);
  }
  bool says(const std::string& words, FontSize size, bool onBlack = false) { return shows::text(rig.frame, words, size, kContent, onBlack); }
  bool saysWrapped(const std::string& words) {
    const auto lines = text::wrap(font(FontSize::Small), words, kContent.w - 12);
    for (std::string_view line : lines)
      if (!says(std::string(line), FontSize::Small)) return false;
    return !lines.empty();
  }
};

}  // namespace

// ---- the states -----------------------------------------------------------------------------------------

void test_where_the_profile_does_not_allow_the_radio_nothing_can_be_pressed() {
  Page page;
  TEST_ASSERT_TRUE(page.says("Radio: not allowed", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysWrapped("The radio is not allowed on this device."));
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_FALSE(page.rig.core.takeRadioStartRequest());
}

void test_an_allowed_radio_that_is_off_offers_to_switch_it_on() {
  Page page(allowed());
  TEST_ASSERT_TRUE(page.says("Radio: off", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Switch radio on", FontSize::Text, true));
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.takeRadioStartRequest());              // the application starts it and attaches it
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
}

void test_a_radio_that_runs_with_nobody_in_range_says_so_and_that_it_reports_by_itself() {
  Page page(allowed());
  page.rig.radio.setUp(true);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Radio: nobody in range", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysWrapped("Nobody heard yet. The radio reports by itself as soon as one is in range."));
  TEST_ASSERT_TRUE(page.says("Save mode: radio only", FontSize::Text, true));
  TEST_ASSERT_TRUE(page.says("Radio off (restart)", FontSize::Text));
}

void test_a_ready_radio_gives_the_strength_and_lists_its_contacts_with_way_and_age() {
  Page page(allowed());
  page.radioRuns();
  TEST_ASSERT_TRUE(page.says("Radio: ready, -95 dBm", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Node One", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("direct \xC2\xB7 3 min ago", FontSize::Small));
  TEST_ASSERT_FALSE(page.saysWrapped("Nobody heard yet. The radio reports by itself as soon as one is in range."));
}

void test_an_unknown_strength_is_left_out_and_a_contact_that_goes_by_detours_without_a_time_is_told_so() {
  Page page(allowed());
  page.rig.radio.setUp(true);
  page.rig.radio.addPeer("Node Two", false, 0);
  page.rig.radio.setRssi(0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Radio: ready", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("via detours \xC2\xB7 heard", FontSize::Small));
}

void test_the_words_are_those_of_the_language_of_the_moment() {
  Page page(allowed());
  page.radioRuns();
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.says("Funk: bereit, -95 dBm", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("direkt \xC2\xB7 vor 3 Min.", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Sparmodus: nur Funk", FontSize::Text, true));
}

// ---- the controls ------------------------------------------------------------------------------------------------

void test_save_mode_switches_on_and_the_page_offers_the_probe_and_the_way_out() {
  Page page(allowed());
  page.radioRuns();
  page.key(Key::Select);                                                // "Save mode: radio only"
  TEST_ASSERT_TRUE(page.rig.core.state().saveMode);
  TEST_ASSERT_TRUE(page.says("Save mode: radio only", FontSize::Small));  // the head line
  TEST_ASSERT_TRUE(page.says("Check cellular", FontSize::Text, true));
  TEST_ASSERT_TRUE(page.says("End save mode", FontSize::Text));
  page.down(1);
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.rig.core.state().saveMode);
  TEST_ASSERT_TRUE(page.says("Radio off (restart)", FontSize::Text, true));   // the focus stayed on the second button
}

void test_the_probe_counts_down_on_its_button() {
  Page page(allowed());
  page.radioRuns();
  page.key(Key::Select);                                                // into save mode
  page.key(Key::Select);                                                // "Check cellular"
  TEST_ASSERT_TRUE(page.rig.core.state().probeSecondsLeft > 0);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Network probe 3:00", FontSize::Text, true));
  page.rig.run(1);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Network probe 2:59", FontSize::Text, true));
  page.rig.run(55);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Network probe 2:04", FontSize::Text, true));   // two digits of seconds
}

void test_radio_off_asks_for_a_restart() {
  Page page(allowed());
  page.radioRuns();
  page.down(1);
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.restartRequested());
}

// ---- the contacts -------------------------------------------------------------------------------------------------

void test_a_contact_is_written_to_only_where_the_child_may_choose_any_recipient() {
  Page without(allowed(false));
  without.radioRuns();
  without.down(2);
  TEST_ASSERT_TRUE(without.says("Node One", FontSize::Text, true));
  TEST_ASSERT_TRUE(without.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(without.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_EQUAL(0, without.rig.nav.calls.size());
  TEST_ASSERT_EQUAL(0, without.rig.session.recipient.size());

  Page with(allowed(true));
  with.radioRuns();
  with.down(2);
  TEST_ASSERT_EQUAL_STRING("Write", with.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(with.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_TRUE(with.rig.nav.opened(screen::kCompose));
  TEST_ASSERT_EQUAL_STRING("Node One", with.rig.session.recipient.c_str());
  TEST_ASSERT_TRUE(with.rig.session.viaRadio);
}

void test_a_long_name_is_shortened_inside_its_row() {
  Page page(allowed());
  page.rig.radio.setUp(true);
  page.rig.radio.addPeer("A radio contact with a very long name indeed", true, 0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.poll();
  page.draw();
  FrameBuffer fb;
  page.screen.render(fb);
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
  TEST_ASSERT_TRUE(page.says(text::ellipsize(font(FontSize::Text), "A radio contact with a very long name indeed", kContent.w - 6 - 4 - 8), FontSize::Text));
}

void test_many_contacts_scroll_with_the_focus_and_the_last_can_be_reached() {
  Page page(allowed(true));
  page.rig.radio.setUp(true);
  for (int i = 0; i < 12; ++i) page.rig.radio.addPeer("Node " + std::to_string(i), true, 0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.poll();
  page.draw();
  page.down(40);
  TEST_ASSERT_TRUE(page.says("Node 11", FontSize::Text, true));
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_EQUAL_STRING("Node 11", page.rig.session.recipient.c_str());
  page.key(Key::Up);
  page.key(Key::Up);
  TEST_ASSERT_TRUE(page.says("Node 9", FontSize::Text, true));
}

void test_selection_stops_at_the_first_row() {
  Page page(allowed());
  page.radioRuns();
  page.key(Key::Up);
  TEST_ASSERT_TRUE(page.says("Save mode: radio only", FontSize::Text, true));
}

void test_a_tap_selects_and_acts() {
  Page page(allowed(true));
  page.radioRuns();
  // The contact is the third row; find it by where its name is drawn.
  const Rect name = shows::where(page.rig.frame, "Node One", FontSize::Text, kContent).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(name.x + 5, name.y + 5)));
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
  const Rect save = shows::where(page.rig.frame, "Save mode: radio only", FontSize::Text, kContent, true).value();   // in focus: white on black
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(save.x + 5, save.y + 5)));
  TEST_ASSERT_TRUE(page.rig.core.state().saveMode);
}

void test_back_goes_back_and_the_arrows_are_offered_with_more_than_one_row() {
  Page page(allowed());
  page.radioRuns();
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  Page off(allowed());
  TEST_ASSERT_FALSE(off.screen.softKeys().scroll);                      // one button
}

// ---- layout ---------------------------------------------------------------------------------------------------------

void test_every_state_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 6; ++state) {
      Page page(state == 0 ? ProfileBuilder().childName("Alex").build() : allowed(true));
      page.rig.language(language);
      if (state >= 2) {
        page.rig.radio.setUp(true);
        if (state >= 3) {
          page.rig.radio.addPeer("Node One", true, page.rig.clock.epochUtc() - 7200);
          page.rig.radio.addPeer("Node Two", false, 0);
        }
        page.rig.core.setRadio(&page.rig.radio);
        page.rig.poll();
      }
      if (state >= 4) {
        page.draw();
        page.key(Key::Select);                                          // save mode
      }
      if (state == 5) {
        page.key(Key::Select);                                          // the probe
        page.rig.run(1);
      }
      page.draw();
      FrameBuffer fb;
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/screen-radio.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " state " + std::to_string(state) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page(allowed());
    page.rig.language(language);
    page.radioRuns();
    golden::expect(page.rig.frame, std::string("radio_ready_") + language);
    Page off(allowed());
    off.rig.language(language);
    off.draw();
    golden::expect(off.rig.frame, std::string("radio_off_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_where_the_profile_does_not_allow_the_radio_nothing_can_be_pressed);
  RUN_TEST(test_an_allowed_radio_that_is_off_offers_to_switch_it_on);
  RUN_TEST(test_a_radio_that_runs_with_nobody_in_range_says_so_and_that_it_reports_by_itself);
  RUN_TEST(test_a_ready_radio_gives_the_strength_and_lists_its_contacts_with_way_and_age);
  RUN_TEST(test_an_unknown_strength_is_left_out_and_a_contact_that_goes_by_detours_without_a_time_is_told_so);
  RUN_TEST(test_the_words_are_those_of_the_language_of_the_moment);
  RUN_TEST(test_save_mode_switches_on_and_the_page_offers_the_probe_and_the_way_out);
  RUN_TEST(test_the_probe_counts_down_on_its_button);
  RUN_TEST(test_radio_off_asks_for_a_restart);
  RUN_TEST(test_a_contact_is_written_to_only_where_the_child_may_choose_any_recipient);
  RUN_TEST(test_a_long_name_is_shortened_inside_its_row);
  RUN_TEST(test_many_contacts_scroll_with_the_focus_and_the_last_can_be_reached);
  RUN_TEST(test_selection_stops_at_the_first_row);
  RUN_TEST(test_a_tap_selects_and_acts);
  RUN_TEST(test_back_goes_back_and_the_arrows_are_offered_with_more_than_one_row);
  RUN_TEST(test_every_state_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
