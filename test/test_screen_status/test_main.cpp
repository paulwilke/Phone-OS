// Host tests for the status page: the lines, the step through the visibility of remote queries, switching
// off with two presses and not at all on the cable, scrolling when a language makes it long, and the layout.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/StatusScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  StatusScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context(), {"1.2.3", "Example board", {}}) { start(); }
  explicit Page(Profile profile, DeviceInfo info = {"1.2.3", "Example board", {}})
      : rig(std::move(profile)), screen(rig.context(), std::move(info)) { start(); }
  void start() {
    rig.power.setBattery(80);
    rig.power.setMinutesRemaining(150);
    rig.poll();
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  void letter(char c) {
    screen.handle(Input::text(static_cast<uint32_t>(c)));
    draw();
  }
  // A line of the page, as drawn: wrapped inside the text block's width, every piece of it.
  bool says(const std::string& words) {
    const auto lines = text::wrap(font(FontSize::Small), words, kContent.w - 8);
    for (std::string_view line : lines)
      if (!shows::text(rig.frame, std::string(line), FontSize::Small, kContent)) return false;
    return !lines.empty();
  }
};

// A hardware description so long that the page needs scrolling in any language.
const char* kLongHardware = "Example board with a processor, a modem, a receiver and a radio on one card, "
                            "a display that keeps its picture and a battery that is changed by the parents only";

}  // namespace

// ---- the lines ---------------------------------------------------------------------------------------------

void test_the_page_gives_battery_time_left_child_contacts_queries_and_firmware() {
  Page page;
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Status", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(page.says("Battery: 80%"));
  TEST_ASSERT_TRUE(page.says("Remaining: 2 h 30 min"));
  TEST_ASSERT_TRUE(page.says("Child: Alex"));
  TEST_ASSERT_TRUE(page.says("Contacts: 3"));
  TEST_ASSERT_TRUE(page.says("Remote queries: in the history"));
  TEST_ASSERT_TRUE(page.says("Firmware: 1.2.3"));
  TEST_ASSERT_TRUE(page.says("Example board"));
  TEST_ASSERT_TRUE(page.says("Switch off: press x twice"));
  TEST_ASSERT_TRUE(page.says("Restart: press r twice"));
}

void test_unknown_values_are_told_as_unknown_and_empty_ones_are_left_out() {
  Page page(ProfileBuilder().childName("").build(), {"1.2.3", "", {}});
  page.rig.power.setBattery(-1);
  page.rig.power.setMinutesRemaining(-1);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Battery: unknown"));
  TEST_ASSERT_TRUE(page.says("Remaining: unknown"));
  TEST_ASSERT_FALSE(shows::text(page.rig.frame, "Child:", FontSize::Small, kContent) || page.says("Child: "));
  TEST_ASSERT_FALSE(page.says("Example board"));
}

void test_the_cable_is_told_with_the_battery_and_switching_off_says_unplug_first() {
  Page charging;
  charging.rig.power.plugIn(ChargeState::Charging);
  charging.rig.poll();
  charging.draw();
  TEST_ASSERT_TRUE(charging.says("Battery: 80%, charging"));
  TEST_ASSERT_TRUE(charging.says("Switch off: unplug USB first"));
  Page full;
  full.rig.power.plugIn(ChargeState::Full);
  full.rig.poll();
  full.draw();
  TEST_ASSERT_TRUE(full.says("Battery: 80%, full on cable"));
}

void test_the_number_of_contacts_follows_the_book() {
  Page page(ProfileBuilder().noContacts().contact(kParentOne, "Parent One", true).build());
  TEST_ASSERT_TRUE(page.says("Contacts: 1"));
}

void test_the_problems_of_the_configuration_are_counted_and_the_first_one_is_told() {
  DeviceInfo info{"1.2.3", "Example board", {{ConfigProblem::Kind::Setting, "settings.toml", "colour"},
                                           {ConfigProblem::Kind::File, "contacts.toml", ""}}};
  Page page(ProfileBuilder().childName("Alex").build(), info);
  TEST_ASSERT_TRUE(page.says("Configuration: 2 problems"));
  TEST_ASSERT_TRUE(page.says("settings.toml: check colour"));
  TEST_ASSERT_FALSE(page.says("could not be read"));                  // only the first
  Page one(ProfileBuilder().childName("Alex").build(), {"1.2.3", "", {{ConfigProblem::Kind::File, "contacts.toml", ""}}});
  TEST_ASSERT_TRUE(one.says("Configuration: 1 problem"));
  TEST_ASSERT_TRUE(one.says("contacts.toml could not be read"));
  Page entry(ProfileBuilder().childName("Alex").build(), {"1.2.3", "", {{ConfigProblem::Kind::Entry, "", ""}}});
  TEST_ASSERT_TRUE(entry.says("An entry in the lists was not used"));
}

