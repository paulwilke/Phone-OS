// The charger: decoding of the registers with the values measured on the device, both chip revisions, and
// that reading writes nothing.

#include <unity.h>

#include <board/Charger.h>
#include <board/PinMap.h>
#include <fakes/FakeI2cBus.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
// The registers read from the real device on 2026-10-06: a full battery on a cable.
void measured(uint8_t (&r)[kChargerRegisterCount]) {
  for (auto& b : r) b = 0;
  r[0x00] = 0x48;
  r[0x04] = 0x20;
  r[0x05] = 0x13;
  r[0x06] = 0x5E;
  r[0x0B] = 0xBE;
  r[0x0E] = 0x5D;
}
}  // namespace

void test_the_registers_measured_on_the_device_decode() {
  uint8_t r[kChargerRegisterCount];
  measured(r);
  const ChargerStatus s = decodeCharger(r);
  TEST_ASSERT_TRUE(s.powerGood);
  TEST_ASSERT_TRUE(s.phase == ChargePhase::Done);
  TEST_ASSERT_TRUE(s.input == InputKind::UnknownAdapter);
  TEST_ASSERT_EQUAL(500, s.inputCurrentLimitMa);
  TEST_ASSERT_TRUE(s.inputLimitEnabled);
  TEST_ASSERT_EQUAL(2048, s.chargeCurrentMa);
  TEST_ASSERT_EQUAL(128, s.prechargeCurrentMa);
  TEST_ASSERT_EQUAL(256, s.terminationCurrentMa);
  TEST_ASSERT_EQUAL(4208, s.chargeVoltageMv);  // the target the notes name
  TEST_ASSERT_EQUAL(4164, s.batteryMv);        // the cell the notes call 4.16 V
}

void test_every_charge_phase_decodes() {
  uint8_t r[kChargerRegisterCount] = {};
  const ChargePhase expected[] = {ChargePhase::NotCharging, ChargePhase::PreCharge, ChargePhase::FastCharge, ChargePhase::Done};
  for (int phase = 0; phase < 4; ++phase) {
    r[0x0B] = (uint8_t)(phase << 3);
    TEST_ASSERT_TRUE(decodeCharger(r).phase == expected[phase]);
  }
}

// A full battery on a cable reads "not charging" or "done", so the cable is bit 2 and not the phase.
void test_the_cable_is_the_power_good_bit_not_the_phase() {
  uint8_t r[kChargerRegisterCount] = {};
  r[0x0B] = 0x04;  // power good, not charging
  TEST_ASSERT_TRUE(decodeCharger(r).powerGood);
  TEST_ASSERT_TRUE(decodeCharger(r).phase == ChargePhase::NotCharging);
  r[0x0B] = 0x18;  // done, but no supply bit
  TEST_ASSERT_FALSE(decodeCharger(r).powerGood);
}

void test_input_kinds_and_the_limit_range() {
  uint8_t r[kChargerRegisterCount] = {};
  r[0x0B] = 0x20 | 0x04;
  TEST_ASSERT_TRUE(decodeCharger(r).input == InputKind::UsbHost);
  r[0x0B] = 0xC0;
  TEST_ASSERT_TRUE(decodeCharger(r).input == InputKind::NonStandard);
  r[0x0B] = 0x60;
  TEST_ASSERT_TRUE(decodeCharger(r).input == InputKind::UsbDedicated);
  r[0x0B] = 0xE0;
  TEST_ASSERT_TRUE(decodeCharger(r).input == InputKind::OnTheGo);
  r[0x00] = 0x3F;
  TEST_ASSERT_EQUAL(3250, decodeCharger(r).inputCurrentLimitMa);
  TEST_ASSERT_FALSE(decodeCharger(r).inputLimitEnabled);
  r[0x00] = 0x00;
  TEST_ASSERT_EQUAL(100, decodeCharger(r).inputCurrentLimitMa);
}

void test_the_newer_charger_is_found_at_6a() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerSy6970);
  Charger c(bus);
  TEST_ASSERT_TRUE(c.detect());
  TEST_ASSERT_TRUE(c.chip() == ChargerChip::Sy6970);
  TEST_ASSERT_EQUAL_HEX8(0x6A, c.address());
}

