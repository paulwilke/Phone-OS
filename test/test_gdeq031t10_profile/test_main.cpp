// The GDEQ031T10 profile against the sequences the panel's documentation and the vendor's demo give.
//
// **The expected sequences below are written down here, in this file's own notation, as what the panel is
// documented to receive** (docs/hardware/device-display.md names the documents): "C:xx" is a command byte, "D:xx .." the
// data bytes after it, "D[n]" a picture of n bytes (its content is checked on its own). A change of a single
// value of the profile changes the traffic and fails the test of the sequence it belongs to; each of those
// tests has a counter-check that makes such a change and requires the comparison to fail. What this does not
// prove is that the panel shows a picture: that is the owner's check with the device (docs/hardware/bring-up.md).

#include <unity.h>

#include <string>
#include <vector>

#include <board/EinkDisplay.h>
#include <board/GDEQ031T10Profile.h>
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

struct Run {
  FakeDelay delay;
  FakeGpio gpio{&delay};
  FakeSpiPort spi{gpio};
  FakeMillis millis;
  FakeEventLog log;
  EpdWire wire{spi, gpio, delay};
  PanelProfile profile;
  EinkDisplay display;

  explicit Run(const PanelProfile& p) : profile(p), display(wire, &profile, millis, log) {
    gpio.setInput(pin::kDisplayBusy, true);  // free: on this panel low is busy
    display.begin();
  }

  void frame(void (*draw)(ui::Display&)) {
    display.beginFrame();
    draw(display);
    display.endFrame();
  }

  // The bytes on the wire from write number `from` on, "C:00 D:1F C:04 ...", a picture as "D[n]".
  std::string trace(size_t from = 0) const {
    std::string s;
    const auto flat = spi.flatten();
    for (size_t i = from; i < flat.size(); ++i) {
      const auto& w = spi.writes()[i];
      if (w.dcHigh && w.bytes.size() > 16) s += "D[" + std::to_string(w.bytes.size()) + "]";
      else s += flat[i];
      s += " ";
    }
    if (!s.empty()) s.pop_back();
    return s;
  }

  // The data of the partial window command (the line after "C:90"), or "".
  std::string window() const {
    const auto flat = spi.flatten();
    for (size_t i = 0; i + 1 < flat.size(); ++i)
      if (flat[i] == "C:90") return flat[i + 1];
    return "";
  }

  int busyLines() const {
    int n = 0;
    for (const auto& l : log.lines())
      if (l.find("stayed busy after a command") != std::string::npos) ++n;
    return n;
  }
};

PanelProfile mutated(void (*change)(PanelProfile&)) {
  PanelProfile p = gdeq031t10Profile();
  change(p);
  return p;
}

void blackLine(ui::Display& d) { d.fillRect({0, 0, 240, 1}, true); }
void blackLineThenDot(ui::Display& d) {
  d.fillRect({0, 0, 240, 1}, true);
  d.fillRect({40, 100, 3, 3}, true);  // x 40..42 is in byte 5 of rows 100..102
}
void blackLineAndPixelAtLeft(ui::Display& d) {
  d.fillRect({0, 0, 240, 1}, true);
  d.setPixel(0, 5, true);
}

// ---- The documented sequences ----

// Hardware reset (low 10 ms, high 10 ms), panel setting 0x1F, power on, wait for BUSY.
const char* kStart = "C:00 D:1F C:04";
// Power off, wait for BUSY, 100 ms, deep sleep with its check code.
const char* kSleep = "C:02 C:07 D:A5";
// Full refresh: the start, the old picture (all white), the new one, refresh, the sleep.
const char* kFull = "C:00 D:1F C:04 C:10 D[9600] C:13 D[9600] C:12 C:02 C:07 D:A5";
// Partial refresh of the dot of blackLineThenDot: the start, temperature override (0xE0 bit 1, 0xE5 = 121 degrees),
// the data interval at its default, partial in, the window (x 32..47, y 100..102, scan flag 1), the old picture
// of the window (white), the new one (the dot's three pixels are the low bits' zeroes of byte 5: 0x1F), refresh,
// the sleep.
const char* kPartial =
    "C:00 D:1F C:04 C:E0 D:02 C:E5 D:79 C:50 D:D7 C:91 C:90 D:20 2F 00 64 00 66 01 "
    "C:10 D:FF FF FF FF FF FF C:13 D:FF 1F FF 1F FF 1F C:12 C:02 C:07 D:A5";

