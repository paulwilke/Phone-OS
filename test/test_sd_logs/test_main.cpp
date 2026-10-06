// The two logs on the card: the system log that is never allowed to lose a provable line, and the message
// history that the store reads back. A missing card, a full card, a card that fails halfway.

#include <unity.h>

#include <BoardStorageRigs.h>
#include <app/MessageStore.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
std::vector<std::string> fileLines(SdStack& s, const char* path) {
  std::vector<std::string> out;
  const std::string text = s.volume.file(path);
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    out.push_back(text.substr(at, end - at));
    at = end + 1;
  }
  return out;
}
}  // namespace

// --- the system log ----------------------------------------------------------------------------

void test_a_line_without_a_known_time_is_stamped_with_dashes_never_1970() {
  SdStack s;
  FakeClock clock;
  SdEventLog log(s.card, clock);
  log.log("Modem: started");
  TEST_ASSERT_EQUAL_STRING("- --:-- Modem: started", fileLines(s, kSystemLogPath)[0].c_str());
}

void test_a_line_with_a_known_time_has_the_epoch_and_the_local_time_of_day() {
  SdStack s;
  FakeClock clock;
  clock.setEpochUtc(1790000000u);  // 14:13:20 UTC
  clock.setTzOffsetMinutes(120);
  SdEventLog log(s.card, clock);
  log.log("Positioning: fix");
  TEST_ASSERT_EQUAL_STRING("1790000000 16:13 Positioning: fix", fileLines(s, kSystemLogPath)[0].c_str());
}

void test_a_zone_west_of_utc_wraps_the_day_correctly() {
  SdStack s;
  FakeClock clock;
  clock.setEpochUtc(1789948800u + 3600);  // 01:00 UTC on a day that begins at 1789948800
  clock.setTzOffsetMinutes(-720);          // UTC-12: 13:00 of the day before
  SdEventLog log(s.card, clock);
  log.log("x");
  TEST_ASSERT_TRUE(fileLines(s, kSystemLogPath)[0].find(" 13:00 x") != std::string::npos);
}

void test_every_line_goes_to_the_console_too() {
  SdStack s;
  FakeClock clock;
  FakeEventLog console;
  SdEventLog log(s.card, clock, &console);
  log.log("one");
  log.log("two");
  TEST_ASSERT_EQUAL(2, console.lines().size());
  TEST_ASSERT_EQUAL_STRING("two", console.lines()[1].c_str());
}

void test_without_a_card_the_lines_wait_and_are_written_in_order_when_it_comes() {
  SdStack s(false);
  s.volume.removeCard();
  s.card.begin(s.bootLog);
  FakeClock clock;
  SdEventLog log(s.card, clock);
  log.log("first");
  log.log("second");
  TEST_ASSERT_EQUAL(2, log.pending());
  s.volume.insertCard();
  s.card.begin(s.bootLog);
  log.log("third");
  TEST_ASSERT_EQUAL(0, log.pending());
  const auto lines = fileLines(s, kSystemLogPath);
  TEST_ASSERT_EQUAL(3, lines.size());
  TEST_ASSERT_TRUE(lines[0].find("first") != std::string::npos);
  TEST_ASSERT_TRUE(lines[2].find("third") != std::string::npos);
}

void test_more_than_64_waiting_lines_lose_the_oldest_and_say_how_many() {
  SdStack s(false);
  s.volume.removeCard();
  s.card.begin(s.bootLog);
  FakeClock clock;
  SdEventLog log(s.card, clock);
  for (int i = 0; i < 70; ++i) log.log("line " + std::to_string(i));
  TEST_ASSERT_EQUAL(64, log.pending());
  s.volume.insertCard();
  s.card.begin(s.bootLog);
  log.flush();
  const auto lines = fileLines(s, kSystemLogPath);
  TEST_ASSERT_EQUAL(65, lines.size());
  TEST_ASSERT_TRUE(lines[0].find("6 lines were lost") != std::string::npos);
  TEST_ASSERT_TRUE(lines[1].find("line 6") != std::string::npos);
  TEST_ASSERT_TRUE(lines[64].find("line 69") != std::string::npos);
}

