// The Power capability on the board, on scripted chips. The contract suite for Power runs against it in
// test_hardware_contracts; this file holds what is particular to the device: the cable, the two charger
// revisions, a missing gauge, a gauge that disagrees with the voltage, and the shutdown.

#include <unity.h>

#include <BoardPowerRig.h>
#include <fakes/FakeMillis.h>

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

void test_the_gauge_that_said_zero_at_3601_mv_gives_the_voltages_number_and_says_so_once() {
  BoardPowerRig r;
  r.scriptGauge(3601, 0, 1500, 1500);
  const int percent = r.impl.batteryPercent();
  TEST_ASSERT_TRUE(percent > 30 && percent < 45);
  TEST_ASSERT_EQUAL(-1, r.impl.batteryMinutesRemaining());
  r.impl.batteryPercent();
  int lines = 0;
  for (const auto& l : r.log.lines())
    if (l.find("showing about") != std::string::npos) ++lines;
  TEST_ASSERT_EQUAL(1, lines);
  r.scriptGauge(3700, 60, 1500, 1500);
  r.impl.batteryPercent();
  TEST_ASSERT_TRUE(r.log.lines().back().find("agrees with the voltage again") != std::string::npos);
}

// 2026-10-13: the log held a line about every second for minutes, and the screen flapped between 0 % and 30 %.
void test_a_voltage_on_the_border_leaves_one_log_line_and_one_number() {
  BoardPowerRig r;
  int first = -2;
  for (int mv : {3582, 3578, 3580, 3576, 3579, 3581, 3577, 3575, 3574}) {
    r.scriptGauge(mv, 0, 1500, 1500);
    const int percent = r.impl.batteryPercent();
    if (first == -2) first = percent;
    TEST_ASSERT_TRUE_MESSAGE(percent > 20, std::to_string(mv).c_str());   // the voltage's number each time, never the gauge's 0
  }
  int lines = 0;
  for (const auto& l : r.log.lines())
    if (l.find("Battery: the gauge") != std::string::npos) ++lines;
  TEST_ASSERT_EQUAL(1, lines);
  TEST_ASSERT_TRUE(first > 20);
}

// The memory of the last answer does not depend on there being a log.
void test_the_rule_remembers_its_answer_without_a_log() {
  BoardPowerRig r(false);
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, r.log);
  r.scriptGauge(3582, 0, 1500, 1500);
  const int a = r.impl.batteryPercent();
  r.scriptGauge(3578, 0, 1500, 1500);
  TEST_ASSERT_TRUE(r.impl.batteryPercent() > 20);
  TEST_ASSERT_TRUE(a > 20);
}

void test_the_cell_voltage_is_the_gauges_else_the_chargers_else_unknown() {
  BoardPowerRig r;
  r.scriptGauge(3754, 63, 1500, 1500);
  TEST_ASSERT_EQUAL(3754, r.impl.batteryMillivolts());
  r.scriptGauge(0, 63, 1500, 1500);
  TEST_ASSERT_EQUAL(4164, r.impl.batteryMillivolts());  // the charger's 0x0E = 0x5D, as measured on 2026-10-06
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

void test_the_start_limits_the_charge_current_once_and_says_so() {
  BoardPowerRig r(false);  // the chip as it was read on 2026-10-12: 2048 mA, watchdog running
  r.bus.setRegister(board::i2c::kChargerSy6970, 0x07, 0x9D);
  r.bus.setRegister(board::i2c::kChargerSy6970, 0x04, 0x20);
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("charge current limited to 960 mA (it was 2048 mA)"));
  TEST_ASSERT_EQUAL_HEX8(0x0F, r.bus.reg(board::i2c::kChargerSy6970, 0x04));
  TEST_ASSERT_EQUAL_HEX8(0x8D, r.bus.reg(board::i2c::kChargerSy6970, 0x07));
  FakeEventLog second;
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, second);
  TEST_ASSERT_TRUE(second.contains("charge current is 960 mA already"));
  TEST_ASSERT_EQUAL(1, r.bus.writeCount(board::i2c::kChargerSy6970, 0x04));  // written once, not at every start
}

void test_a_charger_that_refuses_the_limit_is_logged_and_the_start_goes_on() {
  BoardPowerRig r(false);
  r.bus.refuseRegisterWrite(board::i2c::kChargerSy6970, 0x04);
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("charge current could not be limited"));
  TEST_ASSERT_TRUE(r.log.contains("fuel gauge capacity"));  // the rest of the start ran
}

void test_the_start_sets_the_input_limit_to_800_ma_once_and_says_so() {
  BoardPowerRig r(false);  // register 0x00 as read on the device: 0x48, 500 mA
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("input current limit set to 800 mA (it was 500 mA)"));
  TEST_ASSERT_EQUAL_HEX8(0x4E, r.bus.reg(board::i2c::kChargerSy6970, 0x00));
  FakeEventLog second;
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, second);
  TEST_ASSERT_TRUE(second.contains("input current limit is 800 mA already, nothing to write"));
  TEST_ASSERT_EQUAL(1, r.bus.writeCount(board::i2c::kChargerSy6970, 0x00));  // written once, not at every start
}

