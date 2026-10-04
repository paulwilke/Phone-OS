#pragma once

// The contract of ConfigStore. Rig: `ConfigStore& store()`,
//   bool failNextWrite()      the next write fails halfway; false if not possible
//   bool failAllWrites(bool)  every write fails (a full or missing card); false if not possible

#include <algorithm>
#include <string>
#include <vector>

#include <hal/ConfigStore.h>

#include "Contract.h"

template <class Rig>
struct ConfigStoreContract {
  static std::string content(Rig& rig, const std::string& path) {
    std::string out;
    TEST_ASSERT_TRUE_MESSAGE(rig.store().read(path, out, 1 << 20), path.c_str());
    return out;
  }
  static bool listed(Rig& rig, const std::string& dir, const std::string& name) {
    std::vector<FileEntry> entries;
    if (!rig.store().list(dir, entries)) return false;
    for (const FileEntry& e : entries) {
      if (e.name == name) return true;
    }
    return false;
  }

  // ---- reading and writing -----------------------------------------------------------

  static void what_is_written_is_read_back_and_sized() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.store().write("/config/settings.toml", "language = \"en\"\n"));
    TEST_ASSERT_EQUAL_STRING("language = \"en\"\n", content(rig, "/config/settings.toml").c_str());
    TEST_ASSERT_EQUAL(16, rig.store().fileSize("/config/settings.toml"));
  }

  static void a_missing_file_is_missing_everywhere() {
    Rig rig;
    std::string out = "leftover";
    TEST_ASSERT_FALSE(rig.store().read("/nothing.txt", out, 100));
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
    out = "leftover";
    TEST_ASSERT_FALSE(rig.store().readTail("/nothing.txt", out, 100));
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
    TEST_ASSERT_EQUAL(-1, rig.store().fileSize("/nothing.txt"));
    TEST_ASSERT_FALSE(rig.store().remove("/nothing.txt"));
  }

  // An empty file exists; a missing one does not: the two are told apart.
  static void an_empty_file_exists_and_is_not_a_missing_one() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.store().write("/empty.txt", ""));
    std::string out = "leftover";
    TEST_ASSERT_TRUE(rig.store().read("/empty.txt", out, 100));
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
    TEST_ASSERT_EQUAL(0, rig.store().fileSize("/empty.txt"));
    TEST_ASSERT_TRUE(listed(rig, "/", "empty.txt"));
  }

  static void a_write_replaces_the_whole_file() {
    Rig rig;
    rig.store().write("/a.txt", "a long first content");
    TEST_ASSERT_TRUE(rig.store().write("/a.txt", "short"));
    TEST_ASSERT_EQUAL_STRING("short", content(rig, "/a.txt").c_str());
    TEST_ASSERT_EQUAL(5, rig.store().fileSize("/a.txt"));
  }

  static void a_reading_larger_than_the_limit_is_cut_and_the_size_says_how_large_it_is() {
    Rig rig;
    rig.store().write("/a.txt", "0123456789");
    std::string out;
    TEST_ASSERT_TRUE(rig.store().read("/a.txt", out, 4));
    TEST_ASSERT_EQUAL_STRING("0123", out.c_str());
    TEST_ASSERT_TRUE(rig.store().read("/a.txt", out, 0));
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
    TEST_ASSERT_EQUAL(10, rig.store().fileSize("/a.txt"));
  }

  static void the_tail_is_the_end_of_the_file() {
    Rig rig;
    rig.store().write("/system.log", "line 1\nline 2\nline 3\n");
    std::string out;
    TEST_ASSERT_TRUE(rig.store().readTail("/system.log", out, 7));
    TEST_ASSERT_EQUAL_STRING("line 3\n", out.c_str());
    TEST_ASSERT_TRUE(rig.store().readTail("/system.log", out, 1000));
    TEST_ASSERT_EQUAL_STRING("line 1\nline 2\nline 3\n", out.c_str());
  }

  static void any_bytes_survive_including_zero_bytes_and_text_with_umlauts() {
    Rig rig;
    std::string data("a\0b\xFF\xC3\xBC\r\n", 8);
    TEST_ASSERT_TRUE(rig.store().write("/bin.dat", data));
    TEST_ASSERT_TRUE(content(rig, "/bin.dat") == data);
    std::string big(100000, 'x');
    big[5000] = 'y';
    TEST_ASSERT_TRUE(rig.store().write("/big.dat", big));
    TEST_ASSERT_TRUE(content(rig, "/big.dat") == big);
    TEST_ASSERT_EQUAL(100000, rig.store().fileSize("/big.dat"));
  }

  static void files_are_independent() {
    Rig rig;
    rig.store().write("/a.txt", "A");
    rig.store().write("/b.txt", "B");
    rig.store().write("/a.txt", "AA");
    TEST_ASSERT_EQUAL_STRING("B", content(rig, "/b.txt").c_str());
  }

  // ---- a failed write -------------------------------------------------------------------

  // The hard promise: a write that fails halfway leaves the old content, whole.
  static void a_failed_write_leaves_the_old_content_intact() {
    Rig rig;
    rig.store().write("/config/settings.toml", "old content that must survive");
    CONTRACT_NEEDS(rig.failNextWrite());
    TEST_ASSERT_FALSE(rig.store().write("/config/settings.toml", "new content that fails halfway"));
    TEST_ASSERT_EQUAL_STRING("old content that must survive", content(rig, "/config/settings.toml").c_str());
    TEST_ASSERT_EQUAL(29, rig.store().fileSize("/config/settings.toml"));
  }

  static void a_failed_write_of_a_new_file_leaves_no_file() {
    Rig rig;
    CONTRACT_NEEDS(rig.failNextWrite());
    TEST_ASSERT_FALSE(rig.store().write("/new.txt", "content"));
    TEST_ASSERT_EQUAL(-1, rig.store().fileSize("/new.txt"));
    std::string out;
    TEST_ASSERT_FALSE(rig.store().read("/new.txt", out, 100));
    TEST_ASSERT_FALSE(listed(rig, "/", "new.txt"));
  }

  static void a_failed_write_leaves_nothing_visible_in_the_listing() {
    Rig rig;
    rig.store().write("/config/settings.toml", "old");
    CONTRACT_NEEDS(rig.failNextWrite());
    rig.store().write("/config/settings.toml", "new content that fails halfway");
    std::vector<FileEntry> entries;
    TEST_ASSERT_TRUE(rig.store().list("/config", entries));
    TEST_ASSERT_EQUAL(1, entries.size());
    TEST_ASSERT_EQUAL_STRING("settings.toml", entries[0].name.c_str());
    TEST_ASSERT_EQUAL(3, entries[0].size);
  }

  static void after_a_failed_write_the_next_one_works() {
    Rig rig;
    rig.store().write("/a.txt", "old");
    CONTRACT_NEEDS(rig.failNextWrite());
    TEST_ASSERT_FALSE(rig.store().write("/a.txt", "fails"));
    TEST_ASSERT_TRUE(rig.store().write("/a.txt", "works"));
    TEST_ASSERT_EQUAL_STRING("works", content(rig, "/a.txt").c_str());
  }

  static void a_card_that_refuses_writes_changes_nothing() {
    Rig rig;
    rig.store().write("/a.txt", "old");
    CONTRACT_NEEDS(rig.failAllWrites(true));
    TEST_ASSERT_FALSE(rig.store().write("/a.txt", "new"));
    TEST_ASSERT_FALSE(rig.store().write("/b.txt", "new"));
    TEST_ASSERT_EQUAL_STRING("old", content(rig, "/a.txt").c_str());
    TEST_ASSERT_EQUAL(-1, rig.store().fileSize("/b.txt"));
    rig.failAllWrites(false);
    TEST_ASSERT_TRUE(rig.store().write("/b.txt", "new"));
  }

  // ---- paths ------------------------------------------------------------------------------

  static void unsafe_paths_are_refused_by_every_operation() {
    Rig rig;
    rig.store().write("/secret.txt", "s");
    const char* bad[] = {"", "config/x", "/../secret.txt", "/config/../secret.txt", "//secret.txt", "/a b", "/a\\b",
                         "/x\n", "/\xC3\xA4", "/a:b", "/x.tmp", "/X.TMP", "/config/settings.toml.tmp", "/%2e%2e/x",
                         "/..", "/./secret.txt"};
    for (const char* path : bad) {
      std::string out;
      TEST_ASSERT_FALSE_MESSAGE(rig.store().write(path, "data"), path);
      TEST_ASSERT_FALSE_MESSAGE(rig.store().read(path, out, 100), path);
      TEST_ASSERT_FALSE_MESSAGE(rig.store().readTail(path, out, 100), path);
      TEST_ASSERT_EQUAL_MESSAGE(-1, rig.store().fileSize(path), path);
      TEST_ASSERT_FALSE_MESSAGE(rig.store().remove(path), path);
    }
    TEST_ASSERT_EQUAL_STRING("s", content(rig, "/secret.txt").c_str());
  }

  static void a_path_that_is_too_long_is_refused() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.store().write("/" + std::string(41, 'a'), "x"));
    TEST_ASSERT_FALSE(rig.store().write("/" + std::string(40, 'a') + "/" + std::string(40, 'b'), "x"));
  }

  // The card is FAT: names differ in case only are the same file.
  static void names_are_compared_without_regard_to_case() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.store().write("/Config/Settings.TOML", "one"));
    TEST_ASSERT_EQUAL_STRING("one", content(rig, "/config/settings.toml").c_str());
    TEST_ASSERT_TRUE(rig.store().write("/CONFIG/SETTINGS.toml", "two"));
    std::vector<FileEntry> entries;
    TEST_ASSERT_TRUE(rig.store().list("/config", entries));
    TEST_ASSERT_EQUAL(1, entries.size());
    TEST_ASSERT_EQUAL_STRING("two", content(rig, "/Config/settings.toml").c_str());
  }

  // ---- directories and listing ---------------------------------------------------------

  static void a_missing_directory_of_one_level_is_created() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.store().write("/newdir/file.txt", "x"));
    TEST_ASSERT_EQUAL_STRING("x", content(rig, "/newdir/file.txt").c_str());
  }

  static void a_path_two_levels_below_a_missing_directory_fails() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.store().write("/a/b/file.txt", "x"));
    TEST_ASSERT_EQUAL(-1, rig.store().fileSize("/a/b/file.txt"));
  }

  static void the_listing_has_files_with_sizes_and_directories() {
    Rig rig;
    rig.store().write("/top.txt", "12345");
    rig.store().write("/config/a.toml", "1");
    rig.store().write("/config/b.toml", "12");
    std::vector<FileEntry> entries;
    TEST_ASSERT_TRUE(rig.store().list("/", entries));
    TEST_ASSERT_EQUAL(2, entries.size());
    for (const FileEntry& e : entries) {
      if (e.name == "top.txt") {
        TEST_ASSERT_FALSE(e.isDir);
        TEST_ASSERT_EQUAL(5, e.size);
      } else {
        TEST_ASSERT_EQUAL_STRING("config", e.name.c_str());
        TEST_ASSERT_TRUE(e.isDir);
      }
    }
    TEST_ASSERT_TRUE(rig.store().list("/config", entries));
    TEST_ASSERT_EQUAL(2, entries.size());
    TEST_ASSERT_TRUE(rig.store().list("/config/", entries));  // a trailing slash is fine
    TEST_ASSERT_EQUAL(2, entries.size());
  }

  static void a_missing_directory_cannot_be_listed() {
    Rig rig;
    std::vector<FileEntry> entries;
    entries.push_back({"leftover", 1, false});
    TEST_ASSERT_FALSE(rig.store().list("/nowhere", entries));
    TEST_ASSERT_FALSE(rig.store().list("/../etc", entries));
  }

  static void scratch_files_are_never_visible() {
    Rig rig;
    for (int i = 0; i < 5; ++i) rig.store().write("/config/settings.toml", "version " + std::to_string(i));
    std::vector<FileEntry> entries;
    TEST_ASSERT_TRUE(rig.store().list("/config", entries));
    for (const FileEntry& e : entries) TEST_ASSERT_TRUE(e.name.find(".tmp") == std::string::npos);
    TEST_ASSERT_EQUAL(1, entries.size());
  }

  // ---- removing ---------------------------------------------------------------------------

  static void remove_deletes_a_file_once() {
    Rig rig;
    rig.store().write("/a.txt", "x");
    TEST_ASSERT_TRUE(rig.store().remove("/a.txt"));
    TEST_ASSERT_EQUAL(-1, rig.store().fileSize("/a.txt"));
    TEST_ASSERT_FALSE(listed(rig, "/", "a.txt"));
    TEST_ASSERT_FALSE(rig.store().remove("/a.txt"));
  }

  static void a_removed_file_can_be_written_again() {
    Rig rig;
    rig.store().write("/a.txt", "x");
    rig.store().remove("/a.txt");
    TEST_ASSERT_TRUE(rig.store().write("/a.txt", "y"));
    TEST_ASSERT_EQUAL_STRING("y", content(rig, "/a.txt").c_str());
  }
};

