#pragma once

// The contract of MessageLog. Rig: `MessageLog& log()`,
//   bool failNextAppend()     the next append fails and writes nothing; false if not possible
//   bool failAllAppends(bool) every append fails (a full or missing card); false if not possible

#include <string>
#include <vector>

#include <hal/MessageLog.h>

#include "Contract.h"

namespace message_log_contract {
struct Collect : MessageLog::Visitor {
  std::vector<std::string> lines;
  void line(const char* text) override { lines.push_back(text); }
};
}  // namespace message_log_contract

template <class Rig>
struct MessageLogContract {
  static std::vector<std::string> all(Rig& rig) {
    message_log_contract::Collect c;
    TEST_ASSERT_TRUE(rig.log().readAll(c));
    return c.lines;
  }

  static void there_is_no_history_before_the_first_line() {
    Rig rig;
    message_log_contract::Collect c;
    TEST_ASSERT_FALSE(rig.log().readAll(c));
    TEST_ASSERT_EQUAL(0, c.lines.size());
  }

  static void lines_come_back_in_order_without_line_breaks() {
    Rig rig;
    for (int i = 0; i < 300; ++i) TEST_ASSERT_TRUE(rig.log().append(("line " + std::to_string(i)).c_str()));
    const auto lines = all(rig);
    TEST_ASSERT_EQUAL(300, lines.size());
    for (int i = 0; i < 300; ++i) TEST_ASSERT_EQUAL_STRING(("line " + std::to_string(i)).c_str(), lines[i].c_str());
  }

  static void reading_twice_gives_the_same_lines() {
    Rig rig;
    rig.log().append("a");
    rig.log().append("b");
    TEST_ASSERT_TRUE(all(rig) == all(rig));
  }

  static void a_line_with_a_break_stays_one_line() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.log().append("first\nsecond\r\nthird"));
    TEST_ASSERT_TRUE(rig.log().append("next"));
    const auto lines = all(rig);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_TRUE(lines[0].find("first") != std::string::npos);
    TEST_ASSERT_TRUE(lines[0].find("third") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("next", lines[1].c_str());
  }

  static void empty_lines_are_not_handed_over() {
    Rig rig;
    rig.log().append("a");
    rig.log().append("");
    rig.log().append("b");
    const auto lines = all(rig);
    TEST_ASSERT_EQUAL(2, lines.size());
  }

  static void tabs_and_non_ascii_text_are_kept() {
    Rig rig;
    rig.log().append("M\t1\t0\ts\ti\t+491701111111\tGr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC");
    TEST_ASSERT_EQUAL_STRING("M\t1\t0\ts\ti\t+491701111111\tGr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC", all(rig)[0].c_str());
  }

  // A line that could not be written is reported, and what was written before and after is intact. A card
  // that failed in the middle of a line may have left a piece of it: that piece is a line of its own and
  // never joins the next one.
  static void a_failed_append_is_reported_and_loses_nothing_before_it() {
    Rig rig;
    rig.log().append("one");
    CONTRACT_NEEDS(rig.failNextAppend());
    TEST_ASSERT_FALSE(rig.log().append("two"));
    TEST_ASSERT_TRUE(rig.log().append("three"));
    const auto lines = all(rig);
    TEST_ASSERT_TRUE(lines.size() == 2 || lines.size() == 3);
    TEST_ASSERT_EQUAL_STRING("one", lines.front().c_str());
    TEST_ASSERT_EQUAL_STRING("three", lines.back().c_str());
  }

  static void a_card_that_refuses_appends_changes_nothing() {
    Rig rig;
    rig.log().append("one");
    CONTRACT_NEEDS(rig.failAllAppends(true));
    TEST_ASSERT_FALSE(rig.log().append("two"));
    rig.failAllAppends(false);
    TEST_ASSERT_TRUE(rig.log().append("three"));
    const auto lines = all(rig);
    TEST_ASSERT_EQUAL_STRING("one", lines.front().c_str());
    TEST_ASSERT_EQUAL_STRING("three", lines.back().c_str());
  }
};

#define RUN_MESSAGE_LOG_CONTRACT(Rig)                                                   \
  RUN_TEST(MessageLogContract<Rig>::there_is_no_history_before_the_first_line);          \
  RUN_TEST(MessageLogContract<Rig>::lines_come_back_in_order_without_line_breaks);       \
  RUN_TEST(MessageLogContract<Rig>::reading_twice_gives_the_same_lines);                 \
  RUN_TEST(MessageLogContract<Rig>::a_line_with_a_break_stays_one_line);                 \
  RUN_TEST(MessageLogContract<Rig>::empty_lines_are_not_handed_over);                    \
  RUN_TEST(MessageLogContract<Rig>::tabs_and_non_ascii_text_are_kept);                   \
  RUN_TEST(MessageLogContract<Rig>::a_failed_append_is_reported_and_loses_nothing_before_it); \
  RUN_TEST(MessageLogContract<Rig>::a_card_that_refuses_appends_changes_nothing)
