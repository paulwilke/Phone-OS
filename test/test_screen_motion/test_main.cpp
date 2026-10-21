// Host tests for the motion page: starting only after the first frame, the sensor running only while the page
// is open, the mapping of the sensor's axes to the ball (pinned value by value), shakes counted at every
// sample, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <cmath>

#include <DeviceRig.h>
#include <MotionSourceOf.h>
#include <Shows.h>
#include <fakes/FakeMotion.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/MotionScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  FakeMotion sensor;
  MotionSourceOf source{sensor};
  MotionScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context(), source) {}
  // Opens the page the way the stack does: entered, drawn, and the next tick starts the sensor.
  void open() {
    screen.onEnter();
    draw();
    screen.onTick();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void tilt(float x, float y, float z = 1.0f) {
    sensor.tilt(x, y, z);
    screen.onTick();
    draw();
  }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  bool says(const std::string& words, FontSize size) { return shows::text(rig.frame, words, size, kContent); }
  bool saysWrapped(const std::string& words) {
    const auto lines = text::wrap(font(FontSize::Text), words, kContent.w - 12);
    for (std::string_view line : lines)
      if (!says(std::string(line), FontSize::Text)) return false;
    return !lines.empty();
  }
};

// The ink of one picture that the other lacks, as a centre (the ball that moved).
struct Centre {
  double x = 0, y = 0;
  int n = 0;
};
Centre onlyIn(const FrameBuffer& a, const FrameBuffer& b) {
  Centre c;
  for (int y = kContent.y; y < kContent.bottom(); ++y)
    for (int x = kContent.x; x < kContent.right(); ++x)
      if (a.pixel(x, y) && !b.pixel(x, y)) {
        c.x += x;
        c.y += y;
        ++c.n;
      }
  if (c.n) {
    c.x /= c.n;
    c.y /= c.n;
  }
  return c;
}

}  // namespace

// ---- starting and stopping --------------------------------------------------------------------------------

void test_without_a_sensor_the_page_says_so_and_starts_nothing() {
  Page page;
  page.sensor.setAvailable(false);
  page.open();
  TEST_ASSERT_TRUE(page.saysWrapped("There is no motion sensor in this device."));
  TEST_ASSERT_EQUAL(0, page.source.starts);
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
}

void test_the_sensor_is_started_only_after_the_frame_that_says_so_has_been_drawn() {
  Page page;
  page.screen.onEnter();
  page.screen.onTick();                                                 // nothing drawn yet
  TEST_ASSERT_EQUAL(0, page.source.starts);
  page.draw();
  TEST_ASSERT_TRUE(page.saysWrapped("Sensor is starting ... (takes about 10 seconds)"));
  TEST_ASSERT_EQUAL(0, page.source.starts);
  page.screen.onTick();                                                 // the frame is out: now the blocking start
  TEST_ASSERT_EQUAL(1, page.source.starts);
  TEST_ASSERT_TRUE(page.sensor.running());
  page.screen.onTick();
  TEST_ASSERT_EQUAL(1, page.source.starts);                             // once
}

void test_a_sensor_that_does_not_answer_is_told_and_not_asked_again() {
  Page page;
  page.sensor.failNextStart();
  page.open();
  TEST_ASSERT_TRUE(page.saysWrapped("The sensor does not answer."));
  TEST_ASSERT_FALSE(page.sensor.running());
  page.screen.onTick();
  page.screen.onTick();
  TEST_ASSERT_EQUAL(1, page.source.starts);
}

void test_until_the_first_value_arrives_the_page_still_says_it_is_starting() {
  Page page;
  page.sensor.clearSample();
  page.open();
  TEST_ASSERT_TRUE(page.sensor.running());
  TEST_ASSERT_TRUE(page.saysWrapped("Sensor is starting ... (takes about 10 seconds)"));
  page.tilt(0, 0);
  TEST_ASSERT_FALSE(page.saysWrapped("Sensor is starting ... (takes about 10 seconds)"));
}

void test_leaving_stops_the_sensor_and_entering_again_starts_it_afresh() {
  Page page;
  page.open();
  TEST_ASSERT_TRUE(page.sensor.running());
  page.screen.onLeave();
  TEST_ASSERT_FALSE(page.sensor.running());
  TEST_ASSERT_EQUAL(1, page.source.stops);
  page.screen.onLeave();                                                // a second leave stops nothing more
  TEST_ASSERT_EQUAL(1, page.source.stops);
  page.open();
  TEST_ASSERT_EQUAL(2, page.source.starts);
  TEST_ASSERT_TRUE(page.sensor.running());
}

