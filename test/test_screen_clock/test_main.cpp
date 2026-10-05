// Host tests for the clock screen: the time and the date from the core's wall clock, the page
// without a time, back, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Text.h>
#include <ui/Wording.h>
#include <ui/screens/ClockScreen.h>

using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

bool says(const FrameBuffer& fb, const std::string& words, FontSize size) {
  return shows::text(fb, words, size, kContent);
}

}  // namespace

void test_it_shows_the_time_the_weekday_the_date_and_where_the_time_comes_from() {
  DeviceRig rig;
  ClockScreen clock(rig.context());
  rig.poll();
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "14:32", FontSize::Large));
  TEST_ASSERT_TRUE(says(rig.frame, "Tuesday", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "September 15, 2026", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "Time from the network", FontSize::Small));
}

void test_it_writes_the_date_the_way_each_language_does() {
  DeviceRig rig;
  ClockScreen clock(rig.context());
  rig.poll();
  rig.language("de");
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "14:32", FontSize::Large));
  TEST_ASSERT_TRUE(says(rig.frame, "Dienstag", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "15. September 2026", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "Zeit aus dem Netz", FontSize::Small));
  rig.language("fr");
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "mardi", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "15 septembre 2026", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "Heure du r\xC3\xA9seau", FontSize::Small));
}

void test_the_time_follows_the_clock_and_the_zone() {
  DeviceRig rig;
  ClockScreen clock(rig.context());
  rig.clock.setEpochUtc(rig::kTuesdayAfternoon + 10 * 3600);   // 22:32 UTC, 00:32 the next day two hours east
  rig.poll();
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "00:32", FontSize::Large));
  TEST_ASSERT_TRUE(says(rig.frame, "Wednesday", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "September 16, 2026", FontSize::Text));
}

void test_the_year_changes_with_the_wall_clock() {
  DeviceRig rig;
  ClockScreen clock(rig.context());
  rig.clock.setEpochUtc(1798759800);                           // 2026-12-31 23:30 UTC: already 2027 in the zone
  rig.poll();
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "Friday", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "January 1, 2027", FontSize::Text));
  TEST_ASSERT_TRUE(says(rig.frame, "01:30", FontSize::Large));
}

void test_without_a_time_it_says_so_and_invents_nothing() {
  DeviceRig rig;
  ClockScreen clock(rig.context());
  rig.clock.setEpochUtc(0);
  rig.poll();
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "--:--", FontSize::Large));
  TEST_ASSERT_TRUE(says(rig.frame, "No time from the network yet", FontSize::Small));
  TEST_ASSERT_FALSE(says(rig.frame, "Time from the network", FontSize::Small));
  for (const char* day : {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"})
    TEST_ASSERT_FALSE_MESSAGE(says(rig.frame, day, FontSize::Text), day);       // no weekday, no date
  rig.language("de");
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "Noch keine Zeit vom Netz", FontSize::Small));
}

void test_back_goes_back_and_nothing_else_is_used() {
  DeviceRig rig;
  ClockScreen clock(rig.context());
  rig.poll();
  clock.render(rig.frame);
  TEST_ASSERT_TRUE(clock.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", rig.nav.last().c_str());
  rig.nav.clear();
  TEST_ASSERT_FALSE(clock.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_FALSE(clock.handle(Input::pressed(Key::Up)));
  TEST_ASSERT_FALSE(clock.handle(Input::text('x')));
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
  // The soft key is Back, and tapping it goes back.
  TEST_ASSERT_EQUAL_STRING("Back", clock.softKeys().back.c_str());
  TEST_ASSERT_TRUE(clock.softKeys().action.empty());
  const SoftKeySlots slots = softKeySlots(clock.softKeys(), ScreenLayout::standard().softKeys);
  TEST_ASSERT_TRUE(clock.handle(Input::touch(slots.back.x + 3, slots.back.y + 3)));
  TEST_ASSERT_EQUAL_STRING("back", rig.nav.last().c_str());
}

void test_every_weekday_and_month_fits_the_screen_in_the_shipped_languages() {
  std::string tooWide;
  languages::forEach([&](const std::string& language, i18n::Translator& t) {
    if (language == i18n::kPseudoCode) return;
    for (int month = 1; month <= 12; ++month) {
      AppCore::State::WallClock wall;
      wall.known = true;
      wall.year = 2026;
      wall.month = month;
      wall.day = 28;
      wall.weekday = month % 7;
      for (const std::string& line : {wording::dateLine(wall, t), wording::weekdayName(wall, t)}) {
        const int width = text::width(font(FontSize::Text), line);
        if (width > kContent.w - 8) tooWide += language + ": " + line + " (" + std::to_string(width) + " px)\n";
      }
    }
  });
  TEST_ASSERT_TRUE_MESSAGE(tooWide.empty(), tooWide.c_str());
}

void test_the_clock_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int known = 0; known < 2; ++known) {
      DeviceRig rig;
      rig.language(language);
      ClockScreen clock(rig.context());
      if (!known) rig.clock.setEpochUtc(0);
      rig.poll();
      FrameBuffer fb;
      clock.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_clock_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig;
    rig.language(language);
    ClockScreen clock(rig.context());
    rig.poll();
    clock.render(rig.frame);
    golden::expect(rig.frame, std::string("clock_") + language);
    rig.clock.setEpochUtc(0);
    rig.poll();
    clock.render(rig.frame);
    golden::expect(rig.frame, std::string("clock_no_time_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_it_shows_the_time_the_weekday_the_date_and_where_the_time_comes_from);
  RUN_TEST(test_it_writes_the_date_the_way_each_language_does);
  RUN_TEST(test_the_time_follows_the_clock_and_the_zone);
  RUN_TEST(test_the_year_changes_with_the_wall_clock);
  RUN_TEST(test_without_a_time_it_says_so_and_invents_nothing);
  RUN_TEST(test_back_goes_back_and_nothing_else_is_used);
  RUN_TEST(test_every_weekday_and_month_fits_the_screen_in_the_shipped_languages);
  RUN_TEST(test_the_clock_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_clock_pictures_in_english_and_german);
  return UNITY_END();
}
