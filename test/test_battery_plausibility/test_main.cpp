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

void test_the_boundaries() {
  TEST_ASSERT_FALSE(assessBattery(on(70, 4200, false)).contradicted);  // 70 % is not below 70
  TEST_ASSERT_TRUE(assessBattery(on(69, 4200, false)).contradicted);
  TEST_ASSERT_FALSE(assessBattery(on(50, 4100, false)).contradicted);  // 4.1 V is not above 4.1 V
  TEST_ASSERT_TRUE(assessBattery(on(50, 4101, false)).contradicted);
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
  return UNITY_END();
}
