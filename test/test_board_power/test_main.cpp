// The Power capability on the board, on scripted chips. The contract suite for Power runs against it in
// test_hardware_contracts; this file holds what is particular to the device: the cable, the two charger
// revisions, a missing gauge, a gauge that disagrees with the voltage, and the shutdown.

#include <unity.h>

#include <BoardPowerRig.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_the_measurement_of_the_6th_of_october_reads_as_full_on_a_cable() {
  BoardPowerRig r(false);
  r.scriptMeasured();
  r.impl.begin(1500, r.writer, r.log);  // the check repairs the capacity of the gauge
  // The reading of that day is what the rule must withstand, so script it again after the repair.
  r.scriptMeasured();
  TEST_ASSERT_TRUE(r.impl.externalPower());
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Full);
  TEST_ASSERT_EQUAL(-1, r.impl.batteryPercent());
  TEST_ASSERT_EQUAL(-1, r.impl.batteryMinutesRemaining());
}

// Lesson 4: a full battery on a cable reads "not charging", and the cable is still there.
void test_the_cable_is_the_power_good_bit_whatever_the_charge_state() {
  BoardPowerRig r;
  r.scriptGauge(4150, 97, 1500, 1500);
  r.setStatus(0x04);  // power good, not charging
  TEST_ASSERT_TRUE(r.impl.externalPower());
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Full);
  r.scriptGauge(3800, 60, 1500, 1500);
  TEST_ASSERT_TRUE(r.impl.externalPower());  // a cable, charging or not
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Unknown);  // nothing can be claimed about it
  r.setStatus(0x00);
  TEST_ASSERT_FALSE(r.impl.externalPower());
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Discharging);
}

void test_on_the_battery_the_gauge_is_believed_when_the_voltage_agrees() {
  BoardPowerRig r;
  r.scriptGauge(3800, 62, 1500, 1500);
  TEST_ASSERT_EQUAL(62, r.impl.batteryPercent());
  TEST_ASSERT_EQUAL(600, r.impl.batteryMinutesRemaining());
}

void test_a_gauge_that_disagrees_with_the_cell_shows_no_percentage_and_no_time() {
  BoardPowerRig r;
  r.scriptGauge(4150, 45, 1500, 1500);
  TEST_ASSERT_EQUAL(-1, r.impl.batteryPercent());
  TEST_ASSERT_EQUAL(-1, r.impl.batteryMinutesRemaining());
}

void test_charging_is_reported_while_the_charger_charges() {
  BoardPowerRig r;
  r.scriptGauge(3900, 70, 1500, 1500);
  r.setStatus(0x14);
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Charging);
  r.setStatus(0x0C);  // pre-charge
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Charging);
  TEST_ASSERT_EQUAL(-1, r.impl.batteryMinutesRemaining());
}

void test_the_older_charger_at_6b_works_the_same() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  FakeGpio gpio(&delay);
  FakeClock clock;
  FakeModem modem(clock);
  FakeEventLog log;
  FakeGaugeWriter writer(bus);
  Xl9555 expander(bus);
  Charger charger(bus);
  FuelGauge gauge(bus);
  BoardPower power(expander, gpio, charger, gauge, modem);
  bus.addChip(i2c::kExpander);
  bus.addChip(i2c::kFuelGauge);
  bus.addChip(i2c::kChargerBq25896);
  expander.begin();
  bus.setRegister16(i2c::kFuelGauge, 0x3C, 1500);
  bus.setRegister16(i2c::kFuelGauge, 0x12, 1500);
  bus.setRegister16(i2c::kFuelGauge, 0x2C, 80);
  bus.setRegister16(i2c::kFuelGauge, 0x08, 3900);
  bus.setRegister(i2c::kChargerBq25896, 0x0B, 0x14);
  power.begin(1500, writer, log);
  TEST_ASSERT_TRUE(log.contains("BQ25896"));
  TEST_ASSERT_TRUE(power.externalPower());
  TEST_ASSERT_TRUE(power.chargeState() == ChargeState::Charging);
}

