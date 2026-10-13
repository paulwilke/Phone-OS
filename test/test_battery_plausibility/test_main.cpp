// The rule that keeps the interface from showing a percentage the voltage contradicts.

#include <unity.h>

#include <board/BatteryPlausibility.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
BatteryInputs on(int percent, int mv, bool cable, bool done = false, bool notCharging = false) {
  BatteryInputs in;
  in.gaugePercent = percent;
  in.gaugeMillivolts = mv;
  in.chargerKnown = true;
  in.cable = cable;
  in.chargeDone = done;
  in.notCharging = notCharging;
  return in;
}
}  // namespace

// 2026-10-06: gauge 56 %, 4118 mV, charger "done", on a cable.
void test_the_measurement_of_the_6th_of_october_shows_full_and_no_number() {
  const BatteryAssessment a = assessBattery(on(56, 4118, true, true));
  TEST_ASSERT_TRUE(a.contradicted);
  TEST_ASSERT_EQUAL(-1, a.percent);
  TEST_ASSERT_TRUE(a.full);
}

void test_a_gauge_that_agrees_is_shown() {
  const BatteryAssessment a = assessBattery(on(97, 4130, true, true));
  TEST_ASSERT_FALSE(a.contradicted);
  TEST_ASSERT_EQUAL(97, a.percent);
  TEST_ASSERT_TRUE(a.full);
}

void test_on_the_battery_in_the_middle_everything_is_believed() {
  const BatteryAssessment a = assessBattery(on(50, 3750, false));
  TEST_ASSERT_FALSE(a.contradicted);
  TEST_ASSERT_EQUAL(50, a.percent);
  TEST_ASSERT_FALSE(a.full);
}

// A high cell with a low number is wrong with or without a cable, but "full" is only the charger's word.
void test_a_high_cell_on_the_battery_withholds_the_number_and_claims_nothing() {
  const BatteryAssessment a = assessBattery(on(40, 4150, false));
  TEST_ASSERT_TRUE(a.contradicted);
  TEST_ASSERT_EQUAL(-1, a.percent);
  TEST_ASSERT_FALSE(a.full);
}

void test_still_charging_a_high_cell_is_not_called_full() {
  const BatteryAssessment a = assessBattery(on(60, 4120, true));
  TEST_ASSERT_TRUE(a.contradicted);
  TEST_ASSERT_EQUAL(-1, a.percent);
  TEST_ASSERT_FALSE(a.full);
}

// A full battery on a cable reads "not charging".
void test_not_charging_on_a_cable_with_a_high_cell_is_full() {
  TEST_ASSERT_TRUE(assessBattery(on(98, 4150, true, false, true)).full);
  TEST_ASSERT_FALSE(assessBattery(on(60, 3900, true, false, true)).full);
}

// The first rule's edges, on a cable: the voltage rule for the battery would doubt these numbers on its own.
void test_the_boundaries() {
  TEST_ASSERT_FALSE(assessBattery(on(70, 4200, true)).contradicted);  // 70 % is not below 70
  TEST_ASSERT_TRUE(assessBattery(on(69, 4200, true)).contradicted);
  TEST_ASSERT_FALSE(assessBattery(on(50, 4100, true)).contradicted);  // 4.1 V is not above 4.1 V
  TEST_ASSERT_TRUE(assessBattery(on(50, 4101, true)).contradicted);
  TEST_ASSERT_TRUE(assessBattery(on(0, 4101, false)).contradicted);
  TEST_ASSERT_FALSE(assessBattery(on(100, 3300, false)).contradicted);  // high number, low cell: not this rule's business
}

void test_the_chargers_voltage_stands_in_when_the_gauge_gives_none() {
  BatteryInputs in = on(40, 0, false);
  in.chargerMillivolts = 4150;
  TEST_ASSERT_TRUE(assessBattery(in).contradicted);
}

void test_an_unknown_gauge_is_unknown() {
  BatteryInputs in = on(-1, 0, true, true);
  const BatteryAssessment a = assessBattery(in);
  TEST_ASSERT_FALSE(a.contradicted);
  TEST_ASSERT_EQUAL(-1, a.percent);
  TEST_ASSERT_TRUE(a.full);
}

void test_a_charger_that_did_not_answer_claims_nothing() {
  BatteryInputs in = on(56, 3800, true, true);
  in.chargerKnown = false;
  const BatteryAssessment a = assessBattery(in);
  TEST_ASSERT_FALSE(a.full);
  TEST_ASSERT_FALSE(a.contradicted);
  TEST_ASSERT_EQUAL(56, a.percent);
}

// 2026-10-09, on the battery at 140 mA: the gauge went from 55 % (820 mAh) to 6 % (79 mAh) in one step at 3627 mV
// and then said 0 % for three hours while the device ran on to 3426 mV.
void test_a_gauge_that_fell_to_almost_nothing_at_3627_mv_is_not_believed() {
  const BatteryAssessment a = assessBattery(on(6, 3627, false));
  TEST_ASSERT_TRUE(a.contradicted);
  TEST_ASSERT_TRUE(a.estimated);
  TEST_ASSERT_EQUAL(percentFromVoltage(3627), a.percent);
  TEST_ASSERT_TRUE(a.percent >= 40 && a.percent <= 50);
}