void test_a_charger_that_refuses_the_input_limit_is_logged_and_the_start_goes_on() {
  BoardPowerRig r(false);
  r.bus.refuseRegisterWrite(board::i2c::kChargerSy6970, 0x00);
  r.impl.begin(BoardPowerRig::kConfiguredMah, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("input current limit could not be set, the charger keeps its own 500 mA"));
  TEST_ASSERT_TRUE(r.log.contains("charge current limited to 960 mA"));  // the other setting was made
  TEST_ASSERT_TRUE(r.log.contains("fuel gauge capacity"));  // the rest of the start ran
}

void test_the_input_limit_is_written_again_when_the_charger_set_its_own_at_a_plug_in() {
  BoardPowerRig r;  // started: the limit is 800 mA (0x4E)
  FakeMillis millis;
  r.impl.setClock(millis);
  millis.setTo(5000);
  TEST_ASSERT_FALSE(r.impl.externalPower());
  const uint8_t addr = r.chargerAddress();
  r.bus.setRegister(addr, 0x00, 0x48);  // the chip's input detection at the plug-in: "unknown adapter", 500 mA
  r.setStatus(0x14);                    // power good, charging
  millis.advance(BoardPower::kReadingTtlMs);
  TEST_ASSERT_TRUE(r.impl.externalPower());
  TEST_ASSERT_EQUAL_HEX8(0x4E, r.bus.reg(addr, 0x00));
  TEST_ASSERT_TRUE(r.log.contains("input current limit set to 800 mA again (it was 500 mA"));
  millis.advance(BoardPower::kReadingTtlMs);
  r.impl.batteryPercent();
  millis.advance(BoardPower::kReadingTtlMs);
  r.impl.batteryPercent();
  TEST_ASSERT_EQUAL(2, r.bus.writeCount(addr, 0x00));  // the start and the plug-in, not every reading
}

void test_on_the_battery_the_input_limit_register_is_left_alone() {
  BoardPowerRig r;
  const uint8_t addr = r.chargerAddress();
  r.bus.setRegister(addr, 0x00, 0x48);
  r.setStatus(0x00);  // no cable
  TEST_ASSERT_FALSE(r.impl.externalPower());
  TEST_ASSERT_EQUAL_HEX8(0x48, r.bus.reg(addr, 0x00));
  TEST_ASSERT_EQUAL(1, r.bus.writeCount(addr, 0x00));  // the start only
}

static int linesWith(const FakeEventLog& log, const std::string& part) {
  int n = 0;
  for (const std::string& line : log.lines()) n += line.find(part) != std::string::npos;
  return n;
}

