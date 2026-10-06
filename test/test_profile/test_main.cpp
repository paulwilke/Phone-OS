// Host tests for the profile: its defaults and the two questions it answers (who may be
// called, whom the SOS button reaches), plus school time on a known clock.

#include <unity.h>

#include <ProfileBuilder.h>
#include <app/Logger.h>
#include <fakes/FakeEventLog.h>

using namespace people;

void setUp() {}
void tearDown() {}

namespace {
// Tuesday 2026-09-15 14:32 in a zone two hours east of UTC (the instant the earlier test
// suites used for "in school time").
constexpr uint32_t kTuesdayAfternoon = 1789475520;
constexpr int kTz = 120;
}  // namespace

void test_defaults_match_the_defaults() {
  const Profile p;
  TEST_ASSERT_TRUE(p.messageVibration);
  TEST_ASSERT_TRUE(p.callVibration);
  TEST_ASSERT_FALSE(p.quietCallVibration);   // a call in school time is rejected silently
  TEST_ASSERT_TRUE(p.modemSleep);
  TEST_ASSERT_FALSE(p.freeText);
  TEST_ASSERT_FALSE(p.radioAllowed);
  TEST_ASSERT_FALSE(p.parentTools);
  TEST_ASSERT_TRUE(p.commandNotice == CommandNotice::History);
  TEST_ASSERT_TRUE(p.logLevel == loglevel::Level::All);
  TEST_ASSERT_EQUAL_STRING("en", p.smsLanguage.c_str());
  TEST_ASSERT_EQUAL_STRING(maplink::defaultTemplate(), p.mapTemplate.c_str());
  TEST_ASSERT_EQUAL_STRING("", p.childName.c_str());
  TEST_ASSERT_NULL(p.timeZone);
  TEST_ASSERT_EQUAL(0, p.contacts.count());
  TEST_ASSERT_EQUAL(0, p.quietHours.count());
  TEST_ASSERT_EQUAL(0, p.callTargets.size());
  TEST_ASSERT_EQUAL(0, p.extraTriggers.size());
}

void test_an_unreadable_quiet_hour_window_is_refused_with_a_reason() {
  Profile p;
  TEST_ASSERT_TRUE(p.quietHours.add("mon-fri", "25:00", "13:30") == QuietHours::AddResult::BadTime);
  TEST_ASSERT_TRUE(p.quietHours.add("someday", "08:00", "13:30") == QuietHours::AddResult::BadDays);
  TEST_ASSERT_TRUE(p.quietHours.add("mon", "08:00", "08:00") == QuietHours::AddResult::EmptyWindow);
  TEST_ASSERT_EQUAL(0, p.quietHours.count());
  TEST_ASSERT_FALSE(p.schoolTime(kTuesdayAfternoon, kTz));   // a refused window locks nothing
}

void test_school_time_needs_a_known_time() {
  const Profile p = ProfileBuilder().schoolTime("tue", "14:00", "15:00").build();
  int end = -1;
  TEST_ASSERT_TRUE(p.schoolTime(kTuesdayAfternoon, kTz, &end));
  TEST_ASSERT_EQUAL(15 * 60, end);
  TEST_ASSERT_FALSE(p.schoolTime(kTuesdayAfternoon + 3600, kTz));   // 15:32
  TEST_ASSERT_FALSE(p.schoolTime(0, kTz));                          // the time is not known: no lock
  TEST_ASSERT_FALSE(p.schoolTime(0, 0));
}

