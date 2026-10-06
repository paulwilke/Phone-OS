// The NMEA reader. The sentences are built to NMEA 0183 and their checksums are computed in the test, so an
// example is easy to change. The rules at the bottom are those of the capability (hal/GnssFix.h): a valid
// fix has sound numbers and an invalid one has none.

#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <drivers/NmeaParser.h>

namespace {

// "$" + body + "*XX\r\n"
std::string sentence(const std::string& body) {
  uint8_t sum = 0;
  for (char c : body) sum ^= (uint8_t)c;
  char tail[8];
  snprintf(tail, sizeof(tail), "*%02X\r\n", sum);
  return "$" + body + tail;
}

void feed(NmeaParser& parser, const std::string& text) {
  for (char c : text) parser.feed(c);
}

// The example place, 2026-09-16 12:32:10 UTC.
const char* kRmc = "GNRMC,123210.00,A,4808.2470,N,01134.5300,E,0.03,0.00,160926,,,A";
const char* kGga = "GNGGA,123210.00,4808.2470,N,01134.5300,E,1,09,1.20,140.0,M,47.0,M,,";

std::string gga(const char* hdop, const char* quality = "1", const char* sats = "09") {
  return std::string("GNGGA,123210.00,4808.2470,N,01134.5300,E,") + quality + "," + sats + "," + hdop + ",140.0,M,47.0,M,,";
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_position_from_rmc_and_gga() {
  NmeaParser p;
  feed(p, sentence(kGga));
  feed(p, sentence(kRmc));
  const GnssFix& fix = p.fix();
  TEST_ASSERT_TRUE(fix.valid);
  TEST_ASSERT_TRUE(fabs(fix.lat - 48.13745) < 0.0002);
  TEST_ASSERT_TRUE(fabs(fix.lon - 11.57550) < 0.0002);
  TEST_ASSERT_EQUAL(9, fix.sats);
  TEST_ASSERT_TRUE(fabs(fix.hdop - 1.2f) < 0.01f);
  TEST_ASSERT_EQUAL_UINT32(1789561930, fix.utc);  // 2026-09-16 12:32:10 UTC
  TEST_ASSERT_EQUAL(0, p.rejected());
}

void test_southern_and_western_hemisphere() {
  NmeaParser p;
  feed(p, sentence("GNRMC,123210.00,A,3352.1000,S,15112.5000,W,0.0,0.0,160926,,,A"));
  const GnssFix& fix = p.fix();
  TEST_ASSERT_TRUE(fix.valid);
  TEST_ASSERT_TRUE(fabs(fix.lat + 33.868333) < 0.0002);
  TEST_ASSERT_TRUE(fabs(fix.lon + 151.208333) < 0.0002);
}

// The quality is read from GGA whichever sentence comes first, and applies to the fix at once.
void test_the_quality_of_a_fix_follows_the_next_gga() {
  NmeaParser p;
  feed(p, sentence(kRmc));
  TEST_ASSERT_EQUAL(0, p.fix().sats);
  TEST_ASSERT_TRUE(p.fix().hdop > 90.0f);  // unknown until GGA says
  feed(p, sentence(gga("0.80", "1", "12")));
  TEST_ASSERT_EQUAL(12, p.fix().sats);
  TEST_ASSERT_TRUE(fabs(p.fix().hdop - 0.8f) < 0.01f);
}

void test_no_fix_marks_the_fix_invalid_and_takes_its_numbers_away() {
  NmeaParser p;
  feed(p, sentence(kGga));
  feed(p, sentence(kRmc));
  TEST_ASSERT_TRUE(p.fix().valid);
  feed(p, sentence("GNRMC,123215.00,V,,,,,0.0,0.0,160926,,,N"));
  const GnssFix& fix = p.fix();
  TEST_ASSERT_FALSE(fix.valid);
  // "An invalid fix carries no usable numbers at all": nothing of yesterday's position is left to misuse.
  TEST_ASSERT_TRUE(fix.lat == 0 && fix.lon == 0);
  TEST_ASSERT_EQUAL_UINT32(0, fix.utc);
  TEST_ASSERT_EQUAL(0, fix.sats);
}

void test_gga_without_a_fix_invalidates() {
  NmeaParser p;
  feed(p, sentence(kRmc));
  feed(p, sentence("GNGGA,123215.00,,,,,0,00,99.99,,,,,,"));
  TEST_ASSERT_FALSE(p.fix().valid);
}

// --- the HDOP: a number that can be compared, or "unknown" -----------------------------------------

// atof("nan") is NaN, and a NaN compares false with everything: a fix with it is dropped further up
// without a word. The quality of such a fix is unknown, and unknown is the worst value.
void test_a_nan_hdop_is_unknown_and_never_nan() {
  const char* bad[] = {"nan", "NaN", "-nan", "inf", "-inf", "infinity", "", "abc", "1.2x", "-1.5", "0.00", "0"};
  for (const char* hdop : bad) {
    NmeaParser p;
    feed(p, sentence(kRmc));
    feed(p, sentence(gga(hdop)));
    TEST_ASSERT_TRUE_MESSAGE(p.fix().valid, hdop);
    TEST_ASSERT_TRUE_MESSAGE(std::isfinite(p.fix().hdop), hdop);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(99.0f, p.fix().hdop, hdop);
  }
}

void test_a_good_hdop_is_kept_and_a_later_bad_one_does_not_poison_it() {
  NmeaParser p;
  feed(p, sentence(gga("1.20")));
  feed(p, sentence(kRmc));
  TEST_ASSERT_TRUE(fabs(p.fix().hdop - 1.2f) < 0.01f);
  feed(p, sentence(gga("nan")));
  TEST_ASSERT_TRUE(std::isfinite(p.fix().hdop));
  TEST_ASSERT_EQUAL_FLOAT(99.0f, p.fix().hdop);
  feed(p, sentence(gga("2.50")));
  TEST_ASSERT_TRUE(fabs(p.fix().hdop - 2.5f) < 0.01f);
}

void test_the_number_of_satellites_cannot_overflow_a_byte() {
  NmeaParser p;
  feed(p, sentence(kRmc));
  feed(p, sentence(gga("1.0", "1", "999")));
  TEST_ASSERT_EQUAL(255, p.fix().sats);
  feed(p, sentence(gga("1.0", "1", "-4")));
  TEST_ASSERT_EQUAL(0, p.fix().sats);
}

// --- positions that cannot exist -------------------------------------------------------------------

void test_a_valid_fix_with_an_impossible_position_is_no_fix() {
  const char* bad[] = {
      "GNRMC,123210.00,A,9100.0000,N,01134.5300,E,0.0,0.0,160926,,,A",  // latitude 91
      "GNRMC,123210.00,A,4808.2470,N,18100.0000,E,0.0,0.0,160926,,,A",  // longitude 181
      "GNRMC,123210.00,A,4860.0000,N,01134.5300,E,0.0,0.0,160926,,,A",  // 60 minutes
      "GNRMC,123210.00,A,48nan.2470,N,01134.5300,E,0.0,0.0,160926,,,A",
      "GNRMC,123210.00,A,,N,01134.5300,E,0.0,0.0,160926,,,A",
      "GNRMC,123210.00,A,4808.2470,X,01134.5300,E,0.0,0.0,160926,,,A",  // no such hemisphere
      "GNRMC,123210.00,A,4808.2470,N,01134.5300,,0.0,0.0,160926,,,A",
  };
  for (const char* line : bad) {
    NmeaParser p;
    feed(p, sentence(kRmc));
    feed(p, sentence(line));
    TEST_ASSERT_FALSE_MESSAGE(p.fix().valid, line);
    TEST_ASSERT_TRUE_MESSAGE(p.rejected() >= 1, line);
  }
}

void test_the_poles_and_the_date_line_are_positions() {
  NmeaParser p;
  feed(p, sentence("GNRMC,123210.00,A,9000.0000,N,18000.0000,E,0.0,0.0,160926,,,A"));
  TEST_ASSERT_TRUE(p.fix().valid);
  TEST_ASSERT_TRUE(fabs(p.fix().lat - 90.0) < 1e-9);
  TEST_ASSERT_TRUE(fabs(p.fix().lon - 180.0) < 1e-9);
}

// --- the time ---------------------------------------------------------------------------------------

void test_a_date_that_is_no_date_leaves_no_fix() {
  const char* bad[] = {"GNRMC,123210.00,A,4808.2470,N,01134.5300,E,0.0,0.0,321326,,,A",  // day 32, month 13
                       "GNRMC,256210.00,A,4808.2470,N,01134.5300,E,0.0,0.0,160926,,,A",  // hour 25
                       "GNRMC,12ab10.00,A,4808.2470,N,01134.5300,E,0.0,0.0,160926,,,A",
                       "GNRMC,123210.00,A,4808.2470,N,01134.5300,E,0.0,0.0,16092,,,A"};
  for (const char* line : bad) {
    NmeaParser p;
    feed(p, sentence(line));
    TEST_ASSERT_FALSE_MESSAGE(p.fix().valid, line);
  }
}

// A receiver that has not learned the date reports a day long ago: the position stays, its time is unknown.
void test_an_implausible_date_gives_a_position_without_a_time() {
  NmeaParser p;
  feed(p, sentence("GNRMC,123210.00,A,4808.2470,N,01134.5300,E,0.0,0.0,060180,,,A"));  // 2080-01-06
  TEST_ASSERT_TRUE(p.fix().valid);
  TEST_ASSERT_EQUAL_UINT32(0, p.fix().utc);
  NmeaParser q;
  feed(q, sentence("GNRMC,123210.00,A,4808.2470,N,01134.5300,E,0.0,0.0,060105,,,A"));  // 2005-01-06
  TEST_ASSERT_TRUE(q.fix().valid);
  TEST_ASSERT_EQUAL_UINT32(0, q.fix().utc);
}

void test_a_leap_second_is_accepted() {
  NmeaParser p;
  feed(p, sentence("GNRMC,235960.00,A,4808.2470,N,01134.5300,E,0.0,0.0,311227,,,A"));
  TEST_ASSERT_TRUE(p.fix().valid);
  TEST_ASSERT_TRUE(p.fix().utc != 0);
}

// --- the stream ------------------------------------------------------------------------------------

void test_a_bad_checksum_is_rejected() {
  NmeaParser p;
  std::string bad = sentence(kRmc);
  bad[bad.size() - 3] = (bad[bad.size() - 3] == 'A') ? 'B' : 'A';
  feed(p, bad);
  TEST_ASSERT_FALSE(p.fix().valid);
  TEST_ASSERT_EQUAL(1, p.rejected());
  TEST_ASSERT_EQUAL(0, p.positionSentences());
}

void test_garbage_and_truncated_sentences_are_skipped() {
  NmeaParser p;
  feed(p, "noise without a dollar sign\r\n");
  feed(p, "$GNRMC,123210.00,A,4808.2470,N");  // broken off
  feed(p, sentence(kRmc));
  TEST_ASSERT_TRUE(p.fix().valid);
}

void test_an_overlong_sentence_is_dropped_and_the_reader_goes_on() {
  NmeaParser p;
  feed(p, "$GNRMC," + std::string(NmeaParser::kMaxSentence + 50, '9') + "*00\r\n");
  TEST_ASSERT_FALSE(p.fix().valid);
  TEST_ASSERT_TRUE(p.rejected() >= 1);
  feed(p, sentence(kRmc));
  TEST_ASSERT_TRUE(p.fix().valid);
}

void test_other_constellations_are_accepted() {
  NmeaParser p;
  feed(p, sentence("GPRMC,123210.00,A,4808.2470,N,01134.5300,E,0.0,0.0,160926,,,A"));
  TEST_ASSERT_TRUE(p.fix().valid);
}

void test_unknown_sentences_are_ignored() {
  NmeaParser p;
  feed(p, sentence("GNVTG,0.0,T,,M,0.03,N,0.06,K,A"));
  feed(p, sentence("GNGSV,3,1,11,02,45,123,30"));
  TEST_ASSERT_FALSE(p.fix().valid);
  TEST_ASSERT_EQUAL(0, p.rejected());
}

void test_the_receiver_is_heard_to_be_alive_by_its_position_sentences() {
  NmeaParser p;
  TEST_ASSERT_EQUAL(0, p.positionSentences());
  feed(p, sentence("GNRMC,123210.00,V,,,,,0.0,0.0,160926,,,N"));  // no fix, but the receiver talks
  feed(p, sentence(kRmc));
  feed(p, sentence(kGga));
  TEST_ASSERT_EQUAL(2, p.positionSentences());
}

void test_reset_forgets_everything() {
  NmeaParser p;
  feed(p, sentence(kGga));
  feed(p, sentence(kRmc));
  p.reset();
  TEST_ASSERT_FALSE(p.fix().valid);
  TEST_ASSERT_EQUAL(0, p.positionSentences());
  feed(p, sentence(kRmc));
  TEST_ASSERT_EQUAL(0, p.fix().sats);  // the satellites of the earlier run are not remembered either
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_position_from_rmc_and_gga);
  RUN_TEST(test_southern_and_western_hemisphere);
  RUN_TEST(test_the_quality_of_a_fix_follows_the_next_gga);
  RUN_TEST(test_no_fix_marks_the_fix_invalid_and_takes_its_numbers_away);
  RUN_TEST(test_gga_without_a_fix_invalidates);
  RUN_TEST(test_a_nan_hdop_is_unknown_and_never_nan);
  RUN_TEST(test_a_good_hdop_is_kept_and_a_later_bad_one_does_not_poison_it);
  RUN_TEST(test_the_number_of_satellites_cannot_overflow_a_byte);
  RUN_TEST(test_a_valid_fix_with_an_impossible_position_is_no_fix);
  RUN_TEST(test_the_poles_and_the_date_line_are_positions);
  RUN_TEST(test_a_date_that_is_no_date_leaves_no_fix);
  RUN_TEST(test_an_implausible_date_gives_a_position_without_a_time);
  RUN_TEST(test_a_leap_second_is_accepted);
  RUN_TEST(test_a_bad_checksum_is_rejected);
  RUN_TEST(test_garbage_and_truncated_sentences_are_skipped);
  RUN_TEST(test_an_overlong_sentence_is_dropped_and_the_reader_goes_on);
  RUN_TEST(test_other_constellations_are_accepted);
  RUN_TEST(test_unknown_sentences_are_ignored);
  RUN_TEST(test_the_receiver_is_heard_to_be_alive_by_its_position_sentences);
  RUN_TEST(test_reset_forgets_everything);
  return UNITY_END();
}
