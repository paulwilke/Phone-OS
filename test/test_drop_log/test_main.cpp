// Host tests for DropLog: what the device turns away is logged, once per sender per ten minutes, with a
// summary of the rest, and the first line carries the start of the text.
// allow-non-english: a text with accents is cut by characters, not by bytes.

#include <unity.h>

#include <string>

#include <ProfileBuilder.h>
#include <app/DropLog.h>
#include <app/LogLevel.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeEventLog.h>

void setUp() {}
void tearDown() {}

namespace {

constexpr const char* kCountry = "+49";
constexpr const char* kStranger = "+491709999999";

struct Rig {
  FakeClock clock;
  FakeEventLog log{&clock};
  Profile profile = ProfileBuilder().build();
  Logger logger{log, profile};
  uint32_t now = 1000;                              // the steady clock in seconds, by hand: it may be set back
  DropLog drops{logger, [this] { return now; }};

  size_t lines() const { return log.entries().size(); }
  const std::string& line(size_t i) const { return log.entries()[i].line; }
};

}  // namespace

// ---- once per sender per ten minutes --------------------------------------------------------------------

void test_a_flood_of_a_hundred_from_one_number_is_one_line_and_one_summary() {
  Rig r;
  for (int i = 0; i < 100; ++i) {
    r.drops.smsDropped(kStranger, "spam " + std::to_string(i), kCountry);
    r.now += 1;
  }
  TEST_ASSERT_EQUAL(1, r.lines());                                    // the first drop, and nothing else in 100 seconds
  TEST_ASSERT_TRUE(r.line(0).find("(+491709999999): spam 0") != std::string::npos);
  r.now += DropLog::kWindowSeconds;
  r.drops.tick();                                                     // the ten minutes are over: the summary
  TEST_ASSERT_EQUAL(2, r.lines());
  TEST_ASSERT_TRUE(r.line(1).find("(99 more in the last 10 minutes)") != std::string::npos);
  TEST_ASSERT_TRUE(r.line(1).find("(+491709999999)") != std::string::npos);
  r.drops.tick();
  r.now += 5000;
  r.drops.tick();
  TEST_ASSERT_EQUAL(2, r.lines());                                    // and the summary is written once
}

void test_the_summary_is_written_by_the_next_drop_when_nobody_ticks() {
  Rig r;
  for (int i = 0; i < 5; ++i) r.drops.smsDropped(kStranger, "x", kCountry);
  r.now += DropLog::kWindowSeconds + 1;
  r.drops.smsDropped(kStranger, "again", kCountry);
  TEST_ASSERT_EQUAL(3, r.lines());                                    // the first line, the summary, the new first line
  TEST_ASSERT_TRUE(r.line(1).find("(4 more in the last 10 minutes)") != std::string::npos);
  TEST_ASSERT_TRUE(r.line(2).find(": again") != std::string::npos);
}

void test_two_senders_are_counted_apart() {
  Rig r;
  for (int i = 0; i < 3; ++i) r.drops.smsDropped(kStranger, "a", kCountry);
  for (int i = 0; i < 7; ++i) r.drops.smsDropped("+4917012345678", "b", kCountry);
  TEST_ASSERT_EQUAL(2, r.lines());                                    // one line each
  r.now += DropLog::kWindowSeconds;
  r.drops.tick();
  TEST_ASSERT_EQUAL(4, r.lines());
  TEST_ASSERT_TRUE(r.log.contains("(+491709999999) ... (2 more in the last 10 minutes)"));
  TEST_ASSERT_TRUE(r.log.contains("(+4917012345678) ... (6 more in the last 10 minutes)"));
}

void test_one_number_in_three_spellings_is_one_sender_and_a_name_is_one_sender_by_its_text() {
  Rig r;
  r.drops.smsDropped("+491709999999", "a", kCountry);
  r.drops.smsDropped("0170 9999999", "b", kCountry);
  r.drops.smsDropped("0049 170 9999999", "c", kCountry);
  TEST_ASSERT_EQUAL(1, r.lines());
  r.drops.smsDropped("Provider", "d", kCountry);
  r.drops.smsDropped("Provider", "e", kCountry);
  r.drops.smsDropped("Operator", "f", kCountry);                      // another text, another sender
  TEST_ASSERT_EQUAL(3, r.lines());
}

void test_a_gap_of_more_than_ten_minutes_writes_a_line_again() {
  Rig r;
  r.drops.smsDropped(kStranger, "first", kCountry);
  r.now += DropLog::kWindowSeconds - 1;
  r.drops.smsDropped(kStranger, "inside", kCountry);
  TEST_ASSERT_EQUAL(1, r.lines());                                    // 599 seconds: still the window
  r.now += 2;
  r.drops.smsDropped(kStranger, "second", kCountry);
  TEST_ASSERT_TRUE(r.lines() >= 2);
  TEST_ASSERT_TRUE(r.line(r.lines() - 1).find(": second") != std::string::npos);   // and it has its text
}

void test_a_clock_that_jumps_back_does_not_stall_the_log() {
  Rig r;
  r.now = 100000;
  r.drops.smsDropped(kStranger, "before", kCountry);
  r.now = 50;                                                         // earlier than the last line
  r.drops.smsDropped(kStranger, "after", kCountry);
  TEST_ASSERT_EQUAL(2, r.lines());
  TEST_ASSERT_TRUE(r.line(1).find(": after") != std::string::npos);
  r.drops.smsDropped(kStranger, "and again", kCountry);                // the new window runs from there
  TEST_ASSERT_EQUAL(2, r.lines());
}

