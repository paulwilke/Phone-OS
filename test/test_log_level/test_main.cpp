// Host tests for the log level.

#include <unity.h>

#include <initializer_list>

#include <app/LogLevel.h>

using namespace loglevel;

void setUp() {}
void tearDown() {}

namespace {

const Level kAllLevels[] = {Level::All, Level::Events, Level::Errors};

const char* const kAlways[] = {
    "COMMAND +491701111111 (Parent One) position -> done",
    "Remote query answer sent",
    "SOS ping (emergency) started to Parent One",
    "Quiet hours lifted by emergency until 12:30",
    "Restart requested from the web page",
    "Firmware 4.0.0 started",
    "Shutdown requested",
    "Config written",
    "WEB: settings changed",
    "Wi-Fi: link not connected (status 6, heap 41236 free, largest 20480); waiting 5 s before reconnecting",
    "Wi-Fi: link back after 3 s",
    "Wi-Fi: link still not connected after 5 s; reconnecting",
    "SMS dropped: sender not in the contact book (+491709999999)",
    "SMS dropped: sender not in the contact book (non-numeric sender)",
};

}  // namespace

// That somebody queried the device, or sent an emergency, must be provable on
// every level.
void test_what_must_always_be_logged_survives_every_level() {
  for (const char* line : kAlways) {
    TEST_ASSERT_TRUE_MESSAGE(classify(line) == Importance::Always, line);
    for (Level l : kAllLevels) TEST_ASSERT_TRUE_MESSAGE(shouldWrite(l, line), line);
  }
}

// A failure of an always-kept action is still an always-kept line.
void test_an_error_in_an_always_kept_action_is_still_kept() {
  for (const char* line : {"Remote query failed", "SOS could not be sent: error", "Firmware update rejected"}) {
    TEST_ASSERT_TRUE_MESSAGE(classify(line) == Importance::Always, line);
    TEST_ASSERT_TRUE_MESSAGE(shouldWrite(Level::Errors, line), line);
  }
}

void test_levels_cut_the_details_first() {
  const char* detail = "Modem woke up";
  const char* event = "Call from Parent One";
  const char* error = "SMS to Parent One rejected: +CMS ERROR: 500";
  TEST_ASSERT_TRUE(shouldWrite(Level::All, detail));
  TEST_ASSERT_FALSE(shouldWrite(Level::Events, detail));
  TEST_ASSERT_FALSE(shouldWrite(Level::Errors, detail));
  TEST_ASSERT_TRUE(shouldWrite(Level::All, event));
  TEST_ASSERT_TRUE(shouldWrite(Level::Events, event));
  TEST_ASSERT_FALSE(shouldWrite(Level::Errors, event));
  TEST_ASSERT_TRUE(shouldWrite(Level::All, error));
  TEST_ASSERT_TRUE(shouldWrite(Level::Events, error));
  TEST_ASSERT_TRUE(shouldWrite(Level::Errors, error));
}

void test_classes_of_typical_lines() {
  TEST_ASSERT_TRUE(classify("Modem woke up") == Importance::Detail);
  TEST_ASSERT_TRUE(classify("GPS fix: 9 satellites") == Importance::Detail);
  TEST_ASSERT_TRUE(classify("Call from Parent One") == Importance::Event);
  TEST_ASSERT_TRUE(classify("Message received") == Importance::Event);
  TEST_ASSERT_TRUE(classify("WiFi connected") == Importance::Event);
  TEST_ASSERT_TRUE(classify("SIM: ready") == Importance::Event);
  TEST_ASSERT_TRUE(classify("Modem not responding") == Importance::Error);
  TEST_ASSERT_TRUE(classify("Call rejected") == Importance::Error);  // error beats event
  TEST_ASSERT_TRUE(classify("File not found") == Importance::Error);
}

void test_matching_ignores_case() {
  TEST_ASSERT_TRUE(classify("sos") == Importance::Always);
  TEST_ASSERT_TRUE(classify("Sos pressed") == Importance::Always);
  TEST_ASSERT_TRUE(classify("MODEM FAILED") == Importance::Error);
}

// A keyword must begin a word: an inner "sos" in a name is no emergency, and the
// word "consent" is no "sent".
void test_a_keyword_must_start_a_word() {
  TEST_ASSERT_TRUE(classify("Contact Rosos added") == Importance::Detail);
  TEST_ASSERT_TRUE(classify("User consent stored") == Importance::Detail);
  TEST_ASSERT_TRUE(classify("recall of settings") == Importance::Detail);
  // At the start of a word, with anything following, it counts.
  TEST_ASSERT_TRUE(classify("restarted by watchdog") == Importance::Always);
  TEST_ASSERT_TRUE(classify("Contact 'SOS-Grandma' added") == Importance::Always);
}

void test_empty_and_null_lines_are_details() {
  TEST_ASSERT_TRUE(classify(nullptr) == Importance::Detail);
  TEST_ASSERT_TRUE(classify("") == Importance::Detail);
  TEST_ASSERT_TRUE(shouldWrite(Level::All, ""));
  TEST_ASSERT_FALSE(shouldWrite(Level::Events, nullptr));
}

void test_importance_overload_follows_the_same_table() {
  TEST_ASSERT_TRUE(shouldWrite(Level::Errors, Importance::Always));
  TEST_ASSERT_TRUE(shouldWrite(Level::Errors, Importance::Error));
  TEST_ASSERT_FALSE(shouldWrite(Level::Errors, Importance::Event));
  TEST_ASSERT_TRUE(shouldWrite(Level::Events, Importance::Event));
  TEST_ASSERT_FALSE(shouldWrite(Level::Events, Importance::Detail));
  TEST_ASSERT_TRUE(shouldWrite(Level::All, Importance::Detail));
}

void test_parse_defaults_to_everything() {
  TEST_ASSERT_EQUAL((int)Level::All, (int)parse(nullptr));
  TEST_ASSERT_EQUAL((int)Level::All, (int)parse(""));
  TEST_ASSERT_EQUAL((int)Level::All, (int)parse("nonsense"));
  TEST_ASSERT_EQUAL((int)Level::All, (int)parse("Errors"));  // names are exact
  TEST_ASSERT_EQUAL((int)Level::All, (int)parse("all"));
  TEST_ASSERT_EQUAL((int)Level::Events, (int)parse("events"));
  TEST_ASSERT_EQUAL((int)Level::Errors, (int)parse("errors"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_what_must_always_be_logged_survives_every_level);
  RUN_TEST(test_an_error_in_an_always_kept_action_is_still_kept);
  RUN_TEST(test_levels_cut_the_details_first);
  RUN_TEST(test_classes_of_typical_lines);
  RUN_TEST(test_matching_ignores_case);
  RUN_TEST(test_a_keyword_must_start_a_word);
  RUN_TEST(test_empty_and_null_lines_are_details);
  RUN_TEST(test_importance_overload_follows_the_same_table);
  RUN_TEST(test_parse_defaults_to_everything);
  return UNITY_END();
}