void test_the_sos_recipient_is_the_target_or_the_first_call_contact() {
  Profile p = ProfileBuilder().callTargets({kParentTwo, kParentOne}).build();
  TEST_ASSERT_EQUAL_STRING(kParentTwo, p.sosRecipient("").c_str());          // the first call target
  TEST_ASSERT_EQUAL_STRING(kGrandma, p.sosRecipient(kGrandma).c_str());      // an explicit target wins
  const Profile none = ProfileBuilder().callTargets({}).build();
  TEST_ASSERT_EQUAL_STRING("", none.sosRecipient("").c_str());              // nobody: there is no SOS button
  const Profile blank = ProfileBuilder().callTargets({"", kParentOne}).build();
  TEST_ASSERT_EQUAL_STRING(kParentOne, blank.sosRecipient("").c_str());     // an empty target is skipped
  TEST_ASSERT_EQUAL_STRING(kStranger, none.sosRecipient(kStranger).c_str());   // the target needs no list entry
}

void test_who_may_be_called() {
  const Profile p = ProfileBuilder().callTargets({kParentOne, "+491701234567"}).build();
  TEST_ASSERT_TRUE(p.mayCall(kParentOne));
  TEST_ASSERT_TRUE(p.mayCall("0170 1111111"));      // another spelling of a contact
  TEST_ASSERT_TRUE(p.mayCall("0049 170 1111111"));
  TEST_ASSERT_TRUE(p.mayCall(kGrandma));            // on the list, not a parent
  TEST_ASSERT_TRUE(p.mayCall("+491701234567"));     // a call target that is not a contact
  TEST_ASSERT_TRUE(p.mayCall("0170 1234567"));      // ... in another spelling
  TEST_ASSERT_FALSE(p.mayCall(kStranger));
  TEST_ASSERT_FALSE(p.mayCall(""));
  TEST_ASSERT_FALSE(p.mayCall("Provider"));         // not a number: never "the same" as anything
}

void test_who_may_reach_the_child() {
  const Profile p = ProfileBuilder().callTargets({"+491701234567"}).build();
  TEST_ASSERT_TRUE(p.mayReceiveFrom(kParentOne));
  TEST_ASSERT_TRUE(p.mayReceiveFrom("0170 1111111"));      // every spelling of a contact
  TEST_ASSERT_TRUE(p.mayReceiveFrom("0170-1111111"));
  TEST_ASSERT_TRUE(p.mayReceiveFrom("0049 170 1111111"));
  TEST_ASSERT_TRUE(p.mayReceiveFrom("+49 170 1111111"));
  TEST_ASSERT_TRUE(p.mayReceiveFrom(kParentTwo));
  TEST_ASSERT_TRUE(p.mayReceiveFrom(kGrandma));            // any role
  TEST_ASSERT_TRUE(p.mayReceiveFrom("0170 1234567"));      // the target of a call action
  TEST_ASSERT_FALSE(p.mayReceiveFrom(kStranger));
  TEST_ASSERT_FALSE(p.mayReceiveFrom("0170 9999999"));     // the stranger in another spelling
  TEST_ASSERT_FALSE(p.mayReceiveFrom(""));                 // nobody
  TEST_ASSERT_FALSE(p.mayReceiveFrom("Provider"));         // a name
  TEST_ASSERT_FALSE(p.mayReceiveFrom("12345"));            // a short code
  TEST_ASSERT_FALSE(p.mayReceiveFrom("0170 11111111"));    // one digit more than a contact
}

void test_receiving_and_calling_are_one_notion() {
  // They must never drift apart: every number decides the same for both.
  const Profile p = ProfileBuilder().callTargets({"+491701234567"}).build();
  for (const char* n : {"+491701111111", "0170 1111111", "+493012345678", "0170 1234567", "+491709999999", "", "Provider",
                        "12345", "0049 170 1111111", "+4917"})
    TEST_ASSERT_EQUAL_MESSAGE(p.mayCall(n), p.mayReceiveFrom(n), n);
}

void test_a_call_target_is_compared_with_the_country_code_of_the_book() {
  Profile p;
  p.contacts.setCountryCode("+34");
  p.callTargets = {"+34600123456"};
  TEST_ASSERT_TRUE(p.mayCall("0600 123456"));         // a national number, completed with +34
  TEST_ASSERT_FALSE(p.mayCall("0600 123457"));
}

