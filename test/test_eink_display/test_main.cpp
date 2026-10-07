// The panel pipeline: what is sent for a full and a partial refresh, which rectangle, in which order, and what
// happens without a profile or with a panel that hangs.
//
// **The profile used here is invented test data, not the panel's.** It has the shape of one (a start list, the
// commands before each kind of refresh, a window encoding, a sleep list) with values that mean nothing, so
// that the sequences of the driver can be compared byte for byte. That the panel's own values make it show a
// picture is not provable without the panel (docs/device-display.md).

#include <unity.h>

#include <vector>

#include <board/EinkDisplay.h>
#include <board/PinMap.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeMillis.h>
#include <fakes/FakeSpiPort.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
std::vector<uint8_t> testWindow(const ui::Rect& r) {
  return {(uint8_t)(r.x / 8), (uint8_t)((r.x + r.w) / 8), (uint8_t)(r.y >> 8), (uint8_t)r.y, (uint8_t)((r.y + r.h - 1) >> 8),
          (uint8_t)(r.y + r.h - 1)};
}

PanelProfile testProfile() {
  PanelProfile p;
  p.width = 240;
  p.height = 320;
  p.start = {{0xA1, {0x01, 0x02}}, {0xA2, {}, true}};
  p.beforeFull = {{0xB1, {0x11}}};
  p.beforePartial = {{0xB2, {0x22}}};
  p.sleep = {{0xC1, {}, true, 7}, {0xC2, {0x05}}};
  p.refreshSettleMs = 3;
  p.encodeWindow = testWindow;
  p.resetLowMs = 10;
  p.resetHighMs = 20;
  p.busyTimeoutMs = 100;
  p.busyIsHigh = true;
  return p;
}

struct Rig {
  FakeDelay delay;
  FakeGpio gpio{&delay};
  FakeSpiPort spi{gpio};
  FakeMillis millis;
  FakeEventLog log;
  PanelProfile profile = testProfile();
  EpdWire wire{spi, gpio, delay};
  EinkDisplay display;
  explicit Rig(bool withProfile = true) : display(wire, withProfile ? &profile : nullptr, millis, log) {}

  void frame(void (*draw)(ui::Display&)) {
    display.beginFrame();
    draw(display);
    display.endFrame();
  }
  void idleBusy() { gpio.setInput(pin::kDisplayBusy, false); }  // not busy (the profile says busy is high)
};

std::string join(const std::vector<std::string>& v) {
  std::string s;
  for (const auto& x : v) s += x + "|";
  return s;
}
bool starts(const std::vector<std::string>& v, std::initializer_list<const char*> prefix) {
  size_t i = 0;
  for (const char* p : prefix) {
    if (i >= v.size() || v[i] != p) return false;
    ++i;
  }
  return true;
}
}  // namespace

void test_without_a_profile_the_panel_is_not_driven_and_says_so() {
  Rig r(false);
  TEST_ASSERT_FALSE(r.display.begin());
  TEST_ASSERT_FALSE(r.display.configured());
  TEST_ASSERT_TRUE(r.log.contains("Display: not configured"));
  r.frame([](ui::Display& d) { d.fillRect({0, 0, 10, 10}, true); });
  TEST_ASSERT_EQUAL(0, r.spi.writes().size());
  TEST_ASSERT_EQUAL(0, r.display.stats().frames);
}

void test_a_profile_of_the_wrong_size_or_without_values_is_not_used() {
  Rig r;
  r.profile.width = 320;
  TEST_ASSERT_FALSE(r.display.begin());
  r.profile = testProfile();
  r.profile.start.clear();
  TEST_ASSERT_FALSE(r.display.begin());
  r.profile = testProfile();
  r.profile.busyTimeoutMs = 0;
  TEST_ASSERT_FALSE(r.display.begin());
  TEST_ASSERT_EQUAL(0, r.spi.writes().size());
}