#define RUN_CONFIG_STORE_CONTRACT(Rig)                                                                        \
  RUN_TEST(ConfigStoreContract<Rig>::what_is_written_is_read_back_and_sized);                                  \
  RUN_TEST(ConfigStoreContract<Rig>::a_missing_file_is_missing_everywhere);                                    \
  RUN_TEST(ConfigStoreContract<Rig>::an_empty_file_exists_and_is_not_a_missing_one);                           \
  RUN_TEST(ConfigStoreContract<Rig>::a_write_replaces_the_whole_file);                                         \
  RUN_TEST(ConfigStoreContract<Rig>::a_reading_larger_than_the_limit_is_cut_and_the_size_says_how_large_it_is); \
  RUN_TEST(ConfigStoreContract<Rig>::the_tail_is_the_end_of_the_file);                                         \
  RUN_TEST(ConfigStoreContract<Rig>::any_bytes_survive_including_zero_bytes_and_text_with_umlauts);            \
  RUN_TEST(ConfigStoreContract<Rig>::files_are_independent);                                                   \
  RUN_TEST(ConfigStoreContract<Rig>::a_failed_write_leaves_the_old_content_intact);                            \
  RUN_TEST(ConfigStoreContract<Rig>::a_failed_write_of_a_new_file_leaves_no_file);                             \
  RUN_TEST(ConfigStoreContract<Rig>::a_failed_write_leaves_nothing_visible_in_the_listing);                    \
  RUN_TEST(ConfigStoreContract<Rig>::after_a_failed_write_the_next_one_works);                                 \
  RUN_TEST(ConfigStoreContract<Rig>::a_card_that_refuses_writes_changes_nothing);                              \
  RUN_TEST(ConfigStoreContract<Rig>::unsafe_paths_are_refused_by_every_operation);                             \
  RUN_TEST(ConfigStoreContract<Rig>::a_path_that_is_too_long_is_refused);                                      \
  RUN_TEST(ConfigStoreContract<Rig>::names_are_compared_without_regard_to_case);                               \
  RUN_TEST(ConfigStoreContract<Rig>::a_missing_directory_of_one_level_is_created);                             \
  RUN_TEST(ConfigStoreContract<Rig>::a_path_two_levels_below_a_missing_directory_fails);                       \
  RUN_TEST(ConfigStoreContract<Rig>::the_listing_has_files_with_sizes_and_directories);                        \
  RUN_TEST(ConfigStoreContract<Rig>::a_missing_directory_cannot_be_listed);                                    \
  RUN_TEST(ConfigStoreContract<Rig>::scratch_files_are_never_visible);                                         \
  RUN_TEST(ConfigStoreContract<Rig>::remove_deletes_a_file_once);                                              \
  RUN_TEST(ConfigStoreContract<Rig>::a_removed_file_can_be_written_again)
