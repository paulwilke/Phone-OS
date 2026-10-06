// The touch chip: the nine-byte report, the family check, the start-up sequence and what a touch or a key
// becomes. The weird packing of the coordinates and the keys that come before fingers cost days on the device.

#include <unity.h>

#include <board/Cst66xx.h>
#include <board/PinMap.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeI2cBus.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
void withChecksum(uint8_t* buf) {
  const uint16_t sum = cst66xx::checksum(&buf[4], 5);
  buf[0] = (uint8_t)(sum & 0xFF);
  buf[1] = (uint8_t)(sum >> 8);
}

// One finger at (x, y), on the screen.
void fingerReport(uint8_t* buf, int x, int y, uint8_t event = 0x60) {
  buf[2] = 0xFF;
  buf[3] = 0x01;
  buf[4] = (uint8_t)(x & 0xFF);
  buf[5] = (uint8_t)(y & 0xFF);
  buf[6] = 40;
  buf[7] = (uint8_t)(((x >> 8) & 0x0F) | (((y >> 8) & 0x0F) << 4));
  buf[8] = event;
  withChecksum(buf);
}

// A sensor key, pressed or let go.
void keyReport(uint8_t* buf, uint8_t id, bool pressed) {
  buf[2] = 0xFF;
  buf[3] = 0x10;
  buf[4] = buf[5] = buf[6] = buf[7] = 0;
  buf[8] = (uint8_t)((pressed ? 0x10 : 0x00) | (id & 0x0F));
  withChecksum(buf);
}

std::vector<uint8_t> bytes(const uint8_t* b, size_t n) { return std::vector<uint8_t>(b, b + n); }
}  // namespace

void test_finger_coordinates_span_twelve_bits() {
  uint8_t buf[9] = {};
  fingerReport(buf, 231, 317);
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_TRUE(r.hasPosition);
  TEST_ASSERT_FALSE(r.hasKey);
  TEST_ASSERT_TRUE(r.down);
  TEST_ASSERT_EQUAL(231, r.x);
  TEST_ASSERT_EQUAL(317, r.y);
  TEST_ASSERT_TRUE(r.checksumOk);
}

// y keeps its upper bits in the upper nibble of buf[7]; reading them from the lower one lost half the screen.
void test_a_finger_above_255_keeps_both_axes() {
  uint8_t buf[9] = {};
  fingerReport(buf, 300, 300);
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_EQUAL(300, r.x);
  TEST_ASSERT_EQUAL(300, r.y);
}

void test_the_event_nibble_says_whether_the_finger_is_down() {
  uint8_t buf[9] = {};
  fingerReport(buf, 100, 100, 0x00);
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_TRUE(r.hasPosition);
  TEST_ASSERT_FALSE(r.down);
}

void test_each_key_reports_its_number_and_state() {
  for (uint8_t id = 0; id < cst66xx::kKeyCount; ++id) {
    uint8_t buf[9] = {};
    keyReport(buf, id, true);
    const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
    TEST_ASSERT_TRUE(r.hasKey);
    TEST_ASSERT_FALSE(r.hasPosition);
    TEST_ASSERT_EQUAL(id, r.keyId);
    TEST_ASSERT_TRUE(r.keyPressed);
    TEST_ASSERT_TRUE(r.checksumOk);
  }
  uint8_t buf[9] = {};
  keyReport(buf, 2, false);
  TEST_ASSERT_FALSE(cst66xx::parseReport(buf, sizeof(buf)).keyPressed);
}

void test_a_gesture_is_not_a_touch() {
  uint8_t buf[9] = {};
  fingerReport(buf, 50, 50);
  buf[2] = 0xF0;
  withChecksum(buf);
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_FALSE(r.hasPosition);
  TEST_ASSERT_FALSE(r.hasKey);
}

void test_an_empty_report_holds_nothing_and_a_short_buffer_too() {
  uint8_t buf[9] = {};
  buf[2] = 0xFF;
  withChecksum(buf);
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_FALSE(r.hasPosition);
  TEST_ASSERT_FALSE(r.hasKey);
  TEST_ASSERT_FALSE(cst66xx::parseReport(buf, 8).hasPosition);
  TEST_ASSERT_FALSE(cst66xx::parseReport(nullptr, 9).hasPosition);
}

