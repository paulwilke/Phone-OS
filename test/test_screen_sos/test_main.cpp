// Host tests for the SOS page: the choice and its countdown, school time and the five taps, the
// progress of the ping and how it ended, what leaving does, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <RefreshRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/SosScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

Profile inSchool() { return ProfileBuilder().childName("Alex").schoolTime("tue", "14:00", "15:00").build(); }

struct Page {
  DeviceRig rig;
  SosScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { rig.run(10); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { rig.run(10); }
  // Pressing the SOS tile: the core starts the search, the page opens.
  void press() {
    TEST_ASSERT_TRUE(rig.core.beginSos());
    rig.poll();
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void seconds(uint32_t n) {
    rig.run(n);
    draw();
  }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  void letter(char c) {
    screen.handle(Input::text(static_cast<uint32_t>(c)));
    draw();
  }
  bool says(const std::string& words, FontSize size, bool onBlack = false) { return shows::text(rig.frame, words, size, kContent, onBlack); }
  // A sentence that wraps: every line of it.
  bool saysWrapped(const std::string& words, FontSize size, int width, bool onBlack = false) {
    const auto lines = text::wrap(font(size), words, width);
    for (std::string_view line : lines)
      if (!says(std::string(line), size, onBlack)) return false;
    return !lines.empty();
  }
};

// How many of the three progress bars are filled: the bars are the 26-pixel-high boxes.
int filledBars(const FrameBuffer& fb, const Rect& area) {
  // The bars sit below the "what was chosen" line; find a row where the three of them have ink.
  const int barWidth = (area.w - 24 - 16) / 3;
  for (int y = area.y + 40; y < area.bottom() - 30; ++y) {
    int filled = 0, outlined = 0;
    for (int i = 0; i < 3; ++i) {
      const int x = area.x + 12 + i * (barWidth + 8) + barWidth / 2;
      if (fb.pixel(x, y + 13) && fb.pixel(x - 5, y + 13) && fb.pixel(x + 5, y + 13) && fb.pixel(x, y + 8)) ++filled;
      else if (fb.pixel(x, y) && !fb.pixel(x, y + 13)) ++outlined;
    }
    if (filled + outlined == 3) return filled;
  }
  return -1;
}

// A page that does nothing, for the places in a stack where the SOS page is not the one under test:
// a second SosScreen there would release the search itself when it is left.
struct Blank : Screen {
  bool handle(const Input&) override { return false; }
  void render(Display&) override {}
};

}  // namespace

// ---- the choice ------------------------------------------------------------------------------------

void test_pressing_sos_shows_the_choice_with_its_countdown_and_the_search_is_already_running() {
  Page page;
  page.press();
  TEST_ASSERT_TRUE(page.says("SOS", FontSize::Large));
  TEST_ASSERT_TRUE(page.says("Emergency in 40 s", FontSize::Text));
  // The field in focus is inverted; a field's words wrap inside the box rather than being cut.
  const int inner = kContent.w - 32;           // the box is 8 px in from the area, its words 8 px in from the box
  TEST_ASSERT_TRUE(page.saysWrapped("1 Emergency! I am here", FontSize::Text, inner, true));
  TEST_ASSERT_TRUE(page.saysWrapped("2 Come and get me", FontSize::Text, inner));
  TEST_ASSERT_TRUE(page.saysWrapped("3 Cancel", FontSize::Text, inner));
  TEST_ASSERT_TRUE(page.rig.core.state().positioningActive);         // before anybody has chosen
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
}

void test_the_countdown_runs_down() {
  Page page;
  page.press();
  page.seconds(10);
  TEST_ASSERT_TRUE(page.says("Emergency in 30 s", FontSize::Text));
  TEST_ASSERT_FALSE(page.says("Emergency in 40 s", FontSize::Text));
}

void test_the_number_keys_choose_at_once() {
  Page one;
  one.press();
  one.letter('1');
  TEST_ASSERT_TRUE(one.rig.core.state().emergencyRunning);
  TEST_ASSERT_TRUE(one.rig.core.state().pingKind == SosPing::Kind::Emergency);
  Page two;
  two.press();
  two.letter('2');
  TEST_ASSERT_TRUE(two.rig.core.state().emergencyRunning);
  TEST_ASSERT_TRUE(two.rig.core.state().pingKind == SosPing::Kind::Pickup);
  Page three;
  three.press();
  three.letter('3');
  TEST_ASSERT_EQUAL_STRING("back", three.rig.nav.last().c_str());
  TEST_ASSERT_FALSE(three.rig.core.state().emergencyRunning);
  Page other;
  other.press();
  TEST_ASSERT_FALSE(other.screen.handle(Input::text('4')));
  TEST_ASSERT_FALSE(other.screen.handle(Input::text('a')));
  TEST_ASSERT_TRUE(other.rig.core.state().sosPhase == SosFlow::Phase::Choosing);
}

void test_the_arrows_move_the_selection_and_enter_chooses_it() {
  Page page;
  page.press();
  page.key(Key::Down);                                               // the second field
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.state().pingKind == SosPing::Kind::Pickup);
  Page wrap;
  wrap.press();
  wrap.key(Key::Up);                                                 // wraps to the last: Cancel
  wrap.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("back", wrap.rig.nav.last().c_str());
  Page first;
  first.press();
  first.key(Key::Select);                                            // the first field: the emergency
  TEST_ASSERT_TRUE(first.rig.core.state().pingKind == SosPing::Kind::Emergency);
}

