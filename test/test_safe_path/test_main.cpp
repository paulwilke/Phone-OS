// Host tests for the path check: what comes from outside stays on the card.
// allow-non-english: one rejected path contains an umlaut.

#include <unity.h>

#include <string>

#include <app/SafePath.h>

using safepath::Result;

void setUp() {}
void tearDown() {}

void test_ordinary_paths_are_valid() {
  for (const char* p : {"/", "/config", "/config/", "/config/settings.toml", "/system.log",
                        "/logs/a-b_c.1.txt", "/UPPER/Case.TXT"})
    TEST_ASSERT_TRUE_MESSAGE(safepath::valid(p), p);
}

// Every form the previous tests refused, each with the reason it is refused for.
void test_escapes_and_oddities_are_refused_with_their_reason() {
  struct Case { const char* path; Result why; };
  const Case cases[] = {
      {"", Result::NotAbsolute},
      {"config/x", Result::NotAbsolute},
      {"/../etc/passwd", Result::DotSegment},
      {"/config/../secret", Result::DotSegment},
      {"/config/..", Result::DotSegment},
      {"/..", Result::DotSegment},
      {"/./x", Result::DotSegment},
      {"/config/.", Result::DotSegment},
      {"//x", Result::EmptySegment},
      {"/config//x", Result::EmptySegment},
      {"/a b", Result::BadCharacter},
      {"/a\\b", Result::BadCharacter},
      {"/x\n", Result::BadCharacter},
      {"/\xC3\xA4", Result::BadCharacter},
      {"/%2e%2e/x", Result::BadCharacter},
      {"/a:b", Result::BadCharacter},
      {"/config/x.tmp", Result::Reserved},
  };
  for (const Case& c : cases) {
    TEST_ASSERT_TRUE_MESSAGE(safepath::check(c.path) == c.why, c.path);
    TEST_ASSERT_FALSE_MESSAGE(safepath::valid(c.path), c.path);
  }
  TEST_ASSERT_FALSE(safepath::valid(nullptr));
  TEST_ASSERT_TRUE(safepath::check(nullptr) == Result::NotAbsolute);
}

// Names that merely contain dots are not dot segments.
void test_dots_inside_names_are_fine() {
  TEST_ASSERT_TRUE(safepath::valid("/a..b"));
  TEST_ASSERT_TRUE(safepath::valid("/..a"));
  TEST_ASSERT_TRUE(safepath::valid("/a.."));
  TEST_ASSERT_TRUE(safepath::valid("/.hidden"));
  TEST_ASSERT_FALSE(safepath::valid("/..."));
  TEST_ASSERT_FALSE(safepath::valid("/x/..."));  // dots only: a current-directory entry in disguise
}

void test_length_limits() {
  TEST_ASSERT_TRUE(safepath::valid(("/" + std::string(40, 'a')).c_str()));
  TEST_ASSERT_TRUE(safepath::check(("/" + std::string(41, 'a')).c_str()) == Result::SegmentTooLong);
  std::string exact = "/" + std::string(40, 'a') + "/" + std::string(22, 'b');  // 64 bytes
  TEST_ASSERT_EQUAL(64, exact.size());
  TEST_ASSERT_TRUE(safepath::valid(exact.c_str()));
  TEST_ASSERT_TRUE(safepath::check((exact + "b").c_str()) == Result::TooLong);
  std::string deep = "/a";
  while (deep.size() <= safepath::kMaxLength) deep += "/b";
  TEST_ASSERT_TRUE(safepath::check(deep.c_str()) == Result::TooLong);
}

// FAT does not tell "x.tmp" from "x.TMP" or "X.Tmp": all of them are the scratch file.
void test_the_temporary_suffix_is_refused_in_any_case() {
  for (const char* p : {"/x.tmp", "/x.TMP", "/x.Tmp", "/config/settings.toml.tmp", "/a/b.tmp/", "/.tmp"})
    TEST_ASSERT_TRUE_MESSAGE(safepath::check(p) == Result::Reserved, p);
  TEST_ASSERT_TRUE(safepath::valid("/x.tmpx"));
  TEST_ASSERT_TRUE(safepath::valid("/tmp"));
  TEST_ASSERT_TRUE(safepath::valid("/tmp/file"));
  TEST_ASSERT_TRUE(safepath::valid("/x.tmp/y"));  // only the last name is a file that gets written through
}

void test_helpers_ignore_case() {
  TEST_ASSERT_TRUE(safepath::inConfigDir("/config/x"));
  TEST_ASSERT_TRUE(safepath::inConfigDir("/CONFIG/x"));
  TEST_ASSERT_TRUE(safepath::inConfigDir("/Config/"));
  TEST_ASSERT_FALSE(safepath::inConfigDir("/configs/x"));
  TEST_ASSERT_FALSE(safepath::inConfigDir("/config"));
  TEST_ASSERT_FALSE(safepath::inConfigDir(nullptr));
  TEST_ASSERT_TRUE(safepath::endsWith("/system.log", ".log"));
  TEST_ASSERT_TRUE(safepath::endsWith("/SYSTEM.LOG", ".log"));
  TEST_ASSERT_FALSE(safepath::endsWith("/a", ".log"));
  TEST_ASSERT_FALSE(safepath::endsWith("g", ".log"));
  TEST_ASSERT_FALSE(safepath::endsWith(nullptr, ".log"));
}

// A rule about one file must hold in every spelling the card accepts, or a capital letter walks around it.
void test_the_same_file_is_the_same_in_any_case() {
  TEST_ASSERT_TRUE(safepath::sameFile("/config/secrets.toml", "/config/secrets.toml"));
  TEST_ASSERT_TRUE(safepath::sameFile("/CONFIG/SECRETS.TOML", "/config/secrets.toml"));
  TEST_ASSERT_TRUE(safepath::sameFile("/config/Secrets.Toml", "/config/secrets.toml"));
  TEST_ASSERT_FALSE(safepath::sameFile("/config/secrets.tom", "/config/secrets.toml"));
  TEST_ASSERT_FALSE(safepath::sameFile("/config/secrets.toml2", "/config/secrets.toml"));
  TEST_ASSERT_FALSE(safepath::sameFile("/config/secret.toml", "/config/secrets.toml"));
  TEST_ASSERT_FALSE(safepath::sameFile(nullptr, "/config/secrets.toml"));
  TEST_ASSERT_FALSE(safepath::sameFile("/config/secrets.toml", nullptr));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ordinary_paths_are_valid);
  RUN_TEST(test_escapes_and_oddities_are_refused_with_their_reason);
  RUN_TEST(test_dots_inside_names_are_fine);
  RUN_TEST(test_length_limits);
  RUN_TEST(test_the_temporary_suffix_is_refused_in_any_case);
  RUN_TEST(test_helpers_ignore_case);
  RUN_TEST(test_the_same_file_is_the_same_in_any_case);
  return UNITY_END();
}
