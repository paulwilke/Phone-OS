// The configuration store on the card: what the contract cannot say about a real volume — the order of the
// operations of a safe write, a power loss at every step of it, a card that runs full, one that is pulled out,
// and the recovery at the next start.

#include <unity.h>

#include <BoardStorageRigs.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
std::string read(SdConfigStore& store, const std::string& path) {
  std::string out;
  store.read(path, out, 1 << 20);
  return out;
}
}  // namespace

void test_a_safe_write_goes_through_a_scratch_file_in_this_order() {
  SdStack s;
  SdConfigStore store(s.card);
  s.volume.put("/config/settings.toml", "old");
  TEST_ASSERT_TRUE(store.write("/config/settings.toml", "new"));
  const std::vector<std::string> expected = {"write /config/settings.toml.tmp", "remove /config/settings.toml",
                                            "rename /config/settings.toml.tmp /config/settings.toml"};
  TEST_ASSERT_TRUE(expected == s.volume.operations());
  TEST_ASSERT_EQUAL_STRING("new", s.volume.file("/config/settings.toml").c_str());
}

void test_a_new_file_needs_no_removal() {
  SdStack s;
  SdConfigStore store(s.card);
  TEST_ASSERT_TRUE(store.write("/a.txt", "x"));
  TEST_ASSERT_EQUAL(2, s.volume.operations().size());
}

// A power loss at any step leaves the old content or the new one, never neither.
void test_a_power_loss_at_every_step_of_a_write_leaves_the_old_content_or_the_new_one() {
  for (int steps = 0; steps <= 3; ++steps) {
    SdStack s;
    s.volume.put("/config/settings.toml", "old content");
    {
      SdConfigStore store(s.card);
      s.volume.loseCardAfter(steps);
      store.write("/config/settings.toml", "new content");
    }
    s.volume.powerBack();
    s.card.begin(s.bootLog);  // the device starts again
    SdConfigStore restarted(s.card);
    restarted.recover();
    const std::string now = read(restarted, "/config/settings.toml");
    TEST_ASSERT_TRUE_MESSAGE(now == "old content" || now == "new content", std::to_string(steps).c_str());
    for (const std::string& p : s.volume.paths()) TEST_ASSERT_TRUE_MESSAGE(p.find(".tmp") == std::string::npos, p.c_str());
    if (steps == 3) TEST_ASSERT_EQUAL_STRING("new content", now.c_str());
    if (steps <= 1) TEST_ASSERT_EQUAL_STRING("old content", now.c_str());
  }
}

// The old file is removed only after the new one is complete, so a finished scratch file without a target is
// the new content and is put in place.
void test_a_finished_scratch_file_without_a_target_is_renamed_at_the_next_start() {
  SdStack s;
  s.volume.put("/config/settings.toml.tmp", "complete new content");
  SdConfigStore store(s.card);
  TEST_ASSERT_EQUAL(1, store.recover());
  TEST_ASSERT_EQUAL_STRING("complete new content", s.volume.file("/config/settings.toml").c_str());
  TEST_ASSERT_FALSE(s.volume.has("/config/settings.toml.tmp"));
}

void test_a_scratch_file_next_to_its_target_is_an_unfinished_write_and_is_deleted() {
  SdStack s;
  s.volume.put("/config/settings.toml", "whole old content");
  s.volume.put("/config/settings.toml.tmp", "half a ne");
  SdConfigStore store(s.card);
  TEST_ASSERT_EQUAL(1, store.recover());
  TEST_ASSERT_EQUAL_STRING("whole old content", s.volume.file("/config/settings.toml").c_str());
  TEST_ASSERT_FALSE(s.volume.has("/config/settings.toml.tmp"));
}

void test_recovery_looks_in_the_root_and_in_each_directory_and_does_nothing_without_a_card() {
  SdStack s;
  s.volume.put("/a.txt.tmp", "root");
  s.volume.put("/config/b.toml.tmp", "dir");
  SdConfigStore store(s.card);
  TEST_ASSERT_EQUAL(2, store.recover());
  TEST_ASSERT_EQUAL_STRING("root", s.volume.file("/a.txt").c_str());
  SdStack none(false);
  SdConfigStore other(none.card);
  TEST_ASSERT_EQUAL(0, other.recover());
}

void test_a_rename_that_fails_leaves_the_complete_new_content_for_the_next_start() {
  SdStack s;
  s.volume.put("/config/settings.toml", "old");
  SdConfigStore store(s.card);
  s.volume.failRenames(true);
  TEST_ASSERT_FALSE(store.write("/config/settings.toml", "new"));
  s.volume.failRenames(false);
  TEST_ASSERT_EQUAL_STRING("new", s.volume.file("/config/settings.toml.tmp").c_str());
  TEST_ASSERT_EQUAL(1, store.recover());
  TEST_ASSERT_EQUAL_STRING("new", read(store, "/config/settings.toml").c_str());
}