void test_back_cancels_the_choice() {
  Page page;
  page.press();
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  TEST_ASSERT_FALSE(page.rig.core.state().emergencyRunning);
}

void test_a_tap_on_a_field_chooses_it_and_a_tap_beside_the_fields_does_nothing() {
  Page page;
  page.press();
  const Rect second = shows::where(page.rig.frame, "2 Come and get me", FontSize::Text, kContent).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(second.x + 4, second.y + 4)));
  TEST_ASSERT_TRUE(page.rig.core.state().pingKind == SosPing::Kind::Pickup);
  Page beside;
  beside.press();
  TEST_ASSERT_FALSE(beside.screen.handle(Input::touch(2, kContent.y + 100)));
  TEST_ASSERT_TRUE(beside.rig.core.state().sosPhase == SosFlow::Phase::Choosing);
  Page cancel;
  cancel.press();
  const Rect third = shows::where(cancel.rig.frame, "3 Cancel", FontSize::Text, kContent).value();
  cancel.screen.handle(Input::touch(third.x + 4, third.y + 4));
  TEST_ASSERT_EQUAL_STRING("back", cancel.rig.nav.last().c_str());
}

void test_without_a_choice_the_emergency_goes_out_by_itself_and_the_page_shows_the_search() {
  Page page;
  page.press();
  page.seconds(41);
  TEST_ASSERT_TRUE(page.rig.core.state().sosPhase == SosFlow::Phase::Idle);
  TEST_ASSERT_TRUE(page.rig.core.state().pingKind == SosPing::Kind::Emergency);
  TEST_ASSERT_TRUE(page.rig.core.state().emergencyRunning);
  TEST_ASSERT_FALSE(page.says("Emergency in 0 s", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Locating", FontSize::Text) || page.says("Position found", FontSize::Text));
}

// ---- leaving ------------------------------------------------------------------------------------------

void test_leaving_the_choice_releases_the_search_and_sends_nothing() {
  Page page;
  page.press();
  TEST_ASSERT_TRUE(page.rig.core.state().positioningActive);
  page.screen.onLeave();                                             // whatever the way out
  page.rig.poll();
  TEST_ASSERT_TRUE(page.rig.core.state().sosPhase == SosFlow::Phase::Idle);
  TEST_ASSERT_FALSE(page.rig.core.state().positioningActive);
  page.rig.run(60);
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
}

void test_leaving_does_not_stop_a_running_ping_so_the_key_says_back_and_not_cancel() {
  Page page;
  page.press();
  page.letter('1');
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  page.key(Key::Back);
  page.screen.onLeave();
  page.rig.run(30);
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());                // the text goes out all the same
}

void test_every_way_out_of_the_page_releases_the_search_through_the_stack() {
  DeviceRig rig;
  ScreenStack stack(screen::kHome);
  const ScreenContext context = rig.context(stack);
  SosScreen sos(context);
  Blank home, lock;
  stack.add(screen::kHome, home);
  stack.add(screen::kSos, sos);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.begin(0);
  for (int how = 0; how < 3; ++how) {
    rig.run(10);
    TEST_ASSERT_TRUE(rig.core.beginSos());
    stack.open(screen::kSos);
    rig.poll();
    TEST_ASSERT_TRUE(rig.core.state().positioningActive);
    if (how == 0) stack.back();
    if (how == 1) stack.home();
    if (how == 2) stack.lock();
    rig.poll();
    TEST_ASSERT_TRUE_MESSAGE(rig.core.state().sosPhase == SosFlow::Phase::Idle, std::to_string(how).c_str());
    TEST_ASSERT_FALSE_MESSAGE(rig.core.state().positioningActive, std::to_string(how).c_str());
    stack.unlock();
  }
  TEST_ASSERT_EQUAL(0, rig.modem.sent().size());
}

void test_the_page_keeps_the_device_from_locking_while_an_emergency_is_open_or_running() {
  Page page;
  TEST_ASSERT_FALSE(page.screen.preventsAutoLock());
  page.press();
  TEST_ASSERT_TRUE(page.screen.preventsAutoLock());                  // the choice: locking would cancel it
  page.letter('1');
  TEST_ASSERT_TRUE(page.screen.preventsAutoLock());                  // the search
  page.seconds(40);
  TEST_ASSERT_TRUE(page.rig.core.state().pingStage == SosPing::Stage::Done);
  TEST_ASSERT_FALSE(page.screen.preventsAutoLock());                 // over: the device may lock
}

// ---- school time ------------------------------------------------------------------------------------------

void test_in_school_time_the_page_warns_and_shows_the_number_of_taps_instead_of_the_choice() {
  Page page(inSchool());
  page.press();
  TEST_ASSERT_TRUE(page.rig.core.state().sosPhase == SosFlow::Phase::QuietConfirm);
  TEST_ASSERT_TRUE(page.saysWrapped("School time until 15:00. The device is locked.", FontSize::Text, kContent.w - 16));
  TEST_ASSERT_TRUE(page.saysWrapped("A real emergency? Tap 5 times, each within 10 s of the last.", FontSize::Small, kContent.w - 16));
  TEST_ASSERT_FALSE(page.says("2 Come and get me", FontSize::Text));  // only the emergency in school time
  TEST_ASSERT_TRUE(page.says("5", FontSize::Large, true));
  TEST_ASSERT_TRUE(page.rig.core.state().positioningActive);          // the search already runs
  TEST_ASSERT_EQUAL_STRING("Tap", page.screen.softKeys().action.c_str());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
}

void test_five_taps_send_the_emergency_and_lift_the_lock_and_four_do_not() {
  Page page(inSchool());
  page.press();
  for (int i = 0; i < 4; ++i) page.key(Key::Select);
  TEST_ASSERT_TRUE(page.says("1", FontSize::Large, true));
  TEST_ASSERT_TRUE(page.rig.core.state().sosPhase == SosFlow::Phase::QuietConfirm);
  TEST_ASSERT_TRUE(page.rig.core.state().schoolTime);                // four taps are not enough
  page.key(Key::Select);                                             // the fifth
  TEST_ASSERT_TRUE(page.rig.core.state().sosPhase == SosFlow::Phase::Idle);
  TEST_ASSERT_TRUE(page.rig.core.state().pingKind == SosPing::Kind::Emergency);
  TEST_ASSERT_FALSE(page.rig.core.state().schoolTime);               // the lock is lifted
  TEST_ASSERT_TRUE(page.says("Locating", FontSize::Text));           // and the page shows the search
}

void test_the_taps_may_be_given_on_the_button_but_not_beside_it() {
  Page page(inSchool());
  page.press();
  TEST_ASSERT_FALSE(page.screen.handle(Input::touch(120, kContent.y + 20)));
  TEST_ASSERT_FALSE(page.screen.handle(Input::touch(5, kContent.bottom() - 10)));
  TEST_ASSERT_EQUAL(5, page.rig.core.state().sosTapsLeft);
  for (int i = 0; i < 5; ++i) TEST_ASSERT_TRUE(page.screen.handle(Input::touch(120, kContent.bottom() - 60)));
  TEST_ASSERT_TRUE(page.rig.core.state().emergencyRunning);
}

void test_taps_that_are_ten_seconds_apart_start_again_at_five() {
  Page page(inSchool());
  page.press();
  for (int i = 0; i < 4; ++i) page.key(Key::Select);
  page.seconds(11);                                                  // a pocket taps now and then
  TEST_ASSERT_EQUAL(5, page.rig.core.state().sosTapsLeft);
  TEST_ASSERT_TRUE(page.says("5", FontSize::Large, true));
  TEST_ASSERT_FALSE(page.rig.core.state().emergencyRunning);
}

void test_back_from_the_school_time_warning_changes_nothing() {
  Page page(inSchool());
  page.press();
  page.key(Key::Select);
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  page.screen.onLeave();
  page.rig.poll();
  TEST_ASSERT_TRUE(page.rig.core.state().schoolTime);                // the lock stays
  TEST_ASSERT_FALSE(page.rig.core.state().positioningActive);
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
}

// ---- the ping -------------------------------------------------------------------------------------------------

void test_the_search_is_shown_as_it_goes_and_ends_with_how_accurate_it_was_and_to_whom() {
  Page page;
  page.rig.gnss.fixAfter(10, 48.13745, 11.5755, 1.2f);
  page.press();
  page.letter('1');
  TEST_ASSERT_TRUE(page.says("Emergency! I am here", FontSize::Small));        // what was chosen
  TEST_ASSERT_TRUE(page.says("Locating", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Searching for 0 s", FontSize::Small));
  TEST_ASSERT_EQUAL(1, filledBars(page.rig.frame, kContent));                  // the first bar: the search has begun
  page.seconds(5);
  TEST_ASSERT_TRUE(page.says("Searching for 5 s", FontSize::Small));
  page.seconds(7);                                                             // a position at 10 s
  TEST_ASSERT_TRUE(page.says("Position found", FontSize::Text));
  TEST_ASSERT_TRUE(page.saysWrapped("Getting more exact ... 12 s, now " + std::to_string(SosPing::accuracyMeters(1.2f)) + " m", FontSize::Small, kContent.w - 16));
  TEST_ASSERT_EQUAL(2, filledBars(page.rig.frame, kContent));
  page.seconds(15);                                                            // sent after the minimum time
  TEST_ASSERT_TRUE(page.rig.core.state().pingStage == SosPing::Stage::Done);
  TEST_ASSERT_TRUE(page.says("Sent", FontSize::Text));
  TEST_ASSERT_EQUAL(3, filledBars(page.rig.frame, kContent));
  TEST_ASSERT_TRUE(page.saysWrapped("Position accurate to " + std::to_string(SosPing::accuracyMeters(1.2f)) + " m", FontSize::Small, kContent.w - 16));
  TEST_ASSERT_TRUE(page.says("To Parent One", FontSize::Small));
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.modem.sent()[0].phone.c_str());
  TEST_ASSERT_FALSE(page.rig.core.state().positioningActive);                 // the receiver is off again
}

void test_a_pick_up_says_so_and_goes_to_the_same_person() {
  Page page;
  page.press();
  page.letter('2');
  TEST_ASSERT_TRUE(page.says("Come and get me", FontSize::Small));
  TEST_ASSERT_FALSE(page.says("Emergency! I am here", FontSize::Small));
  page.seconds(35);
  TEST_ASSERT_TRUE(page.says("Sent", FontSize::Text));
}

void test_without_any_position_it_is_sent_all_the_same_and_says_so() {
  Page page;
  page.rig.gnss.never();
  page.press();
  page.letter('1');
  page.seconds(130);
  TEST_ASSERT_TRUE(page.rig.core.state().pingStage == SosPing::Stage::Done);
  TEST_ASSERT_TRUE(page.says("Sent", FontSize::Text));
  TEST_ASSERT_TRUE(page.saysWrapped("No position: no GPS reception", FontSize::Small, kContent.w - 16));
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
}

void test_a_text_that_waits_for_the_network_says_so() {
  Page page;
  page.rig.modem.setNetwork("", 0);
  page.rig.poll();
  page.press();
  page.letter('1');
  page.seconds(35);
  TEST_ASSERT_TRUE(page.says("Sent", FontSize::Text));
  TEST_ASSERT_TRUE(page.saysWrapped("To Parent One, waits for network", FontSize::Small, kContent.w - 16));
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
}

void test_a_text_that_cannot_be_sent_says_not_sent_and_try_again_starts_over() {
  Page page;
  page.rig.core.setCellularEnabled(false);
  for (size_t i = 0; i < Messaging::kOutboxSize; ++i) TEST_ASSERT_TRUE(page.rig.core.send(kParentTwo, "x"));
  page.press();
  page.letter('1');
  page.seconds(35);
  TEST_ASSERT_TRUE(page.rig.core.state().pingStage == SosPing::Stage::Failed);
  TEST_ASSERT_TRUE(page.says("Not sent", FontSize::Text));
  TEST_ASSERT_TRUE(page.saysWrapped("No network and no radio. Try again.", FontSize::Small, kContent.w - 16));
  TEST_ASSERT_EQUAL_STRING("Try again", page.screen.softKeys().action.c_str());
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL(2, filledBars(page.rig.frame, kContent));        // never three bars for a text that did not go out
  page.key(Key::Select);                                             // again: the choice
  TEST_ASSERT_TRUE(page.rig.core.state().sosPhase == SosFlow::Phase::Choosing);
  TEST_ASSERT_TRUE(page.says("Emergency in 40 s", FontSize::Text));
}

void test_a_remote_query_is_not_the_childs_business_and_the_page_says_nothing_runs() {
  Page page;
  page.rig.modem.injectSms(kParentTwo, "#WhereAreYou", 1);
  page.rig.run(40);
  page.draw();
  // The query really is under way (or done), so that the page has something it must decline to show.
  TEST_ASSERT_TRUE(page.rig.core.state().pingKind == SosPing::Kind::Query);
  TEST_ASSERT_TRUE(page.rig.core.state().pingStage == SosPing::Stage::Done || page.rig.core.state().pingStage == SosPing::Stage::Failed);
  TEST_ASSERT_FALSE(page.rig.core.state().emergencyRunning);
  TEST_ASSERT_TRUE(page.saysWrapped("No emergency is running", FontSize::Text, kContent.w - 16));
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_the_words_in_german_and_french() {
  Page page;
  page.rig.language("de");
  page.press();
  TEST_ASSERT_TRUE(page.says("Notfall in 40 s", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("3 Abbrechen", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("W\xC3\xA4hlen", page.screen.softKeys().action.c_str());
  page.letter('1');
  TEST_ASSERT_TRUE(page.says("Ortung l\xC3\xA4uft", FontSize::Text));
  page.rig.language("fr");
  page.draw();
  TEST_ASSERT_TRUE(page.says("Localisation", FontSize::Text));
}

// ---- the layout --------------------------------------------------------------------------------------------------------

void test_every_phase_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int phase = 0; phase < 6; ++phase) {
      Page page(phase == 1 ? inSchool() : ProfileBuilder().childName("Alex").build());
      page.rig.language(language);
      if (phase == 5) page.rig.core.setCellularEnabled(false);
      if (phase == 5) for (size_t i = 0; i < Messaging::kOutboxSize; ++i) page.rig.core.send(kParentTwo, "x");
      page.press();
      if (phase == 2) page.letter('1');
      if (phase == 3) { page.letter('1'); page.seconds(5); }
      if (phase == 4) { page.letter('1'); page.seconds(40); }
      if (phase == 5) { page.letter('1'); page.seconds(40); }
      FrameBuffer fb;
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows
      // may run into the soft-key bar there (as documented for the other pages), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " phase " + std::to_string(phase) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    const char* names[] = {"choice", "quiet", "searching", "found", "sent", "failed"};
    for (int phase = 0; phase < 6; ++phase) {
      Page page(phase == 1 ? inSchool() : ProfileBuilder().childName("Alex").build());
      page.rig.language(language);
      page.rig.gnss.fixAfter(10, 48.13745, 11.5755, 1.2f);
      if (phase == 5) page.rig.core.setCellularEnabled(false);
      if (phase == 5) for (size_t i = 0; i < Messaging::kOutboxSize; ++i) page.rig.core.send(kParentTwo, "x");
      page.press();
      if (phase == 2) page.letter('1');
      if (phase == 3) { page.letter('1'); page.seconds(12); }
      if (phase == 4) { page.letter('1'); page.seconds(40); }
      if (phase == 5) { page.letter('1'); page.seconds(40); }
      golden::expect(page.rig.frame, std::string("sos_") + names[phase] + "_" + language);
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pressing_sos_shows_the_choice_with_its_countdown_and_the_search_is_already_running);
  RUN_TEST(test_the_countdown_runs_down);
  RUN_TEST(test_the_number_keys_choose_at_once);
  RUN_TEST(test_the_arrows_move_the_selection_and_enter_chooses_it);
  RUN_TEST(test_back_cancels_the_choice);
  RUN_TEST(test_a_tap_on_a_field_chooses_it_and_a_tap_beside_the_fields_does_nothing);
  RUN_TEST(test_without_a_choice_the_emergency_goes_out_by_itself_and_the_page_shows_the_search);
  RUN_TEST(test_leaving_the_choice_releases_the_search_and_sends_nothing);
  RUN_TEST(test_leaving_does_not_stop_a_running_ping_so_the_key_says_back_and_not_cancel);
  RUN_TEST(test_every_way_out_of_the_page_releases_the_search_through_the_stack);
  RUN_TEST(test_the_page_keeps_the_device_from_locking_while_an_emergency_is_open_or_running);
  RUN_TEST(test_in_school_time_the_page_warns_and_shows_the_number_of_taps_instead_of_the_choice);
  RUN_TEST(test_five_taps_send_the_emergency_and_lift_the_lock_and_four_do_not);
  RUN_TEST(test_the_taps_may_be_given_on_the_button_but_not_beside_it);
  RUN_TEST(test_taps_that_are_ten_seconds_apart_start_again_at_five);
  RUN_TEST(test_back_from_the_school_time_warning_changes_nothing);
  RUN_TEST(test_the_search_is_shown_as_it_goes_and_ends_with_how_accurate_it_was_and_to_whom);
  RUN_TEST(test_a_pick_up_says_so_and_goes_to_the_same_person);
  RUN_TEST(test_without_any_position_it_is_sent_all_the_same_and_says_so);
  RUN_TEST(test_a_text_that_waits_for_the_network_says_so);
  RUN_TEST(test_a_text_that_cannot_be_sent_says_not_sent_and_try_again_starts_over);
  RUN_TEST(test_a_remote_query_is_not_the_childs_business_and_the_page_says_nothing_runs);
  RUN_TEST(test_the_words_in_german_and_french);
  RUN_TEST(test_every_phase_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
