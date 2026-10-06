// Host tests for the wording of the core's codes: every notice kind, system entry kind, call
// reason, Wi-Fi reason and network label has words in every shipped language, and a code without
// a catalog entry fails here instead of showing a key on the child's screen.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <set>

#include <Languages.h>
#include <ProfileBuilder.h>
#include <app/SystemEntry.h>
#include <ui/Wording.h>

using namespace people;
using namespace ui;
using i18n::Translator;

void setUp() {}
void tearDown() {}

namespace {

phone::ContactBook book() {
  Profile p = ProfileBuilder().build();
  return p.contacts;
}

// The shipped languages, without the pseudo-language.
std::vector<std::string> shipped(Translator& t) {
  std::vector<std::string> out;
  for (const auto& l : t.languages())
    if (l.code != i18n::kPseudoCode) out.push_back(l.code);
  return out;
}

// Text a user can read: not empty, no catalog key (a dotted lower-case word), no unfilled placeholder.
void expectWords(const std::string& text, const std::string& where) {
  TEST_ASSERT_FALSE_MESSAGE(text.empty(), where.c_str());
  TEST_ASSERT_TRUE_MESSAGE(text.find('{') == std::string::npos, (where + ": " + text).c_str());
  const bool keyLike = text.find('.') != std::string::npos && text.find(' ') == std::string::npos &&
                       text.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos;
  TEST_ASSERT_FALSE_MESSAGE(keyLike, (where + ": " + text).c_str());
}

Message systemMessage(const char* kind, const std::string& peer, const char* argument = "") {
  Message m;
  TEST_ASSERT_TRUE(sysentry::make(m, kind, argument));
  snprintf(m.peer, sizeof m.peer, "%s", peer.c_str());
  return m;
}

}  // namespace

// ---- the clock time ------------------------------------------------------------------------

void test_a_minute_of_the_day_is_written_with_two_digits_each() {
  TEST_ASSERT_EQUAL_STRING("00:00", wording::clockTime(0).c_str());
  TEST_ASSERT_EQUAL_STRING("00:05", wording::clockTime(5).c_str());
  TEST_ASSERT_EQUAL_STRING("13:30", wording::clockTime(13 * 60 + 30).c_str());
  TEST_ASSERT_EQUAL_STRING("23:59", wording::clockTime(1439).c_str());
}

void test_a_minute_outside_the_day_does_not_write_a_wrong_time() {
  TEST_ASSERT_EQUAL_STRING("00:00", wording::clockTime(-1).c_str());      // "no time" is not a time before midnight
  TEST_ASSERT_EQUAL_STRING("00:00", wording::clockTime(1440).c_str());
  TEST_ASSERT_EQUAL_STRING("01:00", wording::clockTime(1440 + 60).c_str());
}

// ---- the date ------------------------------------------------------------------------------

void test_every_weekday_and_month_has_words_in_every_shipped_language_and_none_repeats() {
  Translator t = languages::loaded();
  for (const std::string& code : shipped(t)) {
    t.select(code);
    std::set<std::string> days, months;
    for (int i = 0; i < 12; ++i) {
      AppCore::State::WallClock wall;
      wall.known = true;
      wall.year = 2026;
      wall.month = i + 1;
      wall.day = 5;
      wall.weekday = i % 7;
      const std::string month = wording::dateLine(wall, t);
      expectWords(month, code + " month " + std::to_string(i + 1));
      TEST_ASSERT_TRUE_MESSAGE(months.insert(month).second, month.c_str());
      if (i < 7) {
        expectWords(wording::weekdayName(wall, t), code + " weekday " + std::to_string(i));
        TEST_ASSERT_TRUE_MESSAGE(days.insert(wording::weekdayName(wall, t)).second, code.c_str());
      }
    }
  }
  TEST_ASSERT_EQUAL(0, t.fallbackCount());
  TEST_ASSERT_EQUAL(0, t.missingKeys().size());
}