bool endsWith(const std::string& s, const std::string& tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}
bool startsWith(const std::string& s, const std::string& head) { return s.compare(0, head.size(), head) == 0; }

// A partial refresh after a first full one; the traffic of the second frame only.
std::string partialTrace(Run& r, void (*second)(ui::Display&) = blackLineThenDot) {
  r.frame(blackLine);
  const size_t first = r.spi.writes().size();
  r.frame(second);
  return r.trace(first);
}

}  // namespace

// ---------- the start ----------

void test_the_start_is_panel_setting_then_power_on_after_a_reset_of_10_ms_low_and_10_ms_high() {
  Run r(gdeq031t10Profile());
  r.frame(blackLine);
  TEST_ASSERT_TRUE(startsWith(r.trace(), kStart));
  uint32_t fell = 0, rose = 0;
  for (const auto& e : r.gpio.events())
    if (e.pin == pin::kDisplayReset) (e.high ? rose : fell) = e.atMs;
  TEST_ASSERT_EQUAL(10, rose - fell);
  TEST_ASSERT_EQUAL(10, rose);  // the line rose at 10 ms and the start followed 10 ms later
}

void test_counter_the_flow_chart_value_or_a_missing_power_on_changes_the_start() {
  Run a(mutated([](PanelProfile& p) { p.start[0].data[0] = 0x0F; }));  // the datasheet flow chart's value
  a.frame(blackLine);
  TEST_ASSERT_FALSE(startsWith(a.trace(), kStart));
  Run b(mutated([](PanelProfile& p) { p.start.pop_back(); }));
  b.frame(blackLine);
  TEST_ASSERT_FALSE(startsWith(b.trace(), kStart));
}

void test_the_start_waits_for_busy_after_power_on() {
  Run r(gdeq031t10Profile());
  r.gpio.setInput(pin::kDisplayBusy, false);  // busy, and stays so
  r.frame(blackLine);
  TEST_ASSERT_EQUAL(2, r.busyLines());  // after power on, and after power off
  TEST_ASSERT_TRUE(r.log.contains("stayed busy after a refresh"));
}

void test_counter_without_the_wait_after_power_on_the_hung_panel_is_not_noticed_there() {
  Run r(mutated([](PanelProfile& p) { p.start[1].waitBusy = false; }));
  r.gpio.setInput(pin::kDisplayBusy, false);
  r.frame(blackLine);
  TEST_ASSERT_EQUAL(1, r.busyLines());
}

// ---------- the full refresh ----------

void test_a_full_refresh_is_the_start_both_pictures_the_refresh_and_the_sleep() {
  Run r(gdeq031t10Profile());
  r.frame(blackLine);
  TEST_ASSERT_EQUAL_STRING(kFull, r.trace().c_str());
  TEST_ASSERT_EQUAL(1, r.display.stats().full);
}

void test_the_pictures_of_a_full_refresh_are_old_white_and_new_black_as_zero_row_by_row() {
  Run r(gdeq031t10Profile());
  r.frame(blackLine);
  const auto& old = r.spi.writes()[4].bytes;
  const auto& fresh = r.spi.writes()[6].bytes;
  TEST_ASSERT_EQUAL(9600, (int)old.size());
  for (int i = 0; i < 9600; ++i) TEST_ASSERT_EQUAL_HEX8(0xFF, old[i]);  // nothing shown yet: white is 1
  for (int i = 0; i < 30; ++i) TEST_ASSERT_EQUAL_HEX8(0x00, fresh[i]);  // row 0: 30 bytes of black
  TEST_ASSERT_EQUAL_HEX8(0xFF, fresh[30]);                              // row 1 is white
}

void test_counter_a_temperature_override_in_a_full_refresh_or_no_sleep_changes_the_sequence() {
  Run a(mutated([](PanelProfile& p) { p.beforeFull = {{0xE0, {0x02}}}; }));
  a.frame(blackLine);
  TEST_ASSERT_TRUE(a.trace() != kFull);
  Run b(mutated([](PanelProfile& p) { p.sleep.pop_back(); }));
  b.frame(blackLine);
  TEST_ASSERT_TRUE(b.trace() != kFull);
  Run c(mutated([](PanelProfile& p) { p.blackIsZero = false; }));
  c.frame(blackLine);
  TEST_ASSERT_EQUAL_HEX8(0x00, c.spi.writes()[4].bytes[0]);  // the old picture is no longer white
}

void test_the_refresh_command_is_followed_by_one_ms_and_the_sleep_by_a_pause_of_100_ms() {
  Run r(gdeq031t10Profile());
  r.frame(blackLine);
  TEST_ASSERT_EQUAL(10 + 10 + 1 + 100, r.delay.elapsedMs());  // nothing else costs time while BUSY is free
}

