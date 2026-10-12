// Host tests for the power page: the first view, the question before anything is switched off, the cable,
// the keys and the touch, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/OptionRows.h>
#include <ui/PageTitle.h>
#include <ui/Text.h>
#include <ui/screens/PowerScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  PowerScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) {
    rig.poll();
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  bool says(const std::string& text, bool focused = false) {
    return shows::text(rig.frame, text, FontSize::Text, kContent, focused) || shows::text(rig.frame, text, FontSize::Small, kContent, focused);
  }
  // The centre of box `index` as drawn at the top of the list, with the title `heading`.
  Input tap(size_t index, const std::string& heading) {
    OptionRows probe;
    probe.setRows({{"a", ""}, {"b", ""}});
    const int header = pageTitleHeight(kContent, heading);
    const Rect box = probe.rowRect({kContent.x, kContent.y + header, kContent.w, kContent.h - header}, index);
    return Input::touch(box.x + box.w / 2, box.y + box.h / 2);
  }
};

}  // namespace

void test_the_first_view_offers_switching_off_and_asks_nothing_yet() {
  Page page;
  TEST_ASSERT_TRUE(page.says("Switch off", true));
  TEST_ASSERT_FALSE(page.says("Switch off?"));
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_choosing_it_asks_with_cancel_in_focus_and_nothing_is_switched_off() {
  Page page;
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.says("Switch off?"));
  TEST_ASSERT_TRUE(page.says("Cancel", true));              // the focus starts on the harmless one
  TEST_ASSERT_TRUE(page.says("Yes, switch off"));
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_cancel_goes_back_to_the_first_view_and_does_not_switch_off() {
  Page page;
  page.key(Key::Select);
  page.key(Key::Select);                                    // Select on "Cancel"
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
  TEST_ASSERT_FALSE(page.says("Cancel"));
  TEST_ASSERT_TRUE(page.says("Switch off", true));
}

void test_yes_switches_off_and_only_yes() {
  Page page;
  page.key(Key::Select);
  page.key(Key::Down);
  TEST_ASSERT_TRUE(page.says("Yes, switch off", true));
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.core.powerOffRequested());
}

void test_back_from_the_question_returns_to_the_first_view_and_then_leaves() {
  Page page;
  page.key(Key::Select);
  page.rig.nav.clear();
  page.key(Key::Back);
  TEST_ASSERT_TRUE(page.says("Switch off", true));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());          // still on the page
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_leaving_the_page_forgets_the_question() {
  Page page;
  page.key(Key::Select);
  page.screen.onLeave();
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_FALSE(page.says("Switch off?"));
  page.key(Key::Select);                                    // asks again, switches nothing off
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

// On the cable the board stays on whatever the device does: the box says to unplug and nothing is asked.
void test_on_the_cable_it_says_unplug_first_and_never_asks() {
  Page page;
  page.rig.power.plugIn(ChargeState::Charging);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says("unplug USB first", true));
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.says("Switch off?"));
  TEST_ASSERT_FALSE(page.rig.core.powerOffRequested());
}

void test_a_tap_works_like_select() {
  Page page;
  TEST_ASSERT_TRUE(page.screen.handle(page.tap(0, "Power")));
  page.draw();
  TEST_ASSERT_TRUE(page.says("Switch off?"));
  TEST_ASSERT_TRUE(page.screen.handle(page.tap(1, "Switch off?")));   // "Yes, switch off"
  TEST_ASSERT_TRUE(page.rig.core.powerOffRequested());
}

void test_the_soft_keys_say_what_they_do() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
}

void test_every_view_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int cable = 0; cable < 2; ++cable) {
      for (int asking = 0; asking < 2; ++asking) {
        Page page;
        page.rig.language(language);
        if (cable) {
          page.rig.power.plugIn(ChargeState::Charging);
          page.rig.poll();
        }
        if (asking && !cable) page.key(Key::Select);
        FrameBuffer fb;
        page.screen.render(fb);
        bool outside = false;
        for (const auto& e : fb.clipEvents())
          if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
        TEST_ASSERT_FALSE_MESSAGE(outside, (language + " cable " + std::to_string(cable) + " asking " + std::to_string(asking) + ":\n" + fb.describeClips()).c_str());
      }
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page;
    page.rig.language(language);
    page.draw();
    golden::expect(page.rig.frame, std::string("power_") + language);
    page.key(Key::Select);
    golden::expect(page.rig.frame, std::string("power_ask_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_first_view_offers_switching_off_and_asks_nothing_yet);
  RUN_TEST(test_choosing_it_asks_with_cancel_in_focus_and_nothing_is_switched_off);
  RUN_TEST(test_cancel_goes_back_to_the_first_view_and_does_not_switch_off);
  RUN_TEST(test_yes_switches_off_and_only_yes);
  RUN_TEST(test_back_from_the_question_returns_to_the_first_view_and_then_leaves);
  RUN_TEST(test_leaving_the_page_forgets_the_question);
  RUN_TEST(test_on_the_cable_it_says_unplug_first_and_never_asks);
  RUN_TEST(test_a_tap_works_like_select);
  RUN_TEST(test_the_soft_keys_say_what_they_do);
  RUN_TEST(test_every_view_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