void test_the_date_is_written_the_way_each_language_writes_it() {
  Translator t = languages::loaded();
  AppCore::State::WallClock wall;
  wall.known = true;
  wall.year = 2026;
  wall.month = 3;
  wall.day = 7;
  wall.weekday = 6;
  TEST_ASSERT_EQUAL_STRING("March 7, 2026", wording::dateLine(wall, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Saturday", wording::weekdayName(wall, t).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("7. M\xC3\xA4rz 2026", wording::dateLine(wall, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Samstag", wording::weekdayName(wall, t).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("7 mars 2026", wording::dateLine(wall, t).c_str());
  TEST_ASSERT_EQUAL_STRING("samedi", wording::weekdayName(wall, t).c_str());
}

void test_without_a_time_there_is_no_date_and_a_wild_number_does_not_read_outside_the_table() {
  Translator t = languages::loaded();
  AppCore::State::WallClock wall;                              // not known
  TEST_ASSERT_EQUAL_STRING("", wording::dateLine(wall, t).c_str());
  TEST_ASSERT_EQUAL_STRING("", wording::weekdayName(wall, t).c_str());
  wall.known = true;
  wall.month = 13;
  wall.weekday = -1;
  TEST_ASSERT_FALSE(wording::dateLine(wall, t).empty());
  TEST_ASSERT_FALSE(wording::weekdayName(wall, t).empty());
}

// ---- the time of a message ----------------------------------------------------------------

void test_a_message_time_is_the_clock_for_today_the_date_before_and_dashes_when_unknown() {
  Translator t = languages::loaded();
  AppCore::State::WallClock now;
  now.known = true;
  now.month = 9;
  now.day = 15;
  now.minuteOfDay = 14 * 60 + 32;
  AppCore::State::WallClock today = now;
  today.minuteOfDay = 5;
  AppCore::State::WallClock yesterday = now;
  yesterday.day = 14;
  yesterday.minuteOfDay = 23 * 60 + 59;
  AppCore::State::WallClock unknown;
  TEST_ASSERT_EQUAL_STRING("00:05", wording::messageTime(today, now, t).c_str());
  TEST_ASSERT_EQUAL_STRING("09/14 23:59", wording::messageTime(yesterday, now, t).c_str());
  TEST_ASSERT_EQUAL_STRING("--:--", wording::messageTime(unknown, now, t).c_str());
  AppCore::State::WallClock lastMonth = now;
  lastMonth.month = 8;                                        // the same day number in another month is another day
  TEST_ASSERT_EQUAL_STRING("08/15 14:32", wording::messageTime(lastMonth, now, t).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("14.09. 23:59", wording::messageTime(yesterday, now, t).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("14/09 23:59", wording::messageTime(yesterday, now, t).c_str());
  // Without a known "now" every day is "another day", and the date is shown.
  t.select("en");
  TEST_ASSERT_EQUAL_STRING("09/15 00:05", wording::messageTime(today, unknown, t).c_str());
}

// ---- names ---------------------------------------------------------------------------------

void test_a_name_is_the_contacts_name_the_number_or_the_word_for_unknown() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  TEST_ASSERT_EQUAL_STRING("Parent One", wording::peerName(kParentOne, contacts, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Parent One", wording::peerName("0049 170 1111111", contacts, t).c_str());   // any spelling
  TEST_ASSERT_EQUAL_STRING(kStranger, wording::peerName(kStranger, contacts, t).c_str());               // a number is never shortened
  TEST_ASSERT_EQUAL_STRING("Unknown", wording::peerName("", contacts, t).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Unbekannt", wording::peerName("", contacts, t).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("Inconnu", wording::peerName("", contacts, t).c_str());
}

// ---- notices -------------------------------------------------------------------------------

void test_every_notice_kind_has_words_in_every_shipped_language() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  for (const std::string& code : shipped(t)) {
    t.select(code);
    for (int k = 0; k < static_cast<int>(NoticeKind::Count); ++k) {
      Notice n;
      n.kind = static_cast<NoticeKind>(k);
      n.peer = kParentOne;
      n.minute = 13 * 60 + 30;
      expectWords(wording::notice(n, contacts, t), code + " notice " + std::to_string(k));
    }
  }
  TEST_ASSERT_EQUAL(0, t.fallbackCount());   // no language leans on English for a code
  TEST_ASSERT_EQUAL(0, t.missingKeys().size());
}

void test_a_notice_is_worded_with_its_name_and_its_time() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  Notice school{NoticeKind::SchoolTimeUntil, "", 13 * 60 + 30};
  TEST_ASSERT_EQUAL_STRING("School time until 13:30", wording::notice(school, contacts, t).c_str());
  Notice busy{NoticeKind::CallBusy, kParentOne, -1};
  TEST_ASSERT_EQUAL_STRING("Parent One: busy", wording::notice(busy, contacts, t).c_str());
  Notice asked{NoticeKind::LocationQueried, kParentTwo, -1};
  TEST_ASSERT_EQUAL_STRING("Location asked by Parent Two", wording::notice(asked, contacts, t).c_str());
  Notice plain{NoticeKind::MessageSent, "", -1};
  TEST_ASSERT_EQUAL_STRING("Message is out", wording::notice(plain, contacts, t).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Schulzeit bis 13:30", wording::notice(school, contacts, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Parent One: besetzt", wording::notice(busy, contacts, t).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("Parent One : occupé", wording::notice(busy, contacts, t).c_str());
}

void test_a_notice_for_a_caller_who_is_not_in_the_book_names_the_number_or_nobody() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  Notice stranger{NoticeKind::CallerInSchoolTime, kStranger, -1};
  TEST_ASSERT_EQUAL_STRING((std::string(kStranger) + " is trying to call").c_str(),
                           wording::notice(stranger, contacts, t).c_str());
  // A kind that names somebody and has nobody to name does not print an empty name.
  Notice nobody{NoticeKind::CallBusy, "", -1};
  TEST_ASSERT_EQUAL_STRING("Unknown: busy", wording::notice(nobody, contacts, t).c_str());
}

void test_no_notice_is_no_words() {
  Translator t = languages::loaded();
  TEST_ASSERT_EQUAL_STRING("", wording::notice(Notice(), book(), t).c_str());
}

void test_a_notice_follows_the_language_and_a_renamed_contact() {
  Translator t = languages::loaded();
  Notice busy{NoticeKind::CallBusy, kParentOne, -1};
  phone::ContactBook contacts = book();
  TEST_ASSERT_EQUAL_STRING("Parent One: busy", wording::notice(busy, contacts, t).c_str());
  const std::string renamed = std::string(kParentOne) + " admin Papa\n";
  contacts.load(renamed.data(), renamed.size());
  TEST_ASSERT_EQUAL_STRING("Papa: busy", wording::notice(busy, contacts, t).c_str());
}

// ---- system entries ------------------------------------------------------------------------

void test_every_system_entry_kind_and_reason_has_words_in_every_shipped_language() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  const char* kinds[] = {sysentry::kMissedCall, sysentry::kLocationQueried, sysentry::kQueryRefused,
                         sysentry::kSmsRejected, sysentry::kLocationQueryFailed};
  const char* reasons[] = {sysentry::kBusy, sysentry::kNoAnswer, sysentry::kNoNetwork, sysentry::kSmsOnly,
                           sysentry::kTelephonyNotReady, sysentry::kModemRestarted, sysentry::kNotConnected};
  for (const std::string& code : shipped(t)) {
    t.select(code);
    for (const char* kind : kinds)
      expectWords(wording::systemEntry(systemMessage(kind, kParentOne), contacts, t), code + " " + kind);
    for (const char* reason : reasons) {
      expectWords(wording::systemEntry(systemMessage(sysentry::kCallFailed, kParentOne, reason), contacts, t),
                  code + " call-failed " + reason);
      expectWords(wording::callReason(reason, t), code + " reason " + reason);
    }
  }
  TEST_ASSERT_EQUAL(0, t.fallbackCount());
  TEST_ASSERT_EQUAL(0, t.missingKeys().size());
}

void test_system_entries_are_worded_from_the_kind_the_peer_and_the_reason() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  TEST_ASSERT_EQUAL_STRING("Missed call from Grandma",
                           wording::systemEntry(systemMessage(sysentry::kMissedCall, kGrandma), contacts, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Call to Parent One failed: busy",
                           wording::systemEntry(systemMessage(sysentry::kCallFailed, kParentOne, sysentry::kBusy), contacts, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Location reply to Parent Two failed",
                           wording::systemEntry(systemMessage(sysentry::kLocationQueryFailed, kParentTwo), contacts, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Missed call from Unknown",
                           wording::systemEntry(systemMessage(sysentry::kMissedCall, ""), contacts, t).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Anruf an Parent One fehlgeschlagen: besetzt",
                           wording::systemEntry(systemMessage(sysentry::kCallFailed, kParentOne, sysentry::kBusy), contacts, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Standort-Antwort an Parent Two fehlgeschlagen",
                           wording::systemEntry(systemMessage(sysentry::kLocationQueryFailed, kParentTwo), contacts, t).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("Appel manqué de Grandma",
                           wording::systemEntry(systemMessage(sysentry::kMissedCall, kGrandma), contacts, t).c_str());
}

void test_an_entry_written_yesterday_is_worded_in_todays_language() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  const Message entry = systemMessage(sysentry::kMissedCall, kParentOne);
  const std::string english = wording::systemEntry(entry, contacts, t);
  t.select("de");
  const std::string german = wording::systemEntry(entry, contacts, t);
  TEST_ASSERT_EQUAL_STRING("Missed call from Parent One", english.c_str());
  TEST_ASSERT_EQUAL_STRING("Verpasster Anruf von Parent One", german.c_str());
}

void test_an_entry_this_version_does_not_know_is_shown_as_it_was_stored() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = book();
  // From a later version: a kind that has no key here.
  TEST_ASSERT_EQUAL_STRING("@from-the-future",
                           wording::systemEntry(systemMessage("from-the-future", kParentOne), contacts, t).c_str());
  // From an earlier version: a sentence, not a code.
  Message old;
  old.source = MsgSource::System;
  snprintf(old.text, sizeof old.text, "%s", "Anruf von Oma verpasst");
  TEST_ASSERT_EQUAL_STRING("Anruf von Oma verpasst", wording::systemEntry(old, contacts, t).c_str());
  // A reason this version does not know is shown as it is, inside the sentence.
  TEST_ASSERT_EQUAL_STRING("Call to Parent One failed: out-of-credit",
                           wording::systemEntry(systemMessage(sysentry::kCallFailed, kParentOne, "out-of-credit"), contacts, t).c_str());
}

// ---- Wi-Fi reasons and the network label -------------------------------------------------------

void test_every_wifi_reason_has_words_in_every_shipped_language() {
  Translator t = languages::loaded();
  const WifiReason reasons[] = {WifiReason::NoHomeNetwork, WifiReason::HomeNetworkUnreachable,
                                WifiReason::AccessPointFailed, WifiReason::AccessPointLost,
                                WifiReason::SwitchedOffByItself};
  std::set<std::string> english;
  for (const std::string& code : shipped(t)) {
    t.select(code);
    std::set<std::string> seen;
    for (WifiReason r : reasons) {
      const std::string text = wording::wifiReason(r, t);
      expectWords(text, code + " wifi reason " + std::to_string(static_cast<int>(r)));
      TEST_ASSERT_TRUE_MESSAGE(seen.insert(text).second, text.c_str());   // each reason has its own words
    }
    TEST_ASSERT_EQUAL_STRING("", wording::wifiReason(WifiReason::None, t).c_str());
  }
  TEST_ASSERT_EQUAL(0, t.fallbackCount());
  TEST_ASSERT_EQUAL(0, t.missingKeys().size());
  t.select("en");
  TEST_ASSERT_EQUAL_STRING("Home network not reachable", wording::wifiReason(WifiReason::HomeNetworkUnreachable, t).c_str());
}

void test_the_network_label_is_the_operator_or_a_word_for_what_there_is() {
  using Label = Connectivity::NetworkLabel;
  Translator t = languages::loaded();
  Label op;
  op.kind = Label::Kind::Operator;
  op.name = "Example Mobile";
  TEST_ASSERT_EQUAL_STRING("Example Mobile", wording::networkLabel(op, t).c_str());
  Label unnamed;
  unnamed.kind = Label::Kind::Operator;
  TEST_ASSERT_EQUAL_STRING("Network", wording::networkLabel(unnamed, t).c_str());   // connected, but no name: not "no network"
  Label none;
  TEST_ASSERT_EQUAL_STRING("No network", wording::networkLabel(none, t).c_str());
  Label radio;
  radio.kind = Label::Kind::Radio;
  TEST_ASSERT_EQUAL_STRING("Radio", wording::networkLabel(radio, t).c_str());
  Label radioOnly;
  radioOnly.kind = Label::Kind::RadioOnly;
  TEST_ASSERT_EQUAL_STRING("Radio only", wording::networkLabel(radioOnly, t).c_str());
  for (const std::string& code : shipped(t)) {
    t.select(code);
    for (const Label* l : {&unnamed, &none, &radio, &radioOnly}) expectWords(wording::networkLabel(*l, t), code);
  }
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Netz", wording::networkLabel(unnamed, t).c_str());
  TEST_ASSERT_EQUAL_STRING("Kein Netz", wording::networkLabel(none, t).c_str());
  TEST_ASSERT_EQUAL(0, t.fallbackCount());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_minute_of_the_day_is_written_with_two_digits_each);
  RUN_TEST(test_a_minute_outside_the_day_does_not_write_a_wrong_time);
  RUN_TEST(test_every_weekday_and_month_has_words_in_every_shipped_language_and_none_repeats);
  RUN_TEST(test_the_date_is_written_the_way_each_language_writes_it);
  RUN_TEST(test_without_a_time_there_is_no_date_and_a_wild_number_does_not_read_outside_the_table);
  RUN_TEST(test_a_message_time_is_the_clock_for_today_the_date_before_and_dashes_when_unknown);
  RUN_TEST(test_a_name_is_the_contacts_name_the_number_or_the_word_for_unknown);
  RUN_TEST(test_every_notice_kind_has_words_in_every_shipped_language);
  RUN_TEST(test_a_notice_is_worded_with_its_name_and_its_time);
  RUN_TEST(test_a_notice_for_a_caller_who_is_not_in_the_book_names_the_number_or_nobody);
  RUN_TEST(test_no_notice_is_no_words);
  RUN_TEST(test_a_notice_follows_the_language_and_a_renamed_contact);
  RUN_TEST(test_every_system_entry_kind_and_reason_has_words_in_every_shipped_language);
  RUN_TEST(test_system_entries_are_worded_from_the_kind_the_peer_and_the_reason);
  RUN_TEST(test_an_entry_written_yesterday_is_worded_in_todays_language);
  RUN_TEST(test_an_entry_this_version_does_not_know_is_shown_as_it_was_stored);
  RUN_TEST(test_every_wifi_reason_has_words_in_every_shipped_language);
  RUN_TEST(test_the_network_label_is_the_operator_or_a_word_for_what_there_is);
  return UNITY_END();
}