void test_the_builder_makes_the_usual_contacts_and_can_start_empty() {
  const Profile p = ProfileBuilder().build();
  TEST_ASSERT_EQUAL(3, p.contacts.count());
  TEST_ASSERT_TRUE(p.contacts.isAdmin(kParentOne));
  TEST_ASSERT_TRUE(p.contacts.isAdmin(kParentTwo));
  TEST_ASSERT_FALSE(p.contacts.isAdmin(kGrandma));
  TEST_ASSERT_EQUAL_STRING("Parent One", p.contacts.displayName(kParentOne));
  TEST_ASSERT_EQUAL_STRING("Parent One", p.contacts.displayName("0170 1111111"));
  const Profile empty = ProfileBuilder().noContacts().build();
  TEST_ASSERT_EQUAL(0, empty.contacts.count());
  TEST_ASSERT_EQUAL(0, empty.callTargets.size());
  const Profile berlin = ProfileBuilder().timeZone("Europe/Berlin").build();
  TEST_ASSERT_NOT_NULL(berlin.timeZone);
}

void test_the_country_code_of_the_contact_book_is_readable() {
  Profile p;
  TEST_ASSERT_EQUAL_STRING("+49", p.contacts.countryCode());
  TEST_ASSERT_TRUE(p.contacts.setCountryCode("+34"));
  TEST_ASSERT_EQUAL_STRING("+34", p.contacts.countryCode());
}

void test_the_log_applies_the_log_level_of_the_profile_but_never_drops_what_must_stay() {
  struct Case { loglevel::Level level; bool detail, event, error, always; };
  const Case cases[] = {{loglevel::Level::All, true, true, true, true},
                        {loglevel::Level::Events, false, true, true, true},
                        {loglevel::Level::Errors, false, false, true, true}};
  for (const Case& c : cases) {
    Profile profile = ProfileBuilder().logLevel(c.level).build();
    FakeEventLog out;
    Logger log(out, profile);
    log.log("Modem woke up");                   // detail
    log.log("Message received");                // event
    log.log("SMS rejected: +CMS ERROR: 500");   // error
    log.log("Remote query answer sent");        // always
    TEST_ASSERT_EQUAL(c.detail, out.contains("woke up"));
    TEST_ASSERT_EQUAL(c.event, out.contains("Message received"));
    TEST_ASSERT_EQUAL(c.error, out.contains("rejected"));
    TEST_ASSERT_EQUAL(c.always, out.contains("Remote query"));
  }
}

void test_the_log_follows_a_level_that_changes_while_the_core_runs() {
  Profile profile;
  FakeEventLog out;
  Logger log(out, profile);
  log.log("Modem woke up");
  profile.logLevel = loglevel::Level::Errors;
  log.log("Modem slept");
  TEST_ASSERT_TRUE(out.contains("woke up"));
  TEST_ASSERT_FALSE(out.contains("slept"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_match_the_defaults);
  RUN_TEST(test_an_unreadable_quiet_hour_window_is_refused_with_a_reason);
  RUN_TEST(test_school_time_needs_a_known_time);
  RUN_TEST(test_the_sos_recipient_is_the_target_or_the_first_call_contact);
  RUN_TEST(test_who_may_be_called);
  RUN_TEST(test_who_may_reach_the_child);
  RUN_TEST(test_receiving_and_calling_are_one_notion);
  RUN_TEST(test_a_call_target_is_compared_with_the_country_code_of_the_book);
  RUN_TEST(test_the_builder_makes_the_usual_contacts_and_can_start_empty);
  RUN_TEST(test_the_country_code_of_the_contact_book_is_readable);
  RUN_TEST(test_the_log_applies_the_log_level_of_the_profile_but_never_drops_what_must_stay);
  RUN_TEST(test_the_log_follows_a_level_that_changes_while_the_core_runs);
  return UNITY_END();
}