void test_a_gauge_that_says_zero_at_3601_mv_is_not_believed_and_the_number_is_the_voltages() {
  const BatteryAssessment a = assessBattery(on(0, 3601, false));
  TEST_ASSERT_TRUE(a.estimated);
  TEST_ASSERT_TRUE(a.percent > 30 && a.percent < 45);
}

void test_a_gauge_that_agrees_with_a_low_voltage_is_believed() {
  const BatteryAssessment a = assessBattery(on(8, 3500, false));
  TEST_ASSERT_FALSE(a.contradicted);
  TEST_ASSERT_FALSE(a.estimated);
  TEST_ASSERT_EQUAL(8, a.percent);
}

void test_a_flat_cell_is_zero_by_the_gauge_and_by_the_voltage() {
  const BatteryAssessment a = assessBattery(on(0, 3300, false));
  TEST_ASSERT_FALSE(a.estimated);
  TEST_ASSERT_EQUAL(0, a.percent);
}

// A raised voltage on a cable is the charger's, not the cell's: the second rule is for the battery only.
void test_the_voltage_rule_is_not_applied_on_a_cable_or_with_an_unknown_charger() {
  TEST_ASSERT_FALSE(assessBattery(on(6, 3900, true)).estimated);
  BatteryInputs in = on(6, 3900, false);
  in.chargerKnown = false;
  TEST_ASSERT_FALSE(assessBattery(in).estimated);
}

// The edge of the second rule: 30 points below the voltage's number is doubted, 29 is not.
void test_the_second_rule_has_its_edge_at_30_points() {
  const int fromVoltage = percentFromVoltage(3749);  // 63
  TEST_ASSERT_TRUE(assessBattery(on(fromVoltage - 30, 3749, false)).estimated);
  TEST_ASSERT_FALSE(assessBattery(on(fromVoltage - 29, 3749, false)).estimated);
}

// A dip while the modem sends is a low voltage with a good gauge: only a gauge that says too little is doubted.
void test_a_gauge_that_says_more_than_the_voltage_gives_is_left_alone() {
  const BatteryAssessment a = assessBattery(on(70, 3500, false));
  TEST_ASSERT_FALSE(a.contradicted);
  TEST_ASSERT_EQUAL(70, a.percent);
}

void test_the_curve_is_the_one_measured_and_never_leaves_0_to_100() {
  TEST_ASSERT_EQUAL(100, percentFromVoltage(4200));
  TEST_ASSERT_EQUAL(100, percentFromVoltage(4100));
  TEST_ASSERT_EQUAL(49, percentFromVoltage(3644));
  TEST_ASSERT_EQUAL(3, percentFromVoltage(3430));  // the last measured point; below it a line to the switch-off voltage
  TEST_ASSERT_EQUAL(0, percentFromVoltage(3300));
  TEST_ASSERT_EQUAL(0, percentFromVoltage(3000));
  int last = 0;
  for (int mv = 3400; mv <= 4200; ++mv) {
    const int p = percentFromVoltage(mv);
    TEST_ASSERT_TRUE_MESSAGE(p >= last && p <= 100, "the curve rises with the voltage");
    last = p;
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_measurement_of_the_6th_of_october_shows_full_and_no_number);
  RUN_TEST(test_a_gauge_that_agrees_is_shown);
  RUN_TEST(test_on_the_battery_in_the_middle_everything_is_believed);
  RUN_TEST(test_a_high_cell_on_the_battery_withholds_the_number_and_claims_nothing);
  RUN_TEST(test_still_charging_a_high_cell_is_not_called_full);
  RUN_TEST(test_not_charging_on_a_cable_with_a_high_cell_is_full);
  RUN_TEST(test_the_boundaries);
  RUN_TEST(test_the_chargers_voltage_stands_in_when_the_gauge_gives_none);
  RUN_TEST(test_an_unknown_gauge_is_unknown);
  RUN_TEST(test_a_charger_that_did_not_answer_claims_nothing);
  RUN_TEST(test_a_gauge_that_fell_to_almost_nothing_at_3627_mv_is_not_believed);
  RUN_TEST(test_a_gauge_that_says_zero_at_3601_mv_is_not_believed_and_the_number_is_the_voltages);
  RUN_TEST(test_a_gauge_that_agrees_with_a_low_voltage_is_believed);
  RUN_TEST(test_a_flat_cell_is_zero_by_the_gauge_and_by_the_voltage);
  RUN_TEST(test_the_voltage_rule_is_not_applied_on_a_cable_or_with_an_unknown_charger);
  RUN_TEST(test_the_second_rule_has_its_edge_at_30_points);
  RUN_TEST(test_a_gauge_that_says_more_than_the_voltage_gives_is_left_alone);
  RUN_TEST(test_the_curve_is_the_one_measured_and_never_leaves_0_to_100);
  return UNITY_END();
}
