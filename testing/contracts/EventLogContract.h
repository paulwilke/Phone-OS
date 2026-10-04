#pragma once

// The contract of EventLog. Rig: `EventLog& log()`, `std::vector<std::string> records()`
// (the lines written so far, as they would be read back, without the time stamp).

#include <string>
#include <vector>

#include <hal/EventLog.h>

#include "Contract.h"

template <class Rig>
struct EventLogContract {
  // The log cannot drop anything: every line given is written, in order.
  static void every_line_is_written_in_order() {
    Rig rig;
    for (int i = 0; i < 1000; ++i) rig.log().log("line " + std::to_string(i));
    const auto records = rig.records();
    TEST_ASSERT_EQUAL(1000, records.size());
    for (int i = 0; i < 1000; ++i) TEST_ASSERT_EQUAL_STRING(("line " + std::to_string(i)).c_str(), records[i].c_str());
  }

  static void one_call_is_one_record_even_with_line_breaks() {
    Rig rig;
    rig.log().log("first\nsecond\r\nthird");
    rig.log().log("next");
    const auto records = rig.records();
    TEST_ASSERT_EQUAL(2, records.size());
    TEST_ASSERT_TRUE(records[0].find('\n') == std::string::npos);
    TEST_ASSERT_TRUE(records[0].find('\r') == std::string::npos);
    TEST_ASSERT_TRUE(records[0].find("first") != std::string::npos);
    TEST_ASSERT_TRUE(records[0].find("third") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("next", records[1].c_str());
  }

  // Lines that must always be provable are given like any other and arrive.
  static void a_remote_query_line_arrives_like_any_other() {
    Rig rig;
    rig.log().log("COMMAND +491701111111 (Parent One) position -> executed");
    TEST_ASSERT_EQUAL(1, rig.records().size());
  }

  static void an_empty_line_and_a_long_line_are_records() {
    Rig rig;
    rig.log().log("");
    rig.log().log(std::string(2000, 'x'));
    const auto records = rig.records();
    TEST_ASSERT_EQUAL(2, records.size());
    TEST_ASSERT_EQUAL(2000, records[1].size());
  }

  static void non_ascii_text_is_kept() {
    Rig rig;
    rig.log().log("Telef\xC3\xB3nica \xE2\x82\xAC");
    TEST_ASSERT_EQUAL_STRING("Telef\xC3\xB3nica \xE2\x82\xAC", rig.records()[0].c_str());
  }
};

#define RUN_EVENT_LOG_CONTRACT(Rig)                                                    \
  RUN_TEST(EventLogContract<Rig>::every_line_is_written_in_order);                     \
  RUN_TEST(EventLogContract<Rig>::one_call_is_one_record_even_with_line_breaks);       \
  RUN_TEST(EventLogContract<Rig>::a_remote_query_line_arrives_like_any_other);         \
  RUN_TEST(EventLogContract<Rig>::an_empty_line_and_a_long_line_are_records);          \
  RUN_TEST(EventLogContract<Rig>::non_ascii_text_is_kept)