void test_counter_without_the_one_ms_or_the_100_ms_the_time_is_shorter() {
  Run a(mutated([](PanelProfile& p) { p.refreshSettleMs = 0; }));
  a.frame(blackLine);
  TEST_ASSERT_EQUAL(120, a.delay.elapsedMs());
  Run b(mutated([](PanelProfile& p) { p.sleep[0].delayAfterMs = 0; }));
  b.frame(blackLine);
  TEST_ASSERT_EQUAL(21, b.delay.elapsedMs());
}

// ---------- the partial refresh ----------

void test_a_partial_refresh_overrides_the_temperature_then_gives_the_window_then_both_pictures() {
  Run r(gdeq031t10Profile());
  TEST_ASSERT_EQUAL_STRING(kPartial, partialTrace(r).c_str());
  TEST_ASSERT_EQUAL(1, r.display.stats().partial);
}

void test_counter_another_temperature_a_missing_override_or_another_interval_changes_the_partial() {
  Run a(mutated([](PanelProfile& p) { p.beforePartial[1].data[0] = 0x6E; }));  // the 1.5 s mode's value
  TEST_ASSERT_TRUE(partialTrace(a) != kPartial);
  Run b(mutated([](PanelProfile& p) { p.beforePartial.erase(p.beforePartial.begin()); }));  // no 0xE0
  TEST_ASSERT_TRUE(partialTrace(b) != kPartial);
  Run c(mutated([](PanelProfile& p) { p.beforePartial[2].data[0] = 0x97; }));
  TEST_ASSERT_TRUE(partialTrace(c) != kPartial);
  Run d(mutated([](PanelProfile& p) { p.encodeWindow = [](const ui::Rect&) { return std::vector<uint8_t>(); }; }));
  TEST_ASSERT_TRUE(partialTrace(d) != kPartial);  // a window that cannot be sent: a full refresh
}

void test_the_window_is_at_least_16_by_2_and_starts_on_a_byte_border() {
  Run r(gdeq031t10Profile());
  partialTrace(r, blackLineAndPixelAtLeft);
  // The pixel is in byte 0 of row 5; the window grows to two bytes and two rows.
  TEST_ASSERT_EQUAL_STRING("D:00 0F 00 05 00 06 01", r.window().c_str());
}

void test_the_window_moves_in_at_the_edges_of_the_screen() {
  Run r(gdeq031t10Profile());
  r.frame([](ui::Display&) {});
  r.frame([](ui::Display& d) { d.setPixel(239, 319, true); });
  // Bottom right corner: x 224..239, rows 318..319 (0x13E, 0x13F).
  TEST_ASSERT_EQUAL_STRING("D:E0 EF 01 3E 01 3F 01", r.window().c_str());
}

void test_counter_without_the_minimum_the_window_would_be_one_byte_and_one_row() {
  Run r(mutated([](PanelProfile& p) { p.minWindowWidth = 8; p.minWindowHeight = 1; }));
  partialTrace(r, blackLineAndPixelAtLeft);
  TEST_ASSERT_EQUAL_STRING("D:00 07 00 05 00 05 01", r.window().c_str());  // end equals start: the controller forbids it
}

void test_the_window_encoding_refuses_what_the_panel_cannot_address() {
  const PanelProfile& p = gdeq031t10Profile();
  TEST_ASSERT_TRUE(p.encodeWindow({3, 0, 16, 2}).empty());    // not on a byte border
  TEST_ASSERT_TRUE(p.encodeWindow({0, 0, 12, 2}).empty());    // not a whole number of bytes
  TEST_ASSERT_TRUE(p.encodeWindow({232, 0, 16, 2}).empty());  // past the right edge
  TEST_ASSERT_TRUE(p.encodeWindow({0, 319, 16, 2}).empty());  // past the bottom
  TEST_ASSERT_TRUE(p.encodeWindow({0, 0, 0, 2}).empty());
  TEST_ASSERT_EQUAL(7, (int)p.encodeWindow({0, 0, 240, 320}).size());
}

// ---------- the sleep ----------

void test_every_refresh_ends_with_power_off_then_deep_sleep_with_the_check_code() {
  Run full(gdeq031t10Profile());
  full.frame(blackLine);
  TEST_ASSERT_TRUE(endsWith(full.trace(), kSleep));
  Run partial(gdeq031t10Profile());
  TEST_ASSERT_TRUE(endsWith(partialTrace(partial), kSleep));
}