void test_without_problems_there_is_no_line_about_the_configuration() {
  Page page;
  TEST_ASSERT_FALSE(page.says("Configuration: 0 problems"));
  TEST_ASSERT_FALSE(shows::text(page.rig.frame, "Configuration:", FontSize::Small, kContent));
}

// ---- remote queries -----------------------------------------------------------------------------------------

void test_enter_steps_through_the_visibility_of_remote_queries() {
  Page page;
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.commandNotice() == CommandNotice::Notify);
  TEST_ASSERT_TRUE(page.says("Remote queries: with a notice"));
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.commandNotice() == CommandNotice::Silent);
  TEST_ASSERT_TRUE(page.says("Remote queries: silent"));
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.commandNotice() == CommandNotice::History);
  TEST_ASSERT_TRUE(page.says("Remote queries: in the history"));
  TEST_ASSERT_EQUAL_STRING("Queries", page.screen.softKeys().action.c_str());
}

// ---- switching off -------------------------------------------------------------------------------------------

void test_switching_off_takes_two_presses_of_x() {
  Page page;
  page.letter('x');
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
  TEST_ASSERT_TRUE(page.says("Switch off? Press x again"));
  page.letter('x');
  TEST_ASSERT_TRUE(page.rig.core.powerOffRequested());
}

void test_any_other_key_takes_the_first_press_back() {
  Page page;
  page.letter('x');
  page.key(Key::Down);
  TEST_ASSERT_TRUE(page.says("Switch off: press x twice"));
  page.letter('x');
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
  page.letter('y');                                                   // a letter that is not x
  page.letter('x');
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_a_capital_x_counts_too() {
  Page page;
  page.letter('X');
  page.letter('x');
  TEST_ASSERT_TRUE(page.rig.core.powerOffRequested());
}

void test_a_cable_that_is_not_charging_is_still_a_cable() {
  Page page;
  page.rig.power.plugInWithoutCharging();
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Switch off: unplug USB first"));
  TEST_ASSERT_FALSE(page.says("charging"));
}

void test_on_the_cable_the_application_fails_and_the_notice_says_why() {
  Page page;
  page.rig.power.plugIn(ChargeState::Charging);
  page.rig.poll();
  page.letter('x');
  page.letter('x');
  TEST_ASSERT_TRUE(page.rig.core.powerOffRequested());               // the page asks; the application tries
  page.rig.core.notePowerOffFailed();                                // and the cable does not let it
  page.draw();
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
  TEST_ASSERT_EQUAL_STRING("Switch off only without cable", page.screen.noticeLine().c_str());
}

// ---- restarting ----------------------------------------------------------------------------------------------

void test_restarting_takes_two_presses_of_r_and_logs_who_asked() {
  Page page;
  page.letter('r');
  TEST_ASSERT_FALSE(page.rig.core.restartRequested());
  TEST_ASSERT_TRUE(page.says("Restart? Press r again"));
  TEST_ASSERT_FALSE(page.rig.log.contains("Restart requested on the device"));
  page.letter('r');
  TEST_ASSERT_TRUE(page.rig.core.restartRequested());                 // the host fake reads the core's request
  TEST_ASSERT_TRUE(page.rig.log.contains("Restart requested on the device"));
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_a_capital_r_counts_too() {
  Page page;
  page.letter('R');
  page.letter('r');
  TEST_ASSERT_TRUE(page.rig.core.restartRequested());
}

void test_any_other_key_takes_the_first_press_of_r_back() {
  Page page;
  page.letter('r');
  page.key(Key::Down);
  TEST_ASSERT_TRUE(page.says("Restart: press r twice"));
  page.letter('r');
  TEST_ASSERT_FALSE(page.rig.core.restartRequested());
  page.letter('y');
  page.letter('r');
  TEST_ASSERT_FALSE(page.rig.core.restartRequested());
  page.letter('x');                                                   // the other question takes it back as well
  page.letter('r');
  TEST_ASSERT_FALSE(page.rig.core.restartRequested());
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_r_between_the_presses_of_x_takes_the_switch_off_back() {
  Page page;
  page.letter('x');
  page.letter('r');
  page.letter('x');
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
  TEST_ASSERT_FALSE(page.rig.core.restartRequested());
}

void test_leaving_the_page_takes_the_first_press_of_r_back() {
  Page page;
  page.letter('r');
  page.screen.onEnter();                                              // the page opened afresh
  page.draw();
  TEST_ASSERT_TRUE(page.says("Restart: press r twice"));
  page.letter('r');
  TEST_ASSERT_FALSE(page.rig.core.restartRequested());
}

void test_restarting_works_on_the_cable() {
  Page page;
  page.rig.power.plugIn(ChargeState::Charging);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Switch off: unplug USB first"));
  TEST_ASSERT_TRUE(page.says("Restart: press r twice"));
  page.letter('r');
  TEST_ASSERT_TRUE(page.says("Restart? Press r again"));
  page.letter('r');
  TEST_ASSERT_TRUE(page.rig.core.restartRequested());
}

// ---- keys, scrolling ------------------------------------------------------------------------------------------

void test_back_goes_back_and_scrolling_is_offered_only_when_there_is_something_to_scroll() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  Page long_(ProfileBuilder().childName("Alex").build(), {"1.2.3", kLongHardware, {}});
  TEST_ASSERT_TRUE(long_.screen.softKeys().scroll);
  const std::string top = long_.rig.frame.toAscii();
  long_.key(Key::Down);
  TEST_ASSERT_TRUE(top != long_.rig.frame.toAscii());
  long_.screen.onEnter();
  long_.draw();
  TEST_ASSERT_TRUE(top == long_.rig.frame.toAscii());                 // opened afresh: at the top
}

void test_the_position_in_a_long_text_is_kept_while_nothing_changes() {
  Page page(ProfileBuilder().childName("Alex").build(), {"1.2.3", kLongHardware, {}});
  page.key(Key::Down);
  const std::string scrolled = page.rig.frame.toAscii(kContent);
  page.rig.run(5);                                                    // a few seconds: the same words
  page.draw();
  TEST_ASSERT_TRUE(scrolled == page.rig.frame.toAscii(kContent));
}

// ---- layout ----------------------------------------------------------------------------------------------------------

void test_every_state_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 6; ++state) {
      const DeviceInfo problems{"1.2.3", "Example board", {{ConfigProblem::Kind::Setting, "settings.toml", "interface.language"},
                                                         {ConfigProblem::Kind::File, "contacts.toml", ""}}};
      Page page = state == 4 ? Page(ProfileBuilder().childName("Alex").build(), problems) : Page();
      page.rig.language(language);
      if (state == 1) { page.rig.power.plugIn(ChargeState::Full); page.rig.poll(); }
      if (state == 2) { page.letter('x'); }
      if (state == 5) { page.letter('r'); }
      if (state == 3) { page.rig.power.setBattery(-1); page.rig.power.setMinutesRemaining(-1); page.rig.poll(); }
      FrameBuffer fb;
      page.draw();
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/screen-status.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " state " + std::to_string(state) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page;
    page.rig.language(language);
    page.draw();
    golden::expect(page.rig.frame, std::string("status_") + language);
  }
}