// The last room is the configuration's: a log that ate it would make every setting unsaveable.
void test_below_32_kb_free_the_log_waits_and_the_configuration_keeps_its_room() {
  SdStack s;
  s.volume.setCapacity(100 * 1024);
  s.volume.put("/data.bin", std::string(80 * 1024, 'x'));  // 20 KB free
  FakeClock clock;
  SdEventLog log(s.card, clock);
  log.log("waiting");
  TEST_ASSERT_EQUAL(1, log.pending());
  TEST_ASSERT_FALSE(s.volume.has(kSystemLogPath));
  s.volume.setCapacity(1000000);
  log.flush();
  TEST_ASSERT_EQUAL(0, log.pending());
}

void test_the_log_is_rotated_at_256_kb_and_the_older_one_is_replaced() {
  SdStack s;
  s.volume.put(kSystemLogPath, std::string(kLogRotateBytes, 'x'));
  s.volume.put(kSystemLogOldPath, "older still");
  FakeClock clock;
  SdEventLog log(s.card, clock);
  log.log("after the rotation");
  TEST_ASSERT_EQUAL(kLogRotateBytes, (int64_t)s.volume.file(kSystemLogOldPath).size());
  TEST_ASSERT_EQUAL(1, fileLines(s, kSystemLogPath).size());
  TEST_ASSERT_TRUE(fileLines(s, kSystemLogPath)[0].find("after the rotation") != std::string::npos);
}

void test_a_card_that_fails_in_the_middle_of_a_line_keeps_the_line_and_never_joins_it_to_the_next() {
  SdStack s;
  FakeClock clock;
  SdEventLog log(s.card, clock);
  log.log("before");
  s.volume.failNextWrite();
  log.log("interrupted");
  TEST_ASSERT_EQUAL(1, log.pending());
  log.log("after");
  TEST_ASSERT_EQUAL(0, log.pending());
  const auto lines = fileLines(s, kSystemLogPath);
  bool before = false, interrupted = false, after = false;
  for (const auto& l : lines) {
    before = before || l == "- --:-- before";
    interrupted = interrupted || l == "- --:-- interrupted";
    after = after || l == "- --:-- after";
  }
  TEST_ASSERT_TRUE(before && interrupted && after);  // the line was written again whole; only a piece stayed behind
}

void test_a_line_break_in_a_line_stays_one_record() {
  SdStack s;
  FakeClock clock;
  SdEventLog log(s.card, clock);
  log.log("a\nb\r\nc");
  TEST_ASSERT_EQUAL(1, fileLines(s, kSystemLogPath).size());
}

// --- the message history -------------------------------------------------------------------------

namespace {
struct Collect : MessageLog::Visitor {
  std::vector<std::string> lines;
  void line(const char* t) override { lines.push_back(t); }
};
}  // namespace

void test_the_store_restores_what_it_wrote_through_the_card_log() {
  SdStack s;
  SdMessageLog log(s.card);
  MessageStore store;
  store.attach(&log);
  Message m;
  m.time = 1790000000u;
  m.source = MsgSource::Sms;
  m.dir = MsgDir::In;
  std::snprintf(m.peer, sizeof(m.peer), "+491701111111");
  std::snprintf(m.text, sizeof(m.text), "Pick us up");
  store.add(m);
  MessageStore again;
  again.attach(&log);
  TEST_ASSERT_TRUE(again.restore());
  TEST_ASSERT_EQUAL(1, again.count());
  TEST_ASSERT_EQUAL(0, again.damagedLines());
}

void test_a_cut_off_last_line_comes_back_as_a_damaged_line_not_as_a_message() {
  SdStack s;
  SdMessageLog log(s.card);
  s.volume.put(kMessageLogPath, "M\t1\t1790000000\ts\ti\t+491701111111\tfine\nM\t2\t1790000100\ts\ti\t+49170");
  MessageStore store;
  store.attach(&log);
  TEST_ASSERT_FALSE(store.restore());
  TEST_ASSERT_EQUAL(1, store.count());
  TEST_ASSERT_EQUAL(1, store.damagedLines());
}

void test_a_file_that_was_touched_on_windows_reads_the_same() {
  SdStack s;
  SdMessageLog log(s.card);
  s.volume.put(kMessageLogPath, "a\r\nb\r\n\r\nc");
  Collect c;
  TEST_ASSERT_TRUE(log.readAll(c));
  TEST_ASSERT_EQUAL(3, c.lines.size());
  TEST_ASSERT_EQUAL_STRING("c", c.lines[2].c_str());
}