// The panel sleeps between refreshes, so begin() leaves it alone and every refresh wakes it.
void test_begin_does_not_touch_the_panel_and_a_refresh_resets_and_starts_it_in_order() {
  Rig r;
  r.idleBusy();
  TEST_ASSERT_TRUE(r.display.begin());
  TEST_ASSERT_EQUAL(0, r.spi.writes().size());
  r.frame([](ui::Display&) {});
  TEST_ASSERT_TRUE(starts(r.spi.flatten(), {"C:A1", "D:01 02", "C:A2", "C:B1"}));
  // 10 ms low, 20 ms high, 3 ms after the refresh command, 7 ms after the power-off wait.
  TEST_ASSERT_EQUAL(40, r.delay.elapsedMs());
  // The reset line was pulsed low and released.
  bool low = false, high = false;
  for (const auto& e : r.gpio.events())
    if (e.pin == pin::kDisplayReset) (e.high ? high : low) = true;
  TEST_ASSERT_TRUE(low && high);
}

void test_every_refresh_wakes_the_panel_again() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});
  r.spi.clear();
  r.frame([](ui::Display& d) { d.setPixel(1, 1, true); });
  TEST_ASSERT_TRUE(starts(r.spi.flatten(), {"C:A1", "D:01 02", "C:A2", "C:B2"}));
  TEST_ASSERT_EQUAL_STRING("C:C2", r.spi.flatten()[r.spi.flatten().size() - 2].c_str());
}

// A power-on or power-off that leaves the panel busy is said, and the next frame is a full refresh.
void test_a_panel_that_stays_busy_after_power_on_is_said() {
  Rig r;
  r.gpio.setInput(pin::kDisplayBusy, true);  // busy for ever (the profile says busy is high)
  r.display.begin();
  r.frame([](ui::Display&) {});
  TEST_ASSERT_TRUE(r.log.contains("stayed busy after a command"));
}

// A change smaller than the panel's smallest window is sent as the window around it, inside the screen.
void test_a_tiny_change_is_sent_in_the_smallest_window_the_profile_allows() {
  Rig r;
  r.profile.minWindowWidth = 16;
  r.profile.minWindowHeight = 4;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});
  r.spi.clear();
  r.frame([](ui::Display& d) { d.setPixel(239, 319, true); });  // the bottom right corner
  const auto t = r.spi.flatten();
  // The corner pixel is in byte 29 of row 319; the window grows to 16 x 4 and moves in to fit: bytes 28..29,
  // rows 316..319.
  TEST_ASSERT_EQUAL_STRING("D:1C 1E 01 3C 01 3F", t[7].c_str());
  TEST_ASSERT_EQUAL(2 * 4, (int)r.spi.writes()[9].bytes.size());
}

// A command is sent with the data line low, data with it high, and the chip selected for each.
void test_commands_go_out_with_dc_low_and_data_with_dc_high_inside_a_chip_select() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});
  for (const auto& w : r.spi.writes()) TEST_ASSERT_TRUE(w.csLow);
  TEST_ASSERT_FALSE(r.spi.writes()[0].dcHigh);
  TEST_ASSERT_TRUE(r.spi.writes()[1].dcHigh);
  TEST_ASSERT_TRUE(r.gpio.level(pin::kDisplayCs));  // released afterwards
}

void test_the_first_frame_is_a_full_refresh_with_old_and_new_data_whole() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.spi.clear();
  r.frame([](ui::Display& d) { d.fillRect({0, 0, 8, 1}, true); });
  const auto t = r.spi.flatten();
  TEST_ASSERT_TRUE(starts(t, {"C:A1", "D:01 02", "C:A2", "C:B1", "D:11", "C:10"}));
  TEST_ASSERT_EQUAL(13, t.size());  // A1 and data, A2, B1 and data, 10, old, 13, new, 12, C1, C2 and data
  TEST_ASSERT_EQUAL_STRING("C:13", t[7].c_str());
  TEST_ASSERT_EQUAL_STRING("C:12", t[9].c_str());
  TEST_ASSERT_EQUAL_STRING("C:C1", t[10].c_str());
  // The old picture is white (nothing was shown yet), the new one has its first byte set.
  TEST_ASSERT_EQUAL(1, r.display.stats().full);
  TEST_ASSERT_EQUAL(9600, (int)r.spi.writes()[6].bytes.size());
  TEST_ASSERT_EQUAL_HEX8(0x00, r.spi.writes()[6].bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, r.spi.writes()[8].bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, r.spi.writes()[8].bytes[1]);
}