void test_a_wrong_checksum_is_marked() {
  uint8_t buf[9] = {};
  keyReport(buf, 1, true);
  buf[0] = (uint8_t)(buf[0] + 1);
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_TRUE(r.hasKey);
  TEST_ASSERT_FALSE(r.checksumOk);
}

// A key stands first, a finger would come behind it, outside the nine bytes: no coordinates then.
void test_a_key_and_a_finger_together_report_only_the_key() {
  uint8_t buf[9] = {};
  keyReport(buf, 0, true);
  buf[3] = 0x11;
  const cst66xx::Report r = cst66xx::parseReport(buf, sizeof(buf));
  TEST_ASSERT_TRUE(r.hasKey);
  TEST_ASSERT_FALSE(r.hasPosition);
  TEST_ASSERT_FALSE(r.checksumOk);
}

namespace {
constexpr uint8_t kAddr = i2c::kTouch;

// The identity block the real chip gave: 0xCA twice at 2 and 3 and the resolution 240 by 320 at 28..31.
std::vector<uint8_t> info() {
  std::vector<uint8_t> v(50, 0);
  v[2] = v[3] = 0xCA;
  v[28] = 240 & 0xFF;
  v[29] = 240 >> 8;
  v[30] = 320 & 0xFF;
  v[31] = 320 >> 8;
  return v;
}
const std::vector<uint8_t> kInfoCmd = {0xD0, 0x03, 0x00, 0x00};
const std::vector<uint8_t> kReportCmd = {0xD0, 0x07, 0x00, 0x00};

struct Rig {
  FakeDelay delay;
  FakeI2cBus bus{&delay};
  Cst66xxTouch touch{bus, delay};
  Rig() { bus.addChip(kAddr); }
  void scriptInfo() { bus.setReply(kAddr, kInfoCmd, info()); }
  void scriptReport(const uint8_t (&buf)[9]) { bus.setReply(kAddr, kReportCmd, bytes(buf, 9)); }
};
}  // namespace

void test_the_family_is_asked_for_and_the_resolution_is_read_from_the_chip() {
  Rig r;
  r.scriptInfo();
  TEST_ASSERT_TRUE(r.touch.begin());
  TEST_ASSERT_TRUE(r.touch.ready());
  TEST_ASSERT_EQUAL(240, r.touch.resolutionX());
  TEST_ASSERT_EQUAL(320, r.touch.resolutionY());
}

// Without the normal mode the chip delivers fingers but not the sensor keys.
void test_the_normal_mode_is_set_in_the_order_of_the_manufacturers_driver() {
  Rig r;
  r.scriptInfo();
  r.touch.begin();
  std::vector<std::vector<uint8_t>> commands;
  for (const auto& w : r.bus.writesTo(kAddr))
    if (w.size() == 4 && w != kInfoCmd) commands.push_back(w);
  const std::vector<std::vector<uint8_t>> expected = {{0xD0, 0x00, 0x04, 0x00}, {0xD0, 0x00, 0x04, 0x00},
                                                      {0xD0, 0x00, 0x00, 0x00}, {0xD0, 0x00, 0x0C, 0x00},
                                                      {0xD0, 0x00, 0x01, 0x00}};
  TEST_ASSERT_TRUE(expected == commands);
}

// Another family answers on the same address with its program memory: nothing may be read from it.
void test_a_chip_of_another_family_is_reported_and_never_read() {
  Rig r;
  std::vector<uint8_t> machineCode(50, 0x1F);
  r.bus.setReply(kAddr, kInfoCmd, machineCode);
  TEST_ASSERT_FALSE(r.touch.begin());
  TEST_ASSERT_FALSE(r.touch.ready());
  cst66xx::Report rep;
  TEST_ASSERT_FALSE(r.touch.poll(rep));
  for (const auto& w : r.bus.writesTo(kAddr)) TEST_ASSERT_TRUE(w != kReportCmd);
}