void test_no_charger_says_so_once_and_reports_unknown() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  FakeGpio gpio(&delay);
  FakeClock clock;
  FakeModem modem(clock);
  FakeEventLog log;
  FakeGaugeWriter writer(bus);
  Xl9555 expander(bus);
  Charger charger(bus);
  FuelGauge gauge(bus);
  BoardPower power(expander, gpio, charger, gauge, modem);
  bus.addChip(i2c::kExpander);
  bus.addChip(i2c::kFuelGauge);
  expander.begin();
  power.begin(1500, writer, log);
  TEST_ASSERT_TRUE(log.contains("no charger answers"));
  TEST_ASSERT_FALSE(power.externalPower());
  TEST_ASSERT_TRUE(power.chargeState() == ChargeState::Unknown);
  TEST_ASSERT_FALSE(power.powerOff());  // cannot tell whether a cable feeds the board
}

void test_a_charger_that_appears_later_is_found() {
  BoardPowerRig r(false);
  r.bus.removeChip(i2c::kChargerSy6970);
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_FALSE(r.impl.externalPower());
  r.bus.addChip(i2c::kChargerSy6970);
  r.setStatus(0x14);
  TEST_ASSERT_TRUE(r.impl.externalPower());
}

void test_a_missing_gauge_gives_unknown_battery_and_leaves_the_charger_alone() {
  BoardPowerRig r(false);
  r.bus.removeChip(i2c::kFuelGauge);
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("fuel gauge does not answer"));
  TEST_ASSERT_EQUAL(-1, r.impl.batteryPercent());
  TEST_ASSERT_EQUAL(-1, r.impl.batteryMinutesRemaining());
  r.setStatus(0x1C);
  TEST_ASSERT_TRUE(r.impl.chargeState() == ChargeState::Full);
  TEST_ASSERT_EQUAL(0, r.writer.calls());
}

void test_start_writes_the_capacity_once_and_logs_one_line_about_it() {
  BoardPowerRig r(false);
  r.scriptMeasured();
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_EQUAL(1, r.writer.calls());
  TEST_ASSERT_TRUE(r.log.contains("held design 3000 mAh, full charge 3000 mAh; written 1500 mAh"));
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_EQUAL(1, r.writer.calls());  // the second start writes nothing
}

void test_a_refused_capacity_write_is_logged_and_does_not_stop_the_start() {
  BoardPowerRig r(false);
  r.scriptMeasured();
  r.writer.refuse();
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("refused"));
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Positioning));
}

void test_start_puts_the_receiver_and_the_radio_into_their_idle_state() {
  BoardPowerRig r(false);
  TEST_ASSERT_TRUE(r.expander.level(leg::kGnssPower));  // the boot state powers them
  TEST_ASSERT_TRUE(r.expander.level(leg::kRadioPower));
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Positioning));
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Radio));
  TEST_ASSERT_EQUAL_HEX8(0x98, r.bus.reg(i2c::kExpander, xl9555::kOutput0));  // 0x9E without the two rails
}

void test_the_domains_switch_their_own_legs() {
  BoardPowerRig r;
  TEST_ASSERT_TRUE(r.impl.setEnabled(PowerDomain::Positioning, true));
  TEST_ASSERT_TRUE(r.expander.level(leg::kGnssPower));
  TEST_ASSERT_FALSE(r.expander.level(leg::kRadioPower));
  TEST_ASSERT_TRUE(r.impl.setEnabled(PowerDomain::Radio, true));
  TEST_ASSERT_TRUE(r.expander.level(leg::kRadioPower));
  TEST_ASSERT_TRUE(r.impl.setEnabled(PowerDomain::Frontlight, true));
  TEST_ASSERT_TRUE(r.gpio.level(pin::kFrontlight));
  TEST_ASSERT_TRUE(r.impl.isEnabled(PowerDomain::Frontlight));
}

void test_a_refused_expander_write_leaves_the_domain_as_it_was() {
  BoardPowerRig r;
  r.impl.setEnabled(PowerDomain::Positioning, true);
  r.bus.refuseRegisterWrite(i2c::kExpander, xl9555::kOutput0);
  TEST_ASSERT_FALSE(r.impl.setEnabled(PowerDomain::Positioning, false));
  TEST_ASSERT_TRUE(r.impl.isEnabled(PowerDomain::Positioning));
}