void test_an_unchanged_frame_sends_nothing() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display& d) { d.fillRect({0, 0, 8, 8}, true); });
  r.spi.clear();
  r.frame([](ui::Display& d) { d.fillRect({0, 0, 8, 8}, true); });
  TEST_ASSERT_EQUAL(0, r.spi.writes().size());
  TEST_ASSERT_EQUAL(1, r.display.stats().skipped);
}

// Only the rectangle that changed, widened to whole bytes, goes to the panel, old and new.
void test_a_small_change_is_a_partial_refresh_of_exactly_its_rectangle() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display& d) { d.fillRect({0, 0, 8, 8}, true); });
  r.spi.clear();
  r.frame([](ui::Display& d) {
    d.fillRect({0, 0, 8, 8}, true);
    d.setPixel(21, 100, true);  // column 21 is in byte 2 (columns 16..23)
  });
  const auto t = r.spi.flatten();
  const std::vector<std::string> head = {"C:A1", "D:01 02", "C:A2", "C:B2", "D:22", "C:91", "C:90", "D:02 03 00 64 00 64",
                                         "C:10", "D:00", "C:13", "D:04", "C:12", "C:C1", "C:C2", "D:05"};
  TEST_ASSERT_EQUAL_STRING(join(head).c_str(), join(t).c_str());
  TEST_ASSERT_EQUAL(1, r.display.stats().partial);
}

void test_a_change_over_several_rows_and_columns_sends_the_bounding_box() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});
  r.spi.clear();
  r.frame([](ui::Display& d) {
    d.setPixel(9, 10, true);
    d.setPixel(40, 12, true);
  });
  const auto t = r.spi.flatten();
  TEST_ASSERT_EQUAL_STRING("D:01 06 00 0A 00 0C", t[7].c_str());  // bytes 1..5, rows 10..12
  TEST_ASSERT_EQUAL(5 * 3, (int)r.spi.writes()[9].bytes.size());  // old data: five bytes by three rows
  TEST_ASSERT_EQUAL(5 * 3, (int)r.spi.writes()[11].bytes.size());
}

void test_every_tenth_change_is_a_full_refresh_by_the_policy() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});  // the first: full
  for (int i = 1; i <= 10; ++i) {
    r.display.beginFrame();
    r.display.setPixel(i, 0, true);
    r.display.endFrame();
  }
  TEST_ASSERT_EQUAL(2, r.display.stats().full);
  TEST_ASSERT_EQUAL(9, r.display.stats().partial);
}

void test_a_picture_that_stays_for_four_hours_is_refreshed_in_full_once() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});
  r.spi.clear();
  r.millis.advance(4u * 3600u * 1000u - 1);
  r.frame([](ui::Display&) {});
  TEST_ASSERT_EQUAL(0, r.spi.writes().size());
  r.millis.advance(1);
  r.frame([](ui::Display&) {});
  TEST_ASSERT_EQUAL(2, r.display.stats().full);
}

void test_a_window_the_panel_cannot_address_becomes_a_full_refresh() {
  Rig r;
  r.profile.encodeWindow = [](const ui::Rect&) { return std::vector<uint8_t>(); };
  r.idleBusy();
  r.display.begin();
  r.frame([](ui::Display&) {});
  r.spi.clear();
  r.frame([](ui::Display& d) { d.setPixel(3, 3, true); });
  TEST_ASSERT_TRUE(starts(r.spi.flatten(), {"C:A1", "D:01 02", "C:A2", "C:B1"}));
}

void test_the_polarity_of_the_data_follows_the_profile() {
  Rig r;
  r.profile.blackIsZero = true;
  r.idleBusy();
  r.display.begin();
  r.spi.clear();
  r.frame([](ui::Display& d) { d.fillRect({0, 0, 8, 1}, true); });
  TEST_ASSERT_EQUAL_HEX8(0xFF, r.spi.writes()[6].bytes[0]);  // old: blank is all ones
  TEST_ASSERT_EQUAL_HEX8(0x00, r.spi.writes()[8].bytes[0]);  // new: the black byte is zero
}