void test_a_line_of_more_than_511_bytes_is_handed_over_cut_so_that_it_counts_as_damage() {
  SdStack s;
  SdMessageLog log(s.card);
  s.volume.put(kMessageLogPath, std::string(2000, 'x') + "\nok\n");
  Collect c;
  TEST_ASSERT_TRUE(log.readAll(c));
  TEST_ASSERT_EQUAL(2, c.lines.size());
  TEST_ASSERT_EQUAL(511, c.lines[0].size());
}

void test_a_history_larger_than_one_read_block_comes_back_whole() {
  SdStack s;
  SdMessageLog log(s.card);
  std::string text;
  for (int i = 0; i < 400; ++i) text += "line number " + std::to_string(i) + "\n";
  s.volume.put(kMessageLogPath, text);
  Collect c;
  TEST_ASSERT_TRUE(log.readAll(c));
  TEST_ASSERT_EQUAL(400, c.lines.size());
  TEST_ASSERT_EQUAL_STRING("line number 399", c.lines[399].c_str());
}

void test_a_card_that_fails_reading_the_history_is_reported_and_what_was_read_is_kept() {
  SdStack s;
  SdMessageLog log(s.card);
  s.volume.put(kMessageLogPath, "a\nb\n");
  s.volume.failReads(true);
  Collect c;
  TEST_ASSERT_FALSE(log.readAll(c));
}

void test_a_nearly_full_card_refuses_the_history_a_line_and_the_store_counts_it() {
  SdStack s;
  SdMessageLog log(s.card);
  s.volume.setCapacity(100 * 1024);
  s.volume.put("/data.bin", std::string(80 * 1024, 'x'));
  MessageStore store;
  store.attach(&log);
  Message m;
  std::snprintf(m.peer, sizeof(m.peer), "+491701111111");
  std::snprintf(m.text, sizeof(m.text), "hello");
  store.add(m);
  TEST_ASSERT_EQUAL(1, store.writeErrors());
  TEST_ASSERT_EQUAL(1, store.count());  // the message is on the screen all the same
}

void test_without_a_card_the_history_is_unavailable_and_the_store_goes_on() {
  SdStack s(false);
  s.volume.removeCard();
  s.card.begin(s.bootLog);
  SdMessageLog log(s.card);
  MessageStore store;
  store.attach(&log);
  TEST_ASSERT_FALSE(store.restore());
  Message m;
  std::snprintf(m.peer, sizeof(m.peer), "+491701111111");
  std::snprintf(m.text, sizeof(m.text), "hello");
  store.add(m);
  TEST_ASSERT_EQUAL(1, store.count());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_line_without_a_known_time_is_stamped_with_dashes_never_1970);
  RUN_TEST(test_a_line_with_a_known_time_has_the_epoch_and_the_local_time_of_day);
  RUN_TEST(test_a_zone_west_of_utc_wraps_the_day_correctly);
  RUN_TEST(test_every_line_goes_to_the_console_too);
  RUN_TEST(test_without_a_card_the_lines_wait_and_are_written_in_order_when_it_comes);
  RUN_TEST(test_more_than_64_waiting_lines_lose_the_oldest_and_say_how_many);
  RUN_TEST(test_below_32_kb_free_the_log_waits_and_the_configuration_keeps_its_room);
  RUN_TEST(test_the_log_is_rotated_at_256_kb_and_the_older_one_is_replaced);
  RUN_TEST(test_a_card_that_fails_in_the_middle_of_a_line_keeps_the_line_and_never_joins_it_to_the_next);
  RUN_TEST(test_a_line_break_in_a_line_stays_one_record);
  RUN_TEST(test_the_store_restores_what_it_wrote_through_the_card_log);
  RUN_TEST(test_a_cut_off_last_line_comes_back_as_a_damaged_line_not_as_a_message);
  RUN_TEST(test_a_file_that_was_touched_on_windows_reads_the_same);
  RUN_TEST(test_a_line_of_more_than_511_bytes_is_handed_over_cut_so_that_it_counts_as_damage);
  RUN_TEST(test_a_history_larger_than_one_read_block_comes_back_whole);
  RUN_TEST(test_a_card_that_fails_reading_the_history_is_reported_and_what_was_read_is_kept);
  RUN_TEST(test_a_nearly_full_card_refuses_the_history_a_line_and_the_store_counts_it);
  RUN_TEST(test_without_a_card_the_history_is_unavailable_and_the_store_goes_on);
  return UNITY_END();
}
