// Host tests for the remote location query by SMS. SmsCommand recognises the query and
// checks the sender; the location itself is the SOS flow (test_sos_ping).
// allow-non-english: trigger words in German and French contain umlauts and accents.

#include <unity.h>

#include <string>
#include <utility>
#include <vector>

#include <fstream>
#include <sstream>

#include <app/SmsCommand.h>
#include <i18n/Translator.h>

namespace {

constexpr const char* kParentOne = "+491701111111";
constexpr const char* kStranger = "+491709999999";
constexpr uint32_t kNow = 1789475520;

struct FakeHost : CommandHost {
  std::vector<std::string> admins{kParentOne, "+491702222222"};
  std::vector<std::string> located;
  std::vector<std::pair<CommandRecord, CommandNotice>> records;
  bool busy = false;

  bool isAdmin(const std::string& phone) override {
    for (auto& a : admins) {
      if (a == phone) return true;
    }
    return false;
  }
  bool startLocationReply(const std::string& phone) override {
    if (busy) return false;
    located.push_back(phone);
    return true;
  }
  uint32_t uptimeSeconds() override { return 100; }
  uint32_t epochUtc() override { return kNow; }
  void recordCommand(const CommandRecord& record, CommandNotice notice) override {
    records.push_back({record, notice});
  }
};

SmsCommand::Config withTriggers(std::vector<std::string> words) {
  SmsCommand::Config cfg;
  cfg.triggers = std::move(words);
  return cfg;
}

SmsCommand::Config english() { return withTriggers({"#WhereAreYou"}); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_an_admin_asking_starts_a_location_reply() {
  FakeHost h;
  SmsCommand c(h, english());
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#WhereAreYou") == SmsCommand::Result::Handled);
  TEST_ASSERT_EQUAL(1, h.located.size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, h.located[0].c_str());
  TEST_ASSERT_EQUAL(1, h.records.size());
  TEST_ASSERT_TRUE(h.records[0].first.outcome == CommandOutcome::Executed);
  TEST_ASSERT_EQUAL_STRING(kParentOne, h.records[0].first.phone.c_str());
  TEST_ASSERT_EQUAL_UINT32(kNow, h.records[0].first.epochUtc);
  TEST_ASSERT_EQUAL_UINT32(100, h.records[0].first.uptimeSec);
}

void test_case_and_surrounding_blanks_do_not_matter() {
  FakeHost h;
  SmsCommand c(h, english());
  TEST_ASSERT_TRUE(c.handle(kParentOne, "  #wherEAREyou \n") == SmsCommand::Result::Handled);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "\t#WHEREAREYOU\r\n") == SmsCommand::Result::Handled);
  TEST_ASSERT_EQUAL(2, h.located.size());
}

// Anything but the trigger word alone is an ordinary message for the child.
void test_everything_else_is_a_normal_message() {
  FakeHost h;
  SmsCommand c(h, english());
  for (const char* body : {"School is cancelled", "where are you", "#WhereAreYou today?", "x #WhereAreYou",
                           "#WhereAreYou #WhereAreYou", "#Where Are You", "secret42 pos", "pos", "", "   ", "#"}) {
    TEST_ASSERT_TRUE_MESSAGE(c.handle(kParentOne, body) == SmsCommand::Result::NotACommand, body);
  }
  TEST_ASSERT_EQUAL(0, h.located.size());
  TEST_ASSERT_EQUAL(0, h.records.size());
}

// One word per language; several may be given at the same time.
void test_every_given_trigger_word_works() {
  FakeHost h;
  SmsCommand c(h, withTriggers({"#WhereAreYou", "#WoBistDu", "#O\xC3\xB9" "EsTu"}));
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#wobistdu") == SmsCommand::Result::Handled);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#whereareyou") == SmsCommand::Result::Handled);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#O\xC3\xB9" "ESTU") == SmsCommand::Result::Handled);
  TEST_ASSERT_EQUAL(3, h.located.size());
}

// Only A-Z fold; an accented letter has to match as written.
void test_only_ascii_letters_ignore_case() {
  FakeHost h;
  SmsCommand c(h, withTriggers({"#O\xC3\xB9" "EsTu"}));
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#O\xC3\x99" "ESTU") == SmsCommand::Result::NotACommand);
}

// An empty entry would match every blank message.
void test_empty_and_blank_trigger_entries_are_skipped() {
  FakeHost h;
  SmsCommand c(h, withTriggers({"", "   ", "\t", "#WhereAreYou"}));
  TEST_ASSERT_TRUE(c.handle(kParentOne, "") == SmsCommand::Result::NotACommand);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "   ") == SmsCommand::Result::NotACommand);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#WhereAreYou") == SmsCommand::Result::Handled);
}

// Blanks around a configured word are not part of it.
void test_blanks_around_a_configured_word_are_ignored() {
  FakeHost h;
  SmsCommand c(h, withTriggers({"  #WhereAreYou  "}));
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#WhereAreYou") == SmsCommand::Result::Handled);
}

void test_without_any_trigger_nothing_is_a_command() {
  FakeHost h;
  SmsCommand c(h, SmsCommand::Config());
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#WhereAreYou") == SmsCommand::Result::NotACommand);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "") == SmsCommand::Result::NotACommand);
  TEST_ASSERT_EQUAL(0, h.records.size());
}