// A panel that hangs does not hang the device; the next frame starts from a full refresh.
void test_a_panel_that_stays_busy_is_said_and_the_next_frame_is_full() {
  Rig r;
  r.gpio.setInput(pin::kDisplayBusy, true);  // busy for ever
  r.display.begin();
  r.frame([](ui::Display&) {});
  TEST_ASSERT_TRUE(r.log.contains("stayed busy"));
  TEST_ASSERT_TRUE(r.delay.elapsedMs() >= 100);
  r.idleBusy();
  r.spi.clear();
  r.frame([](ui::Display& d) { d.setPixel(1, 1, true); });
  TEST_ASSERT_TRUE(starts(r.spi.flatten(), {"C:A1", "D:01 02", "C:A2", "C:B1"}));
}

void test_the_picture_is_not_rotated_and_drawing_outside_is_dropped() {
  Rig r;
  r.idleBusy();
  r.display.begin();
  r.spi.clear();
  r.frame([](ui::Display& d) {
    d.setPixel(-1, 0, true);
    d.setPixel(240, 0, true);
    d.setPixel(0, 320, true);
    d.setPixel(239, 319, true);
  });
  const auto& newData = r.spi.writes()[8].bytes;
  TEST_ASSERT_EQUAL(9600, (int)newData.size());
  TEST_ASSERT_EQUAL_HEX8(0x01, newData[9599]);  // the last pixel: bottom right
  TEST_ASSERT_EQUAL_HEX8(0x00, newData[0]);
}

void test_a_bitmap_draws_only_its_set_bits() {
  Bitmap1 b(16, 2);
  b.fillRect({0, 0, 16, 2}, true);
  const uint8_t glyph[] = {0x80, 0x01};  // two pixels of a 8x2 glyph at (4, 0)
  b.drawBitmap(4, 0, glyph, 8, 2, false);
  TEST_ASSERT_FALSE(b.pixel(4, 0));
  TEST_ASSERT_TRUE(b.pixel(5, 0));
  TEST_ASSERT_FALSE(b.pixel(11, 1));
}

void test_differences_widen_to_whole_bytes_and_equal_pictures_have_none() {
  Bitmap1 a(240, 320), b(240, 320);
  ui::Rect r;
  TEST_ASSERT_FALSE(a.differences(b, r));
  b.setPixel(9, 5, true);
  TEST_ASSERT_TRUE(a.differences(b, r));
  TEST_ASSERT_TRUE(r == (ui::Rect{8, 5, 8, 1}));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_without_a_profile_the_panel_is_not_driven_and_says_so);
  RUN_TEST(test_a_profile_of_the_wrong_size_or_without_values_is_not_used);
  RUN_TEST(test_begin_does_not_touch_the_panel_and_a_refresh_resets_and_starts_it_in_order);
  RUN_TEST(test_every_refresh_wakes_the_panel_again);
  RUN_TEST(test_a_panel_that_stays_busy_after_power_on_is_said);
  RUN_TEST(test_a_tiny_change_is_sent_in_the_smallest_window_the_profile_allows);
  RUN_TEST(test_commands_go_out_with_dc_low_and_data_with_dc_high_inside_a_chip_select);
  RUN_TEST(test_the_first_frame_is_a_full_refresh_with_old_and_new_data_whole);
  RUN_TEST(test_an_unchanged_frame_sends_nothing);
  RUN_TEST(test_a_small_change_is_a_partial_refresh_of_exactly_its_rectangle);
  RUN_TEST(test_a_change_over_several_rows_and_columns_sends_the_bounding_box);
  RUN_TEST(test_every_tenth_change_is_a_full_refresh_by_the_policy);
  RUN_TEST(test_a_picture_that_stays_for_four_hours_is_refreshed_in_full_once);
  RUN_TEST(test_a_window_the_panel_cannot_address_becomes_a_full_refresh);
  RUN_TEST(test_the_polarity_of_the_data_follows_the_profile);
  RUN_TEST(test_a_panel_that_stays_busy_is_said_and_the_next_frame_is_full);
  RUN_TEST(test_the_picture_is_not_rotated_and_drawing_outside_is_dropped);
  RUN_TEST(test_a_bitmap_draws_only_its_set_bits);
  RUN_TEST(test_differences_widen_to_whole_bytes_and_equal_pictures_have_none);
  return UNITY_END();
}
