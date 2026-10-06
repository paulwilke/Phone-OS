// The receiver on the board: what the contract cannot say about a real UART — a receiver that goes silent, a
// restart that nobody saw, a stream that never stops, and the rules of the NMEA reader seen from the outside.

#include <unity.h>

#include <cmath>

#include <BoardPowerRig.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
void feed(FakeUartPort& uart, const std::string& body) {
  unsigned char sum = 0;
  for (char c : body) sum ^= (unsigned char)c;
  char tail[8];
  std::snprintf(tail, sizeof(tail), "*%02X\r\n", sum);
  uart.feed("$" + body + tail);
}
}  // namespace

void test_a_fix_that_is_not_refreshed_for_ten_seconds_is_no_longer_valid() {
  BoardGnssRig r;
  r.scriptFix(48.13745, 11.57550, 1.5f, 2);
  r.power().setEnabled(PowerDomain::Positioning, true);
  GnssFix fix;
  r.advanceSeconds(5);
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_TRUE(fix.valid);
  r.receiver.silence(true);  // the wire is dead; the parser still remembers the last position
  r.advanceSeconds(10);
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_TRUE(fix.valid);  // ten seconds exactly: not yet
  r.advanceSeconds(1);
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_FALSE(fix.valid);
}

void test_a_silent_receiver_is_running_without_a_position_from_the_start() {
  BoardGnssRig r;
  r.receiver.silence(true);
  r.power().setEnabled(PowerDomain::Positioning, true);
  GnssFix fix;
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_FALSE(fix.valid);
  r.advanceSeconds(60);
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_FALSE(fix.valid);
}

void test_a_fix_returns_when_the_receiver_speaks_again() {
  BoardGnssRig r;
  r.scriptFix(48.13745, 11.57550, 1.5f, 1);
  r.power().setEnabled(PowerDomain::Positioning, true);
  r.advanceSeconds(3);
  GnssFix fix;
  r.gnss().readFix(fix);
  r.receiver.silence(true);
  r.advanceSeconds(30);
  r.gnss().readFix(fix);
  TEST_ASSERT_FALSE(fix.valid);
  r.receiver.silence(false);
  r.advanceSeconds(1);
  r.gnss().readFix(fix);
  TEST_ASSERT_TRUE(fix.valid);
}

void test_the_numbers_of_a_valid_fix_are_those_of_the_sentences() {
  BoardGnssRig r;
  r.scriptFix(48.13745, 11.57550, 1.25f, 1);
  r.power().setEnabled(PowerDomain::Positioning, true);
  r.advanceSeconds(3);
  GnssFix fix;
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_TRUE(fix.valid);
  TEST_ASSERT_TRUE(std::fabs(fix.lat - 48.13745) < 0.0001);
  TEST_ASSERT_TRUE(std::fabs(fix.lon - 11.57550) < 0.0001);
  TEST_ASSERT_TRUE(std::fabs(fix.hdop - 1.25f) < 0.01f);
  TEST_ASSERT_EQUAL(9, fix.sats);
  TEST_ASSERT_TRUE(fix.utc >= Clock::kEarliestEpoch);
}

// The old-run sentences left in the UART must not come back as the new run's position.
void test_sentences_of_an_earlier_run_that_still_wait_at_the_uart_are_thrown_away() {
  BoardGnssRig r;
  r.scriptFix(48.13745, 11.57550, 1.5f, 1);
  r.power().setEnabled(PowerDomain::Positioning, true);
  r.advanceSeconds(4);
  GnssFix fix;
  r.gnss().readFix(fix);
  TEST_ASSERT_TRUE(fix.valid);
  r.advanceSeconds(3);  // the UART holds three seconds of the first run
  r.power().setEnabled(PowerDomain::Positioning, false);
  r.power().setEnabled(PowerDomain::Positioning, true);
  r.receiver.silence(true);  // the new run has said nothing yet
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_FALSE(fix.valid);
}

// "nan" in the HDOP field of a receiver's sentence reaches nobody as a NaN.
void test_a_nan_hdop_from_the_receiver_reaches_the_caller_as_a_number() {
  BoardGnssRig r;
  r.power().setEnabled(PowerDomain::Positioning, true);
  feed(r.uart, "GNGGA,123210.00,4808.2470,N,01134.5300,E,1,09,nan,140.0,M,47.0,M,,");
  feed(r.uart, "GNRMC,123210.00,A,4808.2470,N,01134.5300,E,0.03,0.00,160926,,,A");
  GnssFix fix;
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_TRUE(fix.valid);
  TEST_ASSERT_TRUE(std::isfinite(fix.hdop));
  TEST_ASSERT_EQUAL_FLOAT(99.0f, fix.hdop);
}

void test_a_stream_that_never_stops_does_not_hold_the_loop() {
  BoardGnssRig r;
  r.power().setEnabled(PowerDomain::Positioning, true);
  r.uart.feed(std::string(100000, 'x'));
  GnssFix fix;
  TEST_ASSERT_TRUE(r.gnss().readFix(fix));
  TEST_ASSERT_EQUAL(100000 - kMaxBytesPerRead, r.uart.pending());
}

void test_a_receiver_that_is_off_does_not_touch_the_uart() {
  BoardGnssRig r;
  r.uart.feed("$GNRMC");
  GnssFix fix;
  TEST_ASSERT_FALSE(r.gnss().readFix(fix));
  TEST_ASSERT_EQUAL(6, r.uart.pending());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_fix_that_is_not_refreshed_for_ten_seconds_is_no_longer_valid);
  RUN_TEST(test_a_silent_receiver_is_running_without_a_position_from_the_start);
  RUN_TEST(test_a_fix_returns_when_the_receiver_speaks_again);
  RUN_TEST(test_the_numbers_of_a_valid_fix_are_those_of_the_sentences);
  RUN_TEST(test_sentences_of_an_earlier_run_that_still_wait_at_the_uart_are_thrown_away);
  RUN_TEST(test_a_nan_hdop_from_the_receiver_reaches_the_caller_as_a_number);
  RUN_TEST(test_a_stream_that_never_stops_does_not_hold_the_loop);
  RUN_TEST(test_a_receiver_that_is_off_does_not_touch_the_uart);
  return UNITY_END();
}