void test_the_pictures_with_the_restart_asked_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page;
    page.rig.language(language);
    page.letter('r');
    golden::expect(page.rig.frame, std::string("status_restart_") + language);
  }
}

void test_the_pictures_with_problems_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page(ProfileBuilder().childName("Alex").build(),
              {"1.2.3", "Example board", {{ConfigProblem::Kind::Setting, "settings.toml", "child.name"}, {ConfigProblem::Kind::File, "contacts.toml", ""}}});
    page.rig.language(language);
    page.draw();
    golden::expect(page.rig.frame, std::string("status_problems_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_page_gives_battery_time_left_child_contacts_queries_and_firmware);
  RUN_TEST(test_unknown_values_are_told_as_unknown_and_empty_ones_are_left_out);
  RUN_TEST(test_the_cable_is_told_with_the_battery_and_switching_off_says_unplug_first);
  RUN_TEST(test_the_number_of_contacts_follows_the_book);
  RUN_TEST(test_the_problems_of_the_configuration_are_counted_and_the_first_one_is_told);
  RUN_TEST(test_without_problems_there_is_no_line_about_the_configuration);
  RUN_TEST(test_enter_steps_through_the_visibility_of_remote_queries);
  RUN_TEST(test_switching_off_takes_two_presses_of_x);
  RUN_TEST(test_any_other_key_takes_the_first_press_back);
  RUN_TEST(test_a_capital_x_counts_too);
  RUN_TEST(test_a_cable_that_is_not_charging_is_still_a_cable);
  RUN_TEST(test_on_the_cable_the_application_fails_and_the_notice_says_why);
  RUN_TEST(test_restarting_takes_two_presses_of_r_and_logs_who_asked);
  RUN_TEST(test_a_capital_r_counts_too);
  RUN_TEST(test_any_other_key_takes_the_first_press_of_r_back);
  RUN_TEST(test_r_between_the_presses_of_x_takes_the_switch_off_back);
  RUN_TEST(test_leaving_the_page_takes_the_first_press_of_r_back);
  RUN_TEST(test_restarting_works_on_the_cable);
  RUN_TEST(test_back_goes_back_and_scrolling_is_offered_only_when_there_is_something_to_scroll);
  RUN_TEST(test_the_position_in_a_long_text_is_kept_while_nothing_changes);
  RUN_TEST(test_every_state_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  RUN_TEST(test_the_pictures_with_the_restart_asked_in_english_and_german);
  RUN_TEST(test_the_pictures_with_problems_in_english_and_german);
  return UNITY_END();
}