void test_five_tries_with_the_bootloader_left_between_them() {
  Rig r;
  r.bus.setReply(kAddr, kInfoCmd, std::vector<uint8_t>(50, 0));
  TEST_ASSERT_FALSE(r.touch.begin());
  int infoReads = 0, leaves = 0;
  for (const auto& w : r.bus.writesTo(kAddr)) {
    if (w == kInfoCmd) ++infoReads;
    if (w == std::vector<uint8_t>({0xD0, 0x00, 0x04, 0x00})) ++leaves;
  }
  TEST_ASSERT_EQUAL(5, infoReads);
  TEST_ASSERT_EQUAL(5, leaves);
  TEST_ASSERT_TRUE(r.delay.elapsedMs() >= 50);
}

void test_a_missing_touch_chip_is_reported() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  Cst66xxTouch touch(bus, delay);
  TEST_ASSERT_FALSE(touch.begin());
}

// With a repeated start this chip returns garbage; the address ends with a stop and the read follows later.
void test_the_chip_is_never_read_with_a_repeated_start() {
  Rig r;
  r.scriptInfo();
  r.touch.begin();
  uint8_t buf[9] = {};
  fingerReport(buf, 10, 20);
  r.scriptReport(buf);
  cst66xx::Report rep;
  r.touch.poll(rep);
  TEST_ASSERT_EQUAL(0, r.bus.repeatedStarts(kAddr));
}

void test_a_poll_reads_one_report_and_acknowledges_it() {
  Rig r;
  r.scriptInfo();
  r.touch.begin();
  uint8_t buf[9] = {};
  fingerReport(buf, 231, 317);
  r.scriptReport(buf);
  cst66xx::Report rep;
  TEST_ASSERT_TRUE(r.touch.poll(rep));
  TEST_ASSERT_EQUAL(231, rep.x);
  TEST_ASSERT_EQUAL(317, rep.y);
  const auto writes = r.bus.writesTo(kAddr);
  TEST_ASSERT_TRUE(writes.back() == std::vector<uint8_t>({0xD0, 0x00, 0x02, 0xAB}));
}

void test_a_failing_bus_gives_no_report() {
  Rig r;
  r.scriptInfo();
  r.touch.begin();
  r.bus.removeChip(kAddr);
  cst66xx::Report rep;
  TEST_ASSERT_FALSE(r.touch.poll(rep));
}

namespace {
bool decodeOne(TouchDecoder& d, const uint8_t (&buf)[9], ui::Input& out) {
  return d.decode(cst66xx::parseReport(buf, 9), out);
}
}  // namespace

void test_a_touch_is_reported_once_when_the_finger_lands() {
  TouchDecoder d;
  uint8_t buf[9] = {};
  ui::Input in;
  fingerReport(buf, 120, 200);
  TEST_ASSERT_TRUE(decodeOne(d, buf, in));
  TEST_ASSERT_TRUE(in.kind == ui::Input::Kind::Touch);
  TEST_ASSERT_EQUAL(120, in.x);
  TEST_ASSERT_EQUAL(200, in.y);
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));  // still down: no second tap
  fingerReport(buf, 121, 201);
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));  // a moving finger is not a new tap
  fingerReport(buf, 121, 201, 0x00);
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));  // lifted
  fingerReport(buf, 50, 60);
  TEST_ASSERT_TRUE(decodeOne(d, buf, in));  // the next contact is a new tap
  TEST_ASSERT_EQUAL(50, in.x);
}

void test_an_empty_report_lifts_the_finger() {
  TouchDecoder d;
  uint8_t buf[9] = {};
  ui::Input in;
  fingerReport(buf, 10, 10);
  decodeOne(d, buf, in);
  uint8_t none[9] = {};
  none[2] = 0xFF;
  withChecksum(none);
  TEST_ASSERT_FALSE(decodeOne(d, none, in));
  TEST_ASSERT_TRUE(decodeOne(d, buf, in));
}