void test_an_old_file_that_cannot_be_removed_stays_whole_and_the_scratch_file_goes() {
  SdStack s;
  s.volume.put("/a.txt", "old");
  SdConfigStore store(s.card);
  s.volume.failRemoves(true);
  TEST_ASSERT_FALSE(store.write("/a.txt", "new"));
  s.volume.failRemoves(false);
  TEST_ASSERT_EQUAL_STRING("old", s.volume.file("/a.txt").c_str());
}

// A full card refuses before it has touched anything, and the old file is whole.
void test_a_full_card_refuses_a_write_without_touching_the_card() {
  SdStack s;
  s.volume.put("/a.txt", std::string(1000, 'o'));
  s.volume.setCapacity(1000 + 5000);  // 5000 bytes free, the data and the 8 KB reserve do not fit
  SdConfigStore store(s.card);
  const size_t before = s.volume.operations().size();
  TEST_ASSERT_FALSE(store.write("/a.txt", std::string(100, 'n')));
  TEST_ASSERT_EQUAL(before, s.volume.operations().size());
  TEST_ASSERT_EQUAL(1000, s.volume.file("/a.txt").size());
  s.volume.setCapacity(1000 + 100000);
  TEST_ASSERT_TRUE(store.write("/a.txt", std::string(100, 'n')));
}

void test_a_write_that_fills_the_card_halfway_leaves_the_old_file_and_no_scratch_file() {
  SdStack s;
  s.volume.put("/a.txt", "old");
  SdConfigStore store(s.card);
  s.volume.setCapacity(3 + 8192 + 20);  // enough for the check, not for the data once the scratch file is written
  TEST_ASSERT_FALSE(store.write("/a.txt", std::string(30, 'n')));
  TEST_ASSERT_EQUAL_STRING("old", s.volume.file("/a.txt").c_str());
  TEST_ASSERT_FALSE(s.volume.has("/a.txt.tmp"));
}

void test_without_a_card_every_call_fails_and_nothing_crashes() {
  SdStack s(false);
  s.volume.removeCard();
  s.card.begin(s.bootLog);
  SdConfigStore store(s.card);
  std::string out = "leftover";
  TEST_ASSERT_FALSE(store.read("/config/settings.toml", out, 100));
  TEST_ASSERT_EQUAL_STRING("", out.c_str());
  TEST_ASSERT_FALSE(store.readTail("/system.log", out, 100));
  TEST_ASSERT_FALSE(store.readAt("/system.log", 0, out, 100));
  TEST_ASSERT_EQUAL(-1, store.fileSize("/a"));
  TEST_ASSERT_FALSE(store.write("/a.txt", "x"));
  TEST_ASSERT_FALSE(store.remove("/a.txt"));
  std::vector<FileEntry> entries;
  TEST_ASSERT_FALSE(store.list("/", entries));
}

void test_a_card_that_fails_writes_three_times_counts_as_pulled_and_the_next_call_does_not_touch_it() {
  SdStack s;
  SdConfigStore store(s.card);
  s.volume.failAllWrites(true);
  for (int i = 0; i < 3; ++i) TEST_ASSERT_FALSE(store.write("/a.txt", "x"));
  TEST_ASSERT_FALSE(s.card.ready());
  const size_t before = s.volume.operations().size();
  TEST_ASSERT_FALSE(store.write("/a.txt", "x"));
  TEST_ASSERT_EQUAL(before, s.volume.operations().size());
}

void test_a_read_is_taken_in_blocks_of_1_kb_and_cut_at_the_limit() {
  SdStack s;
  s.volume.put("/big.log", std::string(2500, 'x'));
  SdConfigStore store(s.card);
  std::string out;
  TEST_ASSERT_TRUE(store.read("/big.log", out, 8192));
  TEST_ASSERT_EQUAL(2500, out.size());
  TEST_ASSERT_EQUAL(3, s.volume.readCount());
  TEST_ASSERT_TRUE(store.read("/big.log", out, 1500));
  TEST_ASSERT_EQUAL(1500, out.size());
}