void test_the_cellular_domain_is_the_modem() {
  BoardPowerRig r;
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Cellular));
  TEST_ASSERT_TRUE(r.impl.setEnabled(PowerDomain::Cellular, true));
  TEST_ASSERT_TRUE(r.modem.state() != ModemState::Off);
  TEST_ASSERT_TRUE(r.impl.isEnabled(PowerDomain::Cellular));
  TEST_ASSERT_TRUE(r.impl.setEnabled(PowerDomain::Cellular, false));
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Cellular));
}

// With a cable VBUS keeps the board powered, so the battery switch would do nothing: refuse and touch nothing.
void test_power_off_is_refused_with_a_cable_and_changes_nothing() {
  BoardPowerRig r;
  r.impl.setEnabled(PowerDomain::Frontlight, true);
  r.setStatus(0x14);
  TEST_ASSERT_FALSE(r.impl.powerOff());
  TEST_ASSERT_TRUE(r.impl.isEnabled(PowerDomain::Frontlight));
  TEST_ASSERT_EQUAL(0, r.bus.writeCount(i2c::kChargerSy6970, 0x09));
}

void test_power_off_on_the_battery_cuts_the_rails_and_then_the_battery() {
  BoardPowerRig r;
  r.impl.setEnabled(PowerDomain::Frontlight, true);
  r.impl.setEnabled(PowerDomain::Positioning, true);
  r.expander.set(leg::kModemPower, true);
  TEST_ASSERT_TRUE(r.impl.powerOff());
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Frontlight));
  TEST_ASSERT_FALSE(r.impl.isEnabled(PowerDomain::Positioning));
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPower));
  TEST_ASSERT_EQUAL_HEX8(0x64, r.bus.reg(i2c::kChargerSy6970, 0x09));
}

void test_a_charger_that_does_not_answer_the_status_read_blocks_power_off() {
  BoardPowerRig r;
  r.impl.setEnabled(PowerDomain::Frontlight, true);
  r.bus.removeChip(i2c::kChargerSy6970);
  TEST_ASSERT_FALSE(r.impl.powerOff());
  TEST_ASSERT_TRUE(r.impl.isEnabled(PowerDomain::Frontlight));
}

void test_a_refused_battery_switch_is_reported() {
  BoardPowerRig r;
  r.bus.refuseRegisterWrite(i2c::kChargerSy6970, 0x09);
  TEST_ASSERT_FALSE(r.impl.powerOff());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_measurement_of_the_6th_of_october_reads_as_full_on_a_cable);
  RUN_TEST(test_the_cable_is_the_power_good_bit_whatever_the_charge_state);
  RUN_TEST(test_on_the_battery_the_gauge_is_believed_when_the_voltage_agrees);
  RUN_TEST(test_a_gauge_that_disagrees_with_the_cell_shows_no_percentage_and_no_time);
  RUN_TEST(test_charging_is_reported_while_the_charger_charges);
  RUN_TEST(test_the_older_charger_at_6b_works_the_same);
  RUN_TEST(test_no_charger_says_so_once_and_reports_unknown);
  RUN_TEST(test_a_charger_that_appears_later_is_found);
  RUN_TEST(test_a_missing_gauge_gives_unknown_battery_and_leaves_the_charger_alone);
  RUN_TEST(test_start_writes_the_capacity_once_and_logs_one_line_about_it);
  RUN_TEST(test_a_refused_capacity_write_is_logged_and_does_not_stop_the_start);
  RUN_TEST(test_start_puts_the_receiver_and_the_radio_into_their_idle_state);
  RUN_TEST(test_the_domains_switch_their_own_legs);
  RUN_TEST(test_a_refused_expander_write_leaves_the_domain_as_it_was);
  RUN_TEST(test_the_cellular_domain_is_the_modem);
  RUN_TEST(test_power_off_is_refused_with_a_cable_and_changes_nothing);
  RUN_TEST(test_power_off_on_the_battery_cuts_the_rails_and_then_the_battery);
  RUN_TEST(test_a_charger_that_does_not_answer_the_status_read_blocks_power_off);
  RUN_TEST(test_a_refused_battery_switch_is_reported);
  return UNITY_END();
}