void test_a_refused_write_at_the_cable_is_said_once_per_cable() {
  BoardPowerRig r;
  FakeMillis millis;
  r.impl.setClock(millis);
  millis.setTo(5000);
  const uint8_t addr = r.chargerAddress();
  r.bus.setRegister(addr, 0x00, 0x48);
  r.bus.refuseRegisterWrite(addr, 0x00);
  r.setStatus(0x14);
  for (int i = 0; i < 3; ++i) {
    millis.advance(BoardPower::kReadingTtlMs);
    r.impl.externalPower();
  }
  TEST_ASSERT_EQUAL(1, linesWith(r.log, "could not be set again at the cable, the charger keeps its own 500 mA"));
  r.setStatus(0x00);
  millis.advance(BoardPower::kReadingTtlMs);
  r.impl.externalPower();
  r.setStatus(0x14);
  millis.advance(BoardPower::kReadingTtlMs);
  r.impl.externalPower();
  TEST_ASSERT_EQUAL(2, linesWith(r.log, "could not be set again at the cable"));  // the next cable says it again
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

// The core's state asks five questions in one pass (percentage, voltage, minutes, charge state, cable), and each
// was a reading of the gauge and the charger over I2C: 52 ms of every pass in the night of 2026-10-13 (D-31).
// With a clock, one reading serves every question for a second; the next second reads again.
void test_with_a_clock_one_reading_serves_every_question_for_a_second() {
  BoardPowerRig r;
  FakeMillis millis;
  r.impl.setClock(millis);
  millis.setTo(5000);
  const size_t before = r.bus.transactions().size();
  r.impl.batteryPercent();
  const size_t oneReading = r.bus.transactions().size() - before;
  TEST_ASSERT_TRUE(oneReading > 0);
  r.impl.batteryMillivolts();
  r.impl.batteryMinutesRemaining();
  r.impl.chargeState();
  r.impl.externalPower();
  TEST_ASSERT_EQUAL(oneReading, r.bus.transactions().size() - before);   // nothing more was read
  millis.advance(BoardPower::kReadingTtlMs - 1);
  r.impl.batteryPercent();
  TEST_ASSERT_EQUAL(oneReading, r.bus.transactions().size() - before);
  millis.advance(1);
  r.impl.batteryPercent();
  TEST_ASSERT_EQUAL(2 * oneReading, r.bus.transactions().size() - before);  // a second later: read again
  // A change on the chips is seen at the next reading, not before.
  r.scriptGauge(3600, 40, 1500, 1500);
  TEST_ASSERT_EQUAL(3900, r.impl.batteryMillivolts());
  millis.advance(BoardPower::kReadingTtlMs);
  TEST_ASSERT_EQUAL(3600, r.impl.batteryMillivolts());
}

void test_without_a_clock_every_question_reads_the_chips() {
  BoardPowerRig r;
  const size_t before = r.bus.transactions().size();
  r.impl.batteryPercent();
  const size_t oneReading = r.bus.transactions().size() - before;
  r.impl.externalPower();
  TEST_ASSERT_TRUE(r.bus.transactions().size() - before > oneReading);
}

void test_a_reading_that_was_kept_is_replaced_after_a_second_and_the_wrap_of_the_counter_is_harmless() {
  BoardPowerRig r;
  FakeMillis millis;
  r.impl.setClock(millis);
  millis.setTo(0xFFFFFFF0u);
  r.scriptGauge(3900, 80, 1500, 1500);
  TEST_ASSERT_EQUAL(3900, r.impl.batteryMillivolts());
  r.scriptGauge(3700, 60, 1500, 1500);
  TEST_ASSERT_EQUAL(3900, r.impl.batteryMillivolts());     // kept
  millis.advance(BoardPower::kReadingTtlMs);                // across the wrap
  TEST_ASSERT_EQUAL(3700, r.impl.batteryMillivolts());     // read again
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

// The gauge said 7 % at about 3630 mV with its compensated thresholds and learned a smaller full charge from
// each such discharge (D-42): the start sets this cell's, once, and says so in one line.
void test_start_sets_the_end_of_discharge_thresholds_once_and_logs_one_line_about_it() {
  BoardPowerRig r(false);
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains(
      "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3420/3470 mV, compensation off "
      "(they were 3031/3385/3501 mV, compensation on)"));
  TEST_ASSERT_EQUAL(4, (int)r.writer.dataMemoryWrites().size());
  FakeEventLog second;
  r.impl.begin(1500, r.writer, second);
  TEST_ASSERT_TRUE(second.contains("end-of-discharge thresholds are 3300/3420/3470 mV, compensation off already, nothing to write"));
  TEST_ASSERT_EQUAL(4, (int)r.writer.dataMemoryWrites().size());  // the second start writes nothing
}

void test_thresholds_the_gauge_refuses_are_logged_and_do_not_stop_the_start() {
  BoardPowerRig r(false);
  r.writer.refuseDataMemory();
  r.impl.begin(1500, r.writer, r.log);
  TEST_ASSERT_TRUE(r.log.contains("end-of-discharge thresholds could not be set"));
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
  RUN_TEST(test_the_gauge_that_said_zero_at_3601_mv_gives_the_voltages_number_and_says_so_once);
  RUN_TEST(test_a_voltage_on_the_border_leaves_one_log_line_and_one_number);
  RUN_TEST(test_the_rule_remembers_its_answer_without_a_log);
  RUN_TEST(test_the_cell_voltage_is_the_gauges_else_the_chargers_else_unknown);
  RUN_TEST(test_charging_is_reported_while_the_charger_charges);
  RUN_TEST(test_the_older_charger_at_6b_works_the_same);
  RUN_TEST(test_the_start_limits_the_charge_current_once_and_says_so);
  RUN_TEST(test_a_charger_that_refuses_the_limit_is_logged_and_the_start_goes_on);
  RUN_TEST(test_the_start_sets_the_input_limit_to_800_ma_once_and_says_so);
  RUN_TEST(test_a_charger_that_refuses_the_input_limit_is_logged_and_the_start_goes_on);
  RUN_TEST(test_the_input_limit_is_written_again_when_the_charger_set_its_own_at_a_plug_in);
  RUN_TEST(test_on_the_battery_the_input_limit_register_is_left_alone);
  RUN_TEST(test_a_refused_write_at_the_cable_is_said_once_per_cable);
  RUN_TEST(test_no_charger_says_so_once_and_reports_unknown);
  RUN_TEST(test_a_charger_that_appears_later_is_found);
  RUN_TEST(test_a_missing_gauge_gives_unknown_battery_and_leaves_the_charger_alone);
  RUN_TEST(test_with_a_clock_one_reading_serves_every_question_for_a_second);
  RUN_TEST(test_without_a_clock_every_question_reads_the_chips);
  RUN_TEST(test_a_reading_that_was_kept_is_replaced_after_a_second_and_the_wrap_of_the_counter_is_harmless);
  RUN_TEST(test_start_writes_the_capacity_once_and_logs_one_line_about_it);
  RUN_TEST(test_a_refused_capacity_write_is_logged_and_does_not_stop_the_start);
  RUN_TEST(test_start_sets_the_end_of_discharge_thresholds_once_and_logs_one_line_about_it);
  RUN_TEST(test_thresholds_the_gauge_refuses_are_logged_and_do_not_stop_the_start);
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