void test_a_stranger_gets_nothing_but_is_recorded() {
  FakeHost h;
  SmsCommand c(h, english());
  TEST_ASSERT_TRUE(c.handle(kStranger, "#WhereAreYou") == SmsCommand::Result::Handled);  // not stored as a message either
  TEST_ASSERT_EQUAL(0, h.located.size());
  TEST_ASSERT_EQUAL(1, h.records.size());
  TEST_ASSERT_TRUE(h.records[0].first.outcome == CommandOutcome::Unauthorized);
  TEST_ASSERT_EQUAL_STRING(kStranger, h.records[0].first.phone.c_str());
}

// Even a contact book that wrongly holds an empty entry must not open this.
void test_an_unreadable_sender_is_unauthorized() {
  FakeHost h;
  h.admins.push_back("");
  SmsCommand c(h, english());
  TEST_ASSERT_TRUE(c.handle("", "#WhereAreYou") == SmsCommand::Result::Handled);
  TEST_ASSERT_EQUAL(0, h.located.size());
  TEST_ASSERT_TRUE(h.records[0].first.outcome == CommandOutcome::Unauthorized);
}

void test_a_running_location_reply_is_recorded_as_busy() {
  FakeHost h;
  h.busy = true;
  SmsCommand c(h, english());
  c.handle(kParentOne, "#WhereAreYou");
  TEST_ASSERT_TRUE(h.records[0].first.outcome == CommandOutcome::Busy);
  TEST_ASSERT_EQUAL(0, h.located.size());
}

void test_the_notice_setting_is_passed_on_and_defaults_to_history() {
  TEST_ASSERT_TRUE(SmsCommand::Config().notice == CommandNotice::History);
  FakeHost h;
  SmsCommand c(h, english());
  c.handle(kParentOne, "#WhereAreYou");
  TEST_ASSERT_TRUE(h.records[0].second == CommandNotice::History);
  c.setNotice(CommandNotice::Silent);
  c.handle(kParentOne, "#WhereAreYou");
  TEST_ASSERT_TRUE(h.records[1].second == CommandNotice::Silent);
  // Silent is still recorded: the setting only changes how visible the entry is.
  TEST_ASSERT_EQUAL(2, h.records.size());
}

void test_the_triggers_can_be_changed_at_run_time() {
  FakeHost h;
  SmsCommand c(h, english());
  SmsCommand::Config cfg = withTriggers({"#WoBistDu"});
  cfg.notice = CommandNotice::Notify;
  c.setConfig(cfg);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#WhereAreYou") == SmsCommand::Result::NotACommand);
  TEST_ASSERT_TRUE(c.handle(kParentOne, "#WoBistDu") == SmsCommand::Result::Handled);
  TEST_ASSERT_TRUE(h.records[0].second == CommandNotice::Notify);
}

// The trigger word of each shipped language is a message key; the caller collects
// them all so that a parent may use any of them.
void test_the_shipped_languages_provide_their_trigger_words() {
  i18n::Translator t;
  SmsCommand::Config cfg;
  for (const char* file : {"en", "de", "fr"}) {
    std::ifstream in(std::string("lang/") + file + ".toml");
    std::stringstream ss;
    ss << in.rdbuf();
    i18n::Catalog c;
    TEST_ASSERT_TRUE_MESSAGE(c.parse(ss.str().data(), ss.str().size()), file);
    t.add(c);
  }
  for (const char* language : {"en", "de", "fr"}) {
    TEST_ASSERT_TRUE(t.select(language));
    cfg.triggers.push_back(t.text("sms.command.where"));
  }
  TEST_ASSERT_EQUAL(3, cfg.triggers.size());
  FakeHost h;
  SmsCommand c(h, cfg);
  for (const char* word : {"#whereareyou", "#WOBISTDU", " #OuEsTu "}) {
    TEST_ASSERT_TRUE_MESSAGE(c.handle(kParentOne, word) == SmsCommand::Result::Handled, word);
  }
  TEST_ASSERT_EQUAL(3, h.located.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_an_admin_asking_starts_a_location_reply);
  RUN_TEST(test_case_and_surrounding_blanks_do_not_matter);
  RUN_TEST(test_everything_else_is_a_normal_message);
  RUN_TEST(test_every_given_trigger_word_works);
  RUN_TEST(test_only_ascii_letters_ignore_case);
  RUN_TEST(test_empty_and_blank_trigger_entries_are_skipped);
  RUN_TEST(test_blanks_around_a_configured_word_are_ignored);
  RUN_TEST(test_without_any_trigger_nothing_is_a_command);
  RUN_TEST(test_a_stranger_gets_nothing_but_is_recorded);
  RUN_TEST(test_an_unreadable_sender_is_unauthorized);
  RUN_TEST(test_a_running_location_reply_is_recorded_as_busy);
  RUN_TEST(test_the_notice_setting_is_passed_on_and_defaults_to_history);
  RUN_TEST(test_the_triggers_can_be_changed_at_run_time);
  RUN_TEST(test_the_shipped_languages_provide_their_trigger_words);
  return UNITY_END();
}
