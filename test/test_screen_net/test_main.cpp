// Host tests for the network page: the lines it gives, the SIM problems and their explanations, a
// network that allows messages only, scrolling when a language makes it long, and the layout.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/NetScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  NetScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").modemSleep(false).build()), screen(rig.context()) { start(); }
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
  bool says(const std::string& words) { return shows::text(rig.frame, words, FontSize::Small, kContent); }
  // A sentence that wraps: every line of it.
  bool saysWrapped(const std::string& words) {
    // The text block wraps inside its width less the scroll bar and a margin.
    const auto lines = text::wrap(font(FontSize::Small), words, kContent.w - 8);
    for (std::string_view line : lines)
      if (!says(std::string(line))) return false;
    return !lines.empty();
  }
};

}  // namespace

// ---- the lines ---------------------------------------------------------------------------------------------

void test_a_healthy_network_is_told_line_by_line() {
  Page page;
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Network", FontSize::Small, kContent));    // the title
  TEST_ASSERT_TRUE(page.says("Network: Example"));
  TEST_ASSERT_TRUE(page.says("Status: connected"));
  TEST_ASSERT_TRUE(page.says("Signal: 4/5"));
  TEST_ASSERT_TRUE(page.says("Cellular: on"));
  TEST_ASSERT_TRUE(page.says("Modem: awake"));
  TEST_ASSERT_TRUE(page.says("Time: known"));
  TEST_ASSERT_TRUE(page.says("Waiting: 0"));
  TEST_ASSERT_FALSE(page.says("SIM: PIN missing"));                       // a SIM line only for a problem
}

void test_no_network_is_told_as_not_connected() {
  Page page;
  page.rig.modem.setNetwork("", 0);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Network: No network"));
  TEST_ASSERT_TRUE(page.says("Status: not connected"));
  TEST_ASSERT_TRUE(page.says("Signal: 0/5"));
}

void test_cellular_off_the_modem_asleep_and_waiting_messages_are_told() {
  Page page;
  page.rig.core.setCellularEnabled(false);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Cellular: off"));
  TEST_ASSERT_TRUE(page.rig.core.send(kParentOne, "Hello"));
  page.draw();
  TEST_ASSERT_TRUE(page.says("Waiting: 1"));
  Page asleep(ProfileBuilder().childName("Alex").modemSleep(true).build());
  asleep.rig.run(120);
  asleep.draw();
  TEST_ASSERT_TRUE(asleep.rig.core.state().modemAsleep);
  TEST_ASSERT_TRUE(asleep.says("Modem: asleep"));
}

void test_an_unknown_time_is_told() {
  Page page;
  page.rig.clock.setEpochUtc(0);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Time: unknown"));
}

// ---- problems and their explanations --------------------------------------------------------------------

void test_every_sim_problem_has_its_line_and_its_explanation() {
  struct Case {
    SimStatus status;
    const char* line;
    const char* hint;
  } cases[] = {
      {SimStatus::Missing, "SIM: no card", "There is no SIM card in the device."},
      {SimStatus::PinRequired, "SIM: PIN missing", "The SIM needs a PIN. The parents have to enter it."},
      {SimStatus::PinWrong, "SIM: PIN wrong", "The PIN was wrong. Do not restart before it is right: three wrong tries lock the SIM."},
      {SimStatus::PukRequired, "SIM: locked", "The SIM is locked. Only the provider's PUK can save it."},
  };
  for (const Case& c : cases) {
    Page page;
    page.rig.modem.setSimStatus(c.status);
    page.draw();
    TEST_ASSERT_TRUE_MESSAGE(page.says(c.line), c.line);
    TEST_ASSERT_TRUE_MESSAGE(page.saysWrapped(c.hint), c.hint);
  }
}

void test_a_wrong_pin_warns_in_every_language_that_three_tries_lock_the_sim() {
  Page de;
  de.rig.language("de");
  de.rig.modem.setSimStatus(SimStatus::PinWrong);
  de.draw();
  TEST_ASSERT_TRUE(de.saysWrapped("Die PIN war falsch. Nicht neu starten, bevor sie stimmt: Drei Fehlversuche sperren die SIM."));
  Page fr;
  fr.rig.language("fr");
  fr.rig.modem.setSimStatus(SimStatus::PinWrong);
  fr.draw();
  TEST_ASSERT_TRUE(fr.saysWrapped("Le PIN \xC3\xA9tait faux. Ne pas red\xC3\xA9marrer avant d'avoir le bon : trois essais rat\xC3\xA9s bloquent la SIM."));
}

void test_a_network_that_allows_messages_only_is_explained_as_no_fault_of_the_device() {
  Page page;
  page.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Status: SMS only"));
  TEST_ASSERT_TRUE(page.saysWrapped("The network allows this SIM messages only, no calls."));
}