void test_counter_another_check_code_or_a_missing_power_off_changes_the_sleep() {
  Run a(mutated([](PanelProfile& p) { p.sleep[1].data[0] = 0xA4; }));
  a.frame(blackLine);
  TEST_ASSERT_FALSE(endsWith(a.trace(), kSleep));
  Run b(mutated([](PanelProfile& p) { p.sleep.erase(p.sleep.begin()); }));
  b.frame(blackLine);
  TEST_ASSERT_FALSE(endsWith(b.trace(), kSleep));
}

void test_the_sleep_waits_for_busy_after_power_off() {
  Run r(mutated([](PanelProfile& p) { p.sleep[0].waitBusy = false; }));
  r.gpio.setInput(pin::kDisplayBusy, false);
  r.frame(blackLine);
  TEST_ASSERT_EQUAL(1, r.busyLines());  // only the power on is waited for
}

// ---------- the whole profile ----------

void test_the_profile_is_240_by_320_busy_is_low_white_is_one_and_a_hung_panel_gives_up_after_ten_seconds() {
  const PanelProfile& p = gdeq031t10Profile();
  TEST_ASSERT_EQUAL(240, p.width);
  TEST_ASSERT_EQUAL(320, p.height);
  TEST_ASSERT_FALSE(p.busyIsHigh);
  TEST_ASSERT_EQUAL(10000, p.busyTimeoutMs);
  TEST_ASSERT_TRUE(p.blackIsZero);
  Run r(p);
  TEST_ASSERT_TRUE(r.display.configured());
}

// A wrong power or booster value can damage the panel; the vendor's demo sends none, and neither does this.
void test_the_profile_sends_no_power_booster_resolution_or_waveform_command() {
  const PanelProfile& p = gdeq031t10Profile();
  std::vector<PanelCommand> all = p.start;
  for (const auto* list : {&p.beforeFull, &p.beforePartial, &p.sleep}) all.insert(all.end(), list->begin(), list->end());
  for (const PanelCommand& c : all) {
    TEST_ASSERT_TRUE(c.command != 0x01 && c.command != 0x06 && c.command != 0x61);
    TEST_ASSERT_FALSE(c.command >= 0x20 && c.command <= 0x26);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_start_is_panel_setting_then_power_on_after_a_reset_of_10_ms_low_and_10_ms_high);
  RUN_TEST(test_counter_the_flow_chart_value_or_a_missing_power_on_changes_the_start);
  RUN_TEST(test_the_start_waits_for_busy_after_power_on);
  RUN_TEST(test_counter_without_the_wait_after_power_on_the_hung_panel_is_not_noticed_there);
  RUN_TEST(test_a_full_refresh_is_the_start_both_pictures_the_refresh_and_the_sleep);
  RUN_TEST(test_the_pictures_of_a_full_refresh_are_old_white_and_new_black_as_zero_row_by_row);
  RUN_TEST(test_counter_a_temperature_override_in_a_full_refresh_or_no_sleep_changes_the_sequence);
  RUN_TEST(test_the_refresh_command_is_followed_by_one_ms_and_the_sleep_by_a_pause_of_100_ms);
  RUN_TEST(test_counter_without_the_one_ms_or_the_100_ms_the_time_is_shorter);
  RUN_TEST(test_a_partial_refresh_overrides_the_temperature_then_gives_the_window_then_both_pictures);
  RUN_TEST(test_counter_another_temperature_a_missing_override_or_another_interval_changes_the_partial);
  RUN_TEST(test_the_window_is_at_least_16_by_2_and_starts_on_a_byte_border);
  RUN_TEST(test_the_window_moves_in_at_the_edges_of_the_screen);
  RUN_TEST(test_counter_without_the_minimum_the_window_would_be_one_byte_and_one_row);
  RUN_TEST(test_the_window_encoding_refuses_what_the_panel_cannot_address);
  RUN_TEST(test_every_refresh_ends_with_power_off_then_deep_sleep_with_the_check_code);
  RUN_TEST(test_counter_another_check_code_or_a_missing_power_off_changes_the_sleep);
  RUN_TEST(test_the_sleep_waits_for_busy_after_power_off);
  RUN_TEST(test_the_profile_is_240_by_320_busy_is_low_white_is_one_and_a_hung_panel_gives_up_after_ten_seconds);
  RUN_TEST(test_the_profile_sends_no_power_booster_resolution_or_waveform_command);
  return UNITY_END();
}
