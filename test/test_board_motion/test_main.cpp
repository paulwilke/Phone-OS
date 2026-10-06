// The motion sensor on the board: the rail, the start that blocks, and above all that the samples are handed
// on exactly as the sensor reports them. Lying flat the device reads about -0.95 on z, and the motion page's
// mapping was checked with these values.

#include <unity.h>

#include <BoardInputRigs.h>
#include <boot/SensorMotionSource.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_start_puts_the_rail_into_its_idle_state() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  Xl9555 expander(bus);
  bus.addChip(i2c::kExpander);
  expander.begin();
  TEST_ASSERT_TRUE(expander.level(leg::kMotionRail));  // the boot state powers it
  FakeAccelerometerHub hub;
  BoardMotion motion(expander, hub, delay);
  motion.begin();
  TEST_ASSERT_FALSE(expander.level(leg::kMotionRail));
}

void test_the_rail_is_on_before_the_hub_is_started_and_off_after_stop() {
  BoardMotionRig r;
  TEST_ASSERT_TRUE(r.impl.start());
  TEST_ASSERT_TRUE(r.hub.railWasOnAtStart());
  TEST_ASSERT_TRUE(r.expander.level(leg::kMotionRail));
  r.impl.stop();
  TEST_ASSERT_FALSE(r.expander.level(leg::kMotionRail));
  TEST_ASSERT_EQUAL(1, r.hub.stops());
}

void test_the_rail_gets_time_to_settle_before_the_chip_is_asked() {
  BoardMotionRig r;
  r.impl.start();
  TEST_ASSERT_TRUE(r.delay.elapsedMs() >= kMotionRailSettleMs);
}

void test_a_chip_that_does_not_answer_leaves_the_rail_off() {
  BoardMotionRig r;
  r.hub.failNextStart();
  TEST_ASSERT_FALSE(r.impl.start());
  TEST_ASSERT_FALSE(r.expander.level(leg::kMotionRail));
  TEST_ASSERT_FALSE(r.impl.running());
}

void test_a_start_after_a_failed_one_works() {
  BoardMotionRig r;
  r.hub.failNextStart();
  r.impl.start();
  TEST_ASSERT_TRUE(r.impl.start());
  TEST_ASSERT_TRUE(r.expander.level(leg::kMotionRail));
}

void test_a_start_while_running_does_not_start_the_hub_again() {
  BoardMotionRig r;
  r.impl.start();
  r.impl.start();
  TEST_ASSERT_EQUAL(1, r.hub.starts());
}

void test_without_an_expander_the_sensor_is_not_available_and_not_started() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  Xl9555 expander(bus);
  expander.begin();  // nobody answers
  FakeAccelerometerHub hub;
  BoardMotion motion(expander, hub, delay);
  TEST_ASSERT_FALSE(motion.available());
  TEST_ASSERT_FALSE(motion.start());
  TEST_ASSERT_EQUAL(0, hub.starts());
}

void test_a_rail_that_cannot_be_switched_does_not_start_the_hub() {
  BoardMotionRig r;
  r.bus.refuseRegisterWrite(i2c::kExpander, xl9555::kOutput0);
  TEST_ASSERT_FALSE(r.impl.start());
  TEST_ASSERT_EQUAL(0, r.hub.starts());
}

// Lying flat the sensor reads -0.95 on z. The sign is not the board class's to change.
void test_the_sample_is_handed_on_as_the_sensor_reports_it() {
  BoardMotionRig r;
  r.impl.start();
  r.hub.tilt(0.02f, -0.03f, -0.95f);
  MotionSample s;
  TEST_ASSERT_TRUE(r.impl.read(s));
  TEST_ASSERT_TRUE(s.valid);
  TEST_ASSERT_EQUAL_FLOAT(0.02f, s.ax);
  TEST_ASSERT_EQUAL_FLOAT(-0.03f, s.ay);
  TEST_ASSERT_EQUAL_FLOAT(-0.95f, s.az);
}

void test_a_device_flat_on_the_table_reads_minus_095_on_z_by_default() {
  BoardMotionRig r;
  r.impl.start();
  MotionSample s;
  TEST_ASSERT_TRUE(r.impl.read(s));
  TEST_ASSERT_EQUAL_FLOAT(-0.95f, s.az);
}

void test_no_value_yet_leaves_the_sample_alone() {
  BoardMotionRig r;
  r.impl.start();
  r.hub.clearSample();
  MotionSample s;
  s.ax = 7;
  TEST_ASSERT_FALSE(r.impl.read(s));
  TEST_ASSERT_EQUAL_FLOAT(7, s.ax);
  TEST_ASSERT_FALSE(s.valid);
}

// Through the interface's own source: the values the motion page was checked with arrive unchanged.
void test_the_values_the_motion_page_was_checked_with_arrive_at_the_page_unchanged() {
  BoardMotionRig r;
  boot::SensorMotionSource source(&r.impl);
  TEST_ASSERT_TRUE(source.start());
  r.hub.tilt(0.30f, 0.0f, -0.95f);  // the lower edge low, from the table in docs/screen-motion.md
  ui::Acceleration a;
  TEST_ASSERT_TRUE(source.read(a));
  TEST_ASSERT_EQUAL_FLOAT(0.30f, a.x);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, a.y);
  TEST_ASSERT_EQUAL_FLOAT(-0.95f, a.z);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_start_puts_the_rail_into_its_idle_state);
  RUN_TEST(test_the_rail_is_on_before_the_hub_is_started_and_off_after_stop);
  RUN_TEST(test_the_rail_gets_time_to_settle_before_the_chip_is_asked);
  RUN_TEST(test_a_chip_that_does_not_answer_leaves_the_rail_off);
  RUN_TEST(test_a_start_after_a_failed_one_works);
  RUN_TEST(test_a_start_while_running_does_not_start_the_hub_again);
  RUN_TEST(test_without_an_expander_the_sensor_is_not_available_and_not_started);
  RUN_TEST(test_a_rail_that_cannot_be_switched_does_not_start_the_hub);
  RUN_TEST(test_the_sample_is_handed_on_as_the_sensor_reports_it);
  RUN_TEST(test_a_device_flat_on_the_table_reads_minus_095_on_z_by_default);
  RUN_TEST(test_no_value_yet_leaves_the_sample_alone);
  RUN_TEST(test_the_values_the_motion_page_was_checked_with_arrive_at_the_page_unchanged);
  return UNITY_END();
}