void test_the_older_charger_is_found_at_6b() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerBq25896);
  bus.setRegister(i2c::kChargerBq25896, 0x0B, 0x14 | 0x04);
  Charger c(bus);
  TEST_ASSERT_TRUE(c.detect());
  TEST_ASSERT_TRUE(c.chip() == ChargerChip::Bq25896);
  ChargerStatus s;
  TEST_ASSERT_TRUE(c.readStatus(s));
  TEST_ASSERT_TRUE(s.phase == ChargePhase::FastCharge);
  TEST_ASSERT_TRUE(s.powerGood);
}

void test_neither_charger_is_reported_as_absent_not_as_a_broken_bus() {
  FakeI2cBus bus;
  bus.addChip(i2c::kExpander);  // the bus works; there is just no charger
  Charger c(bus);
  TEST_ASSERT_FALSE(c.detect());
  TEST_ASSERT_FALSE(c.found());
  TEST_ASSERT_TRUE(c.chip() == ChargerChip::None);
  ChargerStatus s;
  TEST_ASSERT_FALSE(c.readStatus(s));
  TEST_ASSERT_FALSE(c.disconnectBattery());
}

void test_both_present_prefers_6a() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerSy6970);
  bus.addChip(i2c::kChargerBq25896);
  Charger c(bus);
  c.detect();
  TEST_ASSERT_EQUAL_HEX8(0x6A, c.address());
}

void test_reading_all_registers_writes_nothing() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerSy6970);
  uint8_t m[kChargerRegisterCount];
  measured(m);
  bus.setRegisters(i2c::kChargerSy6970, 0x00, std::vector<uint8_t>(m, m + kChargerRegisterCount));
  Charger c(bus);
  c.detect();
  uint8_t r[kChargerRegisterCount];
  ChargerStatus s;
  TEST_ASSERT_TRUE(c.readAll(r, s));
  TEST_ASSERT_EQUAL_HEX8(0xBE, r[0x0B]);
  TEST_ASSERT_EQUAL(4208, s.chargeVoltageMv);
  for (const auto& w : bus.writesTo(i2c::kChargerSy6970))
    TEST_ASSERT_TRUE_MESSAGE(w.size() <= 1, "a write that carries data");  // a probe or the register pointer
  for (uint8_t reg = 0; reg < kChargerRegisterCount; ++reg) TEST_ASSERT_EQUAL(0, bus.writeCount(i2c::kChargerSy6970, reg));
}

void test_a_charger_that_answers_the_probe_but_not_the_read_is_reported() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerSy6970);
  Charger c(bus);
  c.detect();
  bus.removeChip(i2c::kChargerSy6970);
  uint8_t r[kChargerRegisterCount];
  ChargerStatus s;
  TEST_ASSERT_FALSE(c.readAll(r, s));
  TEST_ASSERT_FALSE(c.readStatus(s));
}

// Only the one bit changes; the rest of the register stays as the chip had it.
void test_disconnecting_the_battery_sets_only_the_switch_bit() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerSy6970);
  bus.setRegister(i2c::kChargerSy6970, 0x09, 0x44);
  Charger c(bus);
  c.detect();
  TEST_ASSERT_TRUE(c.disconnectBattery());
  TEST_ASSERT_EQUAL_HEX8(0x64, bus.reg(i2c::kChargerSy6970, 0x09));
  TEST_ASSERT_EQUAL(1, bus.writeCount(i2c::kChargerSy6970, 0x09));
}

void test_a_refused_write_to_the_switch_is_reported() {
  FakeI2cBus bus;
  bus.addChip(i2c::kChargerSy6970);
  bus.refuseRegisterWrite(i2c::kChargerSy6970, 0x09);
  Charger c(bus);
  c.detect();
  TEST_ASSERT_FALSE(c.disconnectBattery());
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.reg(i2c::kChargerSy6970, 0x09));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_registers_measured_on_the_device_decode);
  RUN_TEST(test_every_charge_phase_decodes);
  RUN_TEST(test_the_cable_is_the_power_good_bit_not_the_phase);
  RUN_TEST(test_input_kinds_and_the_limit_range);
  RUN_TEST(test_the_newer_charger_is_found_at_6a);
  RUN_TEST(test_the_older_charger_is_found_at_6b);
  RUN_TEST(test_neither_charger_is_reported_as_absent_not_as_a_broken_bus);
  RUN_TEST(test_both_present_prefers_6a);
  RUN_TEST(test_reading_all_registers_writes_nothing);
  RUN_TEST(test_a_charger_that_answers_the_probe_but_not_the_read_is_reported);
  RUN_TEST(test_disconnecting_the_battery_sets_only_the_switch_bit);
  RUN_TEST(test_a_refused_write_to_the_switch_is_reported);
  return UNITY_END();
}