void test_the_table_is_bounded_and_a_flood_of_numbers_gets_a_line_each() {
  Rig r;
  for (size_t i = 0; i < DropLog::kMaxSenders * 3; ++i) r.drops.smsDropped("+49170" + std::to_string(1000000 + i), "x", kCountry);
  TEST_ASSERT_EQUAL(DropLog::kMaxSenders * 3, r.lines());              // every different sender is a sender
  // The first one was forgotten when the table was full: it gets a line at once, not ten minutes after its last.
  r.drops.smsDropped(std::string("+49170") + std::to_string(1000000), "x", kCountry);
  TEST_ASSERT_EQUAL(DropLog::kMaxSenders * 3 + 1, r.lines());
}

// ---- the text -----------------------------------------------------------------------------------------------

void test_the_first_line_has_the_text_and_nothing_else_of_the_message() {
  Rig r;
  r.drops.smsDropped(kStranger, "Your balance is 3.00 EUR", kCountry);
  TEST_ASSERT_EQUAL_STRING("SMS dropped: sender not in the contact book (+491709999999): Your balance is 3.00 EUR", r.line(0).c_str());
}

void test_control_characters_are_replaced_by_a_space() {
  Rig r;
  r.drops.smsDropped(kStranger, std::string("one\ntwo\tthree\r\nfour\x1b" "five\x7f" "six"), kCountry);
  TEST_ASSERT_EQUAL_STRING("SMS dropped: sender not in the contact book (+491709999999): one two three  four five six", r.line(0).c_str());
  TEST_ASSERT_TRUE(r.line(0).find('\n') == std::string::npos && r.line(0).find('\t') == std::string::npos);
}

void test_a_long_text_is_cut_at_a_hundred_and_twenty_characters() {
  Rig r;
  r.drops.smsDropped(kStranger, std::string(500, 'x'), kCountry);
  const std::string head = "SMS dropped: sender not in the contact book (+491709999999): ";
  TEST_ASSERT_EQUAL(head.size() + 120, r.line(0).size());
  // By characters and not by bytes: 200 two-byte letters give 120 letters, none of them cut in half.
  Rig q;
  std::string accents;
  for (int i = 0; i < 200; ++i) accents += "\xC3\xA4";
  q.drops.smsDropped(kStranger, accents, kCountry);
  TEST_ASSERT_EQUAL(head.size() + 240, q.line(0).size());
}

void test_an_empty_text_has_no_text_part() {
  Rig r;
  r.drops.smsDropped(kStranger, "", kCountry);
  TEST_ASSERT_EQUAL_STRING("SMS dropped: sender not in the contact book (+491709999999)", r.line(0).c_str());
}

void test_a_call_has_no_text_and_shares_the_window_with_the_texts_of_the_same_number() {
  Rig r;
  r.drops.callRejected("0170 9999999", kCountry);
  TEST_ASSERT_EQUAL_STRING("Call rejected: caller not in the contact book (+491709999999)", r.line(0).c_str());
  r.drops.smsDropped(kStranger, "text", kCountry);
  r.drops.callRejected(kStranger, kCountry);
  TEST_ASSERT_EQUAL(1, r.lines());                                    // one sender, one line in ten minutes
  r.now += DropLog::kWindowSeconds;
  r.drops.tick();
  TEST_ASSERT_TRUE(r.line(1).find("(2 more in the last 10 minutes)") != std::string::npos);
  r.drops.callRejected("", kCountry);
  TEST_ASSERT_EQUAL_STRING("Call rejected: number withheld", r.line(2).c_str());
}

// ---- no log level drops them ------------------------------------------------------------------------------

void test_no_line_is_ever_dropped_by_a_log_level() {
  for (const char* level : {"all", "events", "errors"}) {
    Rig r;
    r.profile.logLevel = loglevel::parse(level);
    r.drops.smsDropped(kStranger, "text", kCountry);
    r.drops.callRejected("+4917012345678", kCountry);
    r.drops.radioDropped("Node Two", "hi", "Node One");
    r.drops.smsDropped(kStranger, "text", kCountry);
    r.now += DropLog::kWindowSeconds;
    r.drops.tick();
    TEST_ASSERT_EQUAL_MESSAGE(4, r.lines(), level);                   // three first lines and a summary
    for (size_t i = 0; i < r.lines(); ++i) TEST_ASSERT_TRUE_MESSAGE(loglevel::classify(r.line(i).c_str()) == loglevel::Importance::Always, r.line(i).c_str());
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_flood_of_a_hundred_from_one_number_is_one_line_and_one_summary);
  RUN_TEST(test_the_summary_is_written_by_the_next_drop_when_nobody_ticks);
  RUN_TEST(test_two_senders_are_counted_apart);
  RUN_TEST(test_one_number_in_three_spellings_is_one_sender_and_a_name_is_one_sender_by_its_text);
  RUN_TEST(test_a_gap_of_more_than_ten_minutes_writes_a_line_again);
  RUN_TEST(test_a_clock_that_jumps_back_does_not_stall_the_log);
  RUN_TEST(test_the_table_is_bounded_and_a_flood_of_numbers_gets_a_line_each);
  RUN_TEST(test_the_first_line_has_the_text_and_nothing_else_of_the_message);
  RUN_TEST(test_control_characters_are_replaced_by_a_space);
  RUN_TEST(test_a_long_text_is_cut_at_a_hundred_and_twenty_characters);
  RUN_TEST(test_an_empty_text_has_no_text_part);
  RUN_TEST(test_a_call_has_no_text_and_shares_the_window_with_the_texts_of_the_same_number);
  RUN_TEST(test_no_line_is_ever_dropped_by_a_log_level);
  return UNITY_END();
}