void test_coordinates_stay_on_the_screen() {
  TouchDecoder d;
  uint8_t buf[9] = {};
  ui::Input in;
  fingerReport(buf, 400, 900);
  TEST_ASSERT_TRUE(decodeOne(d, buf, in));
  TEST_ASSERT_EQUAL(239, in.x);
  TEST_ASSERT_EQUAL(319, in.y);
}

// 2026-10-01 on the device: left 0, middle 1, right 2.
void test_the_three_keys_are_home_inbox_and_compose() {
  const ui::Key expected[] = {ui::Key::Home, ui::Key::Inbox, ui::Key::Compose};
  for (uint8_t id = 0; id < 3; ++id) {
    TouchDecoder d;
    uint8_t buf[9] = {};
    keyReport(buf, id, true);
    ui::Input in;
    TEST_ASSERT_TRUE(decodeOne(d, buf, in));
    TEST_ASSERT_TRUE(in.isKey(expected[id]));
    TEST_ASSERT_TRUE(in.isHardwareKey());
  }
}

void test_a_key_is_reported_on_the_way_down_only() {
  TouchDecoder d;
  uint8_t buf[9] = {};
  ui::Input in;
  keyReport(buf, 1, true);
  TEST_ASSERT_TRUE(decodeOne(d, buf, in));
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));  // held: the chip repeats the report
  keyReport(buf, 1, false);
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));
  keyReport(buf, 1, true);
  TEST_ASSERT_TRUE(decodeOne(d, buf, in));
}

// An invented key press jumps to another screen: no checksum, no key.
void test_a_key_with_a_wrong_checksum_or_number_is_ignored() {
  TouchDecoder d;
  uint8_t buf[9] = {};
  ui::Input in;
  keyReport(buf, 0, true);
  buf[0] = (uint8_t)(buf[0] + 1);
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));
  keyReport(buf, 3, true);
  TEST_ASSERT_FALSE(decodeOne(d, buf, in));
}

void test_a_key_does_not_count_as_a_touch_of_the_screen() {
  TouchDecoder d;
  uint8_t buf[9] = {};
  ui::Input in;
  keyReport(buf, 2, true);
  decodeOne(d, buf, in);
  TEST_ASSERT_FALSE(in.kind == ui::Input::Kind::Touch);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_finger_coordinates_span_twelve_bits);
  RUN_TEST(test_a_finger_above_255_keeps_both_axes);
  RUN_TEST(test_the_event_nibble_says_whether_the_finger_is_down);
  RUN_TEST(test_each_key_reports_its_number_and_state);
  RUN_TEST(test_a_gesture_is_not_a_touch);
  RUN_TEST(test_an_empty_report_holds_nothing_and_a_short_buffer_too);
  RUN_TEST(test_a_wrong_checksum_is_marked);
  RUN_TEST(test_a_key_and_a_finger_together_report_only_the_key);
  RUN_TEST(test_the_family_is_asked_for_and_the_resolution_is_read_from_the_chip);
  RUN_TEST(test_the_normal_mode_is_set_in_the_order_of_the_manufacturers_driver);
  RUN_TEST(test_a_chip_of_another_family_is_reported_and_never_read);
  RUN_TEST(test_five_tries_with_the_bootloader_left_between_them);
  RUN_TEST(test_a_missing_touch_chip_is_reported);
  RUN_TEST(test_the_chip_is_never_read_with_a_repeated_start);
  RUN_TEST(test_a_poll_reads_one_report_and_acknowledges_it);
  RUN_TEST(test_a_failing_bus_gives_no_report);
  RUN_TEST(test_a_touch_is_reported_once_when_the_finger_lands);
  RUN_TEST(test_an_empty_report_lifts_the_finger);
  RUN_TEST(test_coordinates_stay_on_the_screen);
  RUN_TEST(test_the_three_keys_are_home_inbox_and_compose);
  RUN_TEST(test_a_key_is_reported_on_the_way_down_only);
  RUN_TEST(test_a_key_with_a_wrong_checksum_or_number_is_ignored);
  RUN_TEST(test_a_key_does_not_count_as_a_touch_of_the_screen);
  return UNITY_END();
}