void test_a_card_that_fails_in_the_middle_of_a_read_gives_no_half_file() {
  SdStack s;
  s.volume.put("/a.txt", "abc");
  SdConfigStore store(s.card);
  s.volume.failReads(true);
  std::string out = "leftover";
  TEST_ASSERT_FALSE(store.read("/a.txt", out, 100));
  TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

void test_the_tail_of_a_large_log_is_its_end() {
  SdStack s;
  std::string log;
  for (int i = 0; i < 400; ++i) log += "line " + std::to_string(i) + "\n";
  s.volume.put("/system.log", log);
  SdConfigStore store(s.card);
  std::string out;
  TEST_ASSERT_TRUE(store.readTail("/system.log", out, 20));
  TEST_ASSERT_EQUAL_STRING(log.substr(log.size() - 20).c_str(), out.c_str());
}

void test_a_piece_of_a_large_log_is_read_from_its_offset_in_blocks_and_a_failing_card_gives_none() {
  SdStack s;
  std::string log;
  for (int i = 0; i < 400; ++i) log += "line " + std::to_string(i) + "\n";
  s.volume.put("/system.log", log);
  SdConfigStore store(s.card);
  std::string out;
  TEST_ASSERT_TRUE(store.readAt("/system.log", 100, out, 2500));
  TEST_ASSERT_EQUAL_STRING(log.substr(100, 2500).c_str(), out.c_str());
  TEST_ASSERT_EQUAL(3, s.volume.readCount());
  TEST_ASSERT_TRUE(store.readAt("/system.log", log.size(), out, 2500));
  TEST_ASSERT_EQUAL_STRING("", out.c_str());
  s.volume.failReads(true);
  out = "leftover";
  TEST_ASSERT_FALSE(store.readAt("/system.log", 100, out, 2500));
  TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

void test_the_listing_hides_scratch_files_and_names_the_page_cannot_use() {
  SdStack s;
  s.volume.put("/config/settings.toml", "a");
  s.volume.put("/config/settings.toml.tmp", "half");
  s.volume.put("/config/hello world.txt", "b");
  s.volume.put("/config/ok-name_1.txt", "c");
  SdConfigStore store(s.card);
  std::vector<FileEntry> entries;
  TEST_ASSERT_TRUE(store.list("/config", entries));
  TEST_ASSERT_EQUAL(2, entries.size());
}

void test_a_directory_is_not_removed_or_read_as_a_file() {
  SdStack s;
  s.volume.put("/config/a.toml", "a");
  SdConfigStore store(s.card);
  TEST_ASSERT_FALSE(store.remove("/config"));
  std::string out;
  TEST_ASSERT_FALSE(store.read("/config", out, 10));
  TEST_ASSERT_EQUAL(-1, store.fileSize("/config"));
  TEST_ASSERT_FALSE(store.write("/config", "x"));
  TEST_ASSERT_TRUE(s.volume.has("/config/a.toml"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_safe_write_goes_through_a_scratch_file_in_this_order);
  RUN_TEST(test_a_new_file_needs_no_removal);
  RUN_TEST(test_a_power_loss_at_every_step_of_a_write_leaves_the_old_content_or_the_new_one);
  RUN_TEST(test_a_finished_scratch_file_without_a_target_is_renamed_at_the_next_start);
  RUN_TEST(test_a_scratch_file_next_to_its_target_is_an_unfinished_write_and_is_deleted);
  RUN_TEST(test_recovery_looks_in_the_root_and_in_each_directory_and_does_nothing_without_a_card);
  RUN_TEST(test_a_rename_that_fails_leaves_the_complete_new_content_for_the_next_start);
  RUN_TEST(test_an_old_file_that_cannot_be_removed_stays_whole_and_the_scratch_file_goes);
  RUN_TEST(test_a_full_card_refuses_a_write_without_touching_the_card);
  RUN_TEST(test_a_write_that_fills_the_card_halfway_leaves_the_old_file_and_no_scratch_file);
  RUN_TEST(test_without_a_card_every_call_fails_and_nothing_crashes);
  RUN_TEST(test_a_card_that_fails_writes_three_times_counts_as_pulled_and_the_next_call_does_not_touch_it);
  RUN_TEST(test_a_read_is_taken_in_blocks_of_1_kb_and_cut_at_the_limit);
  RUN_TEST(test_a_card_that_fails_in_the_middle_of_a_read_gives_no_half_file);
  RUN_TEST(test_the_tail_of_a_large_log_is_its_end);
  RUN_TEST(test_a_piece_of_a_large_log_is_read_from_its_offset_in_blocks_and_a_failing_card_gives_none);
  RUN_TEST(test_the_listing_hides_scratch_files_and_names_the_page_cannot_use);
  RUN_TEST(test_a_directory_is_not_removed_or_read_as_a_file);
  return UNITY_END();
}