void test_every_way_out_of_the_page_stops_the_sensor_through_the_stack() {
  DeviceRig rig;
  FakeMotion sensor;
  MotionSourceOf source(sensor);
  ScreenStack stack(screen::kHome);
  MotionScreen motion(rig.context(stack), source);
  struct Blank : Screen {
    bool handle(const Input&) override { return false; }
    void render(Display&) override {}
  } home, lock;
  stack.add(screen::kHome, home);
  stack.add(screen::kMotion, motion);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.begin(0);
  FrameBuffer fb;
  for (int how = 0; how < 3; ++how) {
    stack.open(screen::kMotion);
    stack.render(fb);                                                   // the page draws "starting"
    stack.poll(1);                                                      // the tick starts the sensor
    TEST_ASSERT_TRUE_MESSAGE(sensor.running(), std::to_string(how).c_str());
    if (how == 0) stack.back();
    if (how == 1) stack.home();
    if (how == 2) stack.lock();
    TEST_ASSERT_FALSE_MESSAGE(sensor.running(), std::to_string(how).c_str());
    stack.unlock();
  }
}

void test_back_leaves_and_the_stack_stops_the_sensor() {
  Page page;
  page.open();
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

// ---- the mapping --------------------------------------------------------------------------------------------

void test_flat_on_a_table_the_ball_sits_in_the_middle() {
  Page page;
  page.open();
  page.tilt(0, 0, 1.0f);
  TEST_ASSERT_EQUAL(0, page.screen.ballX());
  TEST_ASSERT_EQUAL(0, page.screen.ballY());
}

// The sensor sits turned 90 degrees against the screen and its values are used as the device reports them:
// the ball rolls to the lower side. These are the values the first device showed to be right on 2026-10-03;
// the first mapping rolled the ball up when the device was tilted left.
void test_the_ball_rolls_to_the_lower_side_with_the_sensors_axes_as_the_device_reports_them() {
  Page page;
  page.open();
  page.tilt(0.0f, 0.45f);                                               // the left side low
  TEST_ASSERT_EQUAL(-3, page.screen.ballX());
  TEST_ASSERT_EQUAL(0, page.screen.ballY());
  page.tilt(0.0f, -0.45f);                                              // the right side low
  TEST_ASSERT_EQUAL(3, page.screen.ballX());
  TEST_ASSERT_EQUAL(0, page.screen.ballY());
  page.tilt(0.30f, 0.0f);                                               // the lower edge low: tilted towards the user
  TEST_ASSERT_EQUAL(0, page.screen.ballX());
  TEST_ASSERT_EQUAL(-2, page.screen.ballY());
  page.tilt(-0.30f, 0.0f);                                              // the upper edge low
  TEST_ASSERT_EQUAL(0, page.screen.ballX());
  TEST_ASSERT_EQUAL(2, page.screen.ballY());
}

void test_the_fields_are_fifteen_hundredths_of_a_g_and_stop_at_three() {
  TEST_ASSERT_EQUAL(0, MotionScreen::cell(0.07f));
  TEST_ASSERT_EQUAL(1, MotionScreen::cell(0.08f));
  TEST_ASSERT_EQUAL(-1, MotionScreen::cell(-0.08f));
  TEST_ASSERT_EQUAL(2, MotionScreen::cell(0.30f));
  TEST_ASSERT_EQUAL(3, MotionScreen::cell(0.45f));
  TEST_ASSERT_EQUAL(3, MotionScreen::cell(5.0f));
  TEST_ASSERT_EQUAL(-3, MotionScreen::cell(-5.0f));
}

void test_the_drawn_ball_moves_the_way_the_mapping_says() {
  Page page;
  page.open();
  page.tilt(0, 0);
  FrameBuffer flat = page.rig.frame;
  page.tilt(0.0f, 0.45f);                                               // left side low -> the ball goes left
  Centre moved = onlyIn(page.rig.frame, flat);
  Centre left = onlyIn(flat, page.rig.frame);
  TEST_ASSERT_TRUE(moved.n > 0 && left.n > 0);
  TEST_ASSERT_TRUE(moved.x < left.x - 20);
  TEST_ASSERT_TRUE(std::fabs(moved.y - left.y) < 8);
  page.tilt(0.30f, 0.0f);                                               // lower edge low -> the ball goes down
  moved = onlyIn(page.rig.frame, flat);
  left = onlyIn(flat, page.rig.frame);
  TEST_ASSERT_TRUE(moved.y > left.y + 15);                              // pictures count y downwards
  TEST_ASSERT_TRUE(std::fabs(moved.x - left.x) < 8);
}

void test_a_ball_that_stays_in_its_field_does_not_change_the_picture() {
  Page page;
  page.open();
  page.tilt(0.01f, 0.02f);
  const std::string before = page.rig.frame.toAscii(kContent);
  page.tilt(-0.02f, 0.03f);                                             // a tremble inside the middle field
  TEST_ASSERT_TRUE(before == page.rig.frame.toAscii(kContent));
}

// ---- shakes and the strength --------------------------------------------------------------------------------

void test_shakes_are_counted_and_enter_sets_the_counter_to_zero() {
  Page page;
  page.open();
  page.tilt(0, 0);
  TEST_ASSERT_TRUE(page.says("Shaken: 0", FontSize::Text));
  page.sensor.shake();
  page.screen.onTick();
  page.screen.onTick();
  page.screen.onTick();                                                 // the burst is over: rest again
  page.sensor.shake();
  page.screen.onTick();
  page.screen.onTick();
  page.screen.onTick();
  TEST_ASSERT_EQUAL(2, page.screen.shakes());
  page.draw();
  TEST_ASSERT_TRUE(page.says("Shaken: 2", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("Reset", page.screen.softKeys().action.c_str());
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(0, page.screen.shakes());
  TEST_ASSERT_TRUE(page.says("Shaken: 0", FontSize::Text));
}

void test_every_sample_counts_however_seldom_the_picture_is_drawn() {
  Page page;
  page.open();
  page.tilt(0, 0);
  for (int burst = 0; burst < 4; ++burst) {
    page.sensor.shake();
    for (int i = 0; i < 3; ++i) page.screen.onTick();                   // no render in between
  }
  TEST_ASSERT_EQUAL(4, page.screen.shakes());
}

void test_a_slow_tilt_is_no_shake() {
  Page page;
  page.open();
  for (float t = 0; t <= 1.0f; t += 0.1f) page.tilt(t, 0.0f, std::sqrt(1.0f - t * t));
  TEST_ASSERT_EQUAL(0, page.screen.shakes());
}

void test_the_strength_is_told_in_g_with_one_decimal() {
  Page page;
  page.open();
  page.tilt(0.3f, 0.4f, 0.0f);
  TEST_ASSERT_TRUE(page.says("0.5 g", FontSize::Small));
  page.tilt(0, 0, 1.0f);
  TEST_ASSERT_TRUE(page.says("1.0 g", FontSize::Small));
}

void test_enter_does_nothing_before_there_is_a_value() {
  Page page;
  page.sensor.clearSample();
  page.open();
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
}

void test_the_words_follow_the_language() {
  Page page;
  page.open();
  page.tilt(0, 0);
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.says("Gesch\xC3\xBC" "ttelt: 0", FontSize::Text));
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Bewegung", FontSize::Small, kContent));
}

// ---- layout --------------------------------------------------------------------------------------------------------

void test_every_phase_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int phase = 0; phase < 4; ++phase) {
      Page page;
      page.rig.language(language);
      if (phase == 0) { page.sensor.setAvailable(false); page.open(); }
      if (phase == 1) { page.screen.onEnter(); page.draw(); }                    // starting
      if (phase == 2) { page.sensor.failNextStart(); page.open(); }
      if (phase == 3) { page.open(); page.tilt(0.45f, -0.45f); }                 // measuring, the ball in a corner
      FrameBuffer fb;
      page.screen.render(fb);
      // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
      // into the soft-key bar there (see docs/ui/screen-motion.md), the page itself may not.
      bool outside = false;
      for (const auto& e : fb.clipEvents())
        if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
      TEST_ASSERT_FALSE_MESSAGE(outside, (language + " phase " + std::to_string(phase) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page starting;
    starting.rig.language(language);
    starting.screen.onEnter();
    starting.draw();
    golden::expect(starting.rig.frame, std::string("motion_starting_") + language);
    Page level;
    level.rig.language(language);
    level.open();
    level.tilt(0.0f, 0.30f);
    golden::expect(level.rig.frame, std::string("motion_level_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_without_a_sensor_the_page_says_so_and_starts_nothing);
  RUN_TEST(test_the_sensor_is_started_only_after_the_frame_that_says_so_has_been_drawn);
  RUN_TEST(test_a_sensor_that_does_not_answer_is_told_and_not_asked_again);
  RUN_TEST(test_until_the_first_value_arrives_the_page_still_says_it_is_starting);
  RUN_TEST(test_leaving_stops_the_sensor_and_entering_again_starts_it_afresh);
  RUN_TEST(test_every_way_out_of_the_page_stops_the_sensor_through_the_stack);
  RUN_TEST(test_back_leaves_and_the_stack_stops_the_sensor);
  RUN_TEST(test_flat_on_a_table_the_ball_sits_in_the_middle);
  RUN_TEST(test_the_ball_rolls_to_the_lower_side_with_the_sensors_axes_as_the_device_reports_them);
  RUN_TEST(test_the_fields_are_fifteen_hundredths_of_a_g_and_stop_at_three);
  RUN_TEST(test_the_drawn_ball_moves_the_way_the_mapping_says);
  RUN_TEST(test_a_ball_that_stays_in_its_field_does_not_change_the_picture);
  RUN_TEST(test_shakes_are_counted_and_enter_sets_the_counter_to_zero);
  RUN_TEST(test_every_sample_counts_however_seldom_the_picture_is_drawn);
  RUN_TEST(test_a_slow_tilt_is_no_shake);
  RUN_TEST(test_the_strength_is_told_in_g_with_one_decimal);
  RUN_TEST(test_enter_does_nothing_before_there_is_a_value);
  RUN_TEST(test_the_words_follow_the_language);
  RUN_TEST(test_every_phase_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}