// Registered, but the IMS is not: telephony is not ready. Adults see the IMS line next to the network and
// an explanation that says it usually passes; it is neither "SMS only" nor "not connected".
void test_a_network_without_ims_is_told_as_calls_not_ready_and_explained() {
  Page page;
  page.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::ImsMissing);
  page.draw();
  TEST_ASSERT_TRUE(page.says("Network: Example"));
  TEST_ASSERT_TRUE(page.says("IMS: no"));
  TEST_ASSERT_TRUE(page.says("Status: calls not ready"));
  TEST_ASSERT_TRUE(page.saysWrapped("The network is there, but calls are not ready yet. Messages work."));
  TEST_ASSERT_FALSE(page.saysWrapped("The network allows this SIM messages only, no calls."));
}

void test_the_ims_line_says_yes_when_it_is_registered_and_nothing_where_it_is_unknown_or_irrelevant() {
  Page page;
  TEST_ASSERT_TRUE(page.says("IMS: yes"));
  page.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly);   // a restriction of the network: no word on the IMS
  page.draw();
  TEST_ASSERT_FALSE(page.says("IMS:"));
  Page none;
  none.rig.modem.setNetwork("", 0);
  none.draw();
  TEST_ASSERT_FALSE(none.says("IMS:"));
}

void test_the_ims_line_is_worded_in_german_and_french() {
  Page de;
  de.rig.language("de");
  de.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::ImsMissing);
  de.draw();
  TEST_ASSERT_TRUE(de.says("IMS: nein"));
  Page fr;
  fr.rig.language("fr");
  fr.draw();
  TEST_ASSERT_TRUE(fr.says("IMS : oui"));
}

void test_no_explanation_where_nothing_is_wrong() {
  Page page;
  TEST_ASSERT_FALSE(page.saysWrapped("The network allows this SIM messages only, no calls."));
}

// ---- scrolling and keys ----------------------------------------------------------------------------------

void test_a_text_longer_than_the_page_scrolls_and_says_so_in_the_soft_keys() {
  Page page;
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  page.rig.language("qps-ploc");
  page.rig.modem.setSimStatus(SimStatus::PinWrong);
  page.draw();
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  const std::string before = page.rig.frame.toAscii();
  page.key(Key::Down);
  TEST_ASSERT_TRUE(before != page.rig.frame.toAscii());
  page.key(Key::Up);
  TEST_ASSERT_TRUE(before == page.rig.frame.toAscii());
}

void test_the_page_starts_at_the_top_each_time() {
  Page page;
  page.rig.language("qps-ploc");
  page.rig.modem.setSimStatus(SimStatus::PinWrong);
  page.draw();
  const std::string top = page.rig.frame.toAscii();
  page.key(Key::Down);
  page.key(Key::Down);
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_TRUE(top == page.rig.frame.toAscii());
}

void test_back_goes_back_and_there_is_no_action() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

// ---- layout -------------------------------------------------------------------------------------------------------

void test_every_state_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 7; ++state) {
      Page page;
      page.rig.language(language);
      if (state == 1) page.rig.modem.setSimStatus(SimStatus::Missing);
      if (state == 2) page.rig.modem.setSimStatus(SimStatus::PinRequired);
      if (state == 3) page.rig.modem.setSimStatus(SimStatus::PinWrong);
      if (state == 4) page.rig.modem.setSimStatus(SimStatus::PukRequired);
      if (state == 5) page.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly);
      if (state == 6) page.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::ImsMissing);
      FrameBuffer fb;
      page.draw();
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/screen-net.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " state " + std::to_string(state) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page healthy;
    healthy.rig.language(language);
    healthy.draw();
    golden::expect(healthy.rig.frame, std::string("net_ok_") + language);
    Page locked;
    locked.rig.language(language);
    locked.rig.modem.setSimStatus(SimStatus::PinWrong);
    locked.draw();
    golden::expect(locked.rig.frame, std::string("net_pin_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_healthy_network_is_told_line_by_line);
  RUN_TEST(test_no_network_is_told_as_not_connected);
  RUN_TEST(test_cellular_off_the_modem_asleep_and_waiting_messages_are_told);
  RUN_TEST(test_an_unknown_time_is_told);
  RUN_TEST(test_every_sim_problem_has_its_line_and_its_explanation);
  RUN_TEST(test_a_wrong_pin_warns_in_every_language_that_three_tries_lock_the_sim);
  RUN_TEST(test_a_network_that_allows_messages_only_is_explained_as_no_fault_of_the_device);
  RUN_TEST(test_a_network_without_ims_is_told_as_calls_not_ready_and_explained);
  RUN_TEST(test_the_ims_line_says_yes_when_it_is_registered_and_nothing_where_it_is_unknown_or_irrelevant);
  RUN_TEST(test_the_ims_line_is_worded_in_german_and_french);
  RUN_TEST(test_no_explanation_where_nothing_is_wrong);
  RUN_TEST(test_a_text_longer_than_the_page_scrolls_and_says_so_in_the_soft_keys);
  RUN_TEST(test_the_page_starts_at_the_top_each_time);
  RUN_TEST(test_back_goes_back_and_there_is_no_action);
  RUN_TEST(test_every_state_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
