// Host tests for the vocabulary of notices and system entries: every code has a text in every
// shipped language, with exactly the placeholders the code supplies.

#include <unity.h>

#include <set>

#include <Languages.h>
#include <app/Notice.h>

using i18n::Catalog;
using i18n::Translator;

void setUp() {}
void tearDown() {}

namespace {

std::set<std::string> placeholdersOf(Translator& t, const char* key) {
  const auto list = Catalog::placeholders(t.text(key));
  return std::set<std::string>(list.begin(), list.end());
}

std::set<std::string> shipped(const Translator& all) {
  std::set<std::string> out;
  for (const auto& l : all.languages()) out.insert(l.code);
  return out;
}

}  // namespace

void test_every_notice_kind_has_a_text_in_every_shipped_language_with_the_right_placeholders() {
  Translator t = languages::loaded();
  for (const std::string& code : shipped(t)) {
    if (code == i18n::kPseudoCode) continue;
    TEST_ASSERT_TRUE(t.select(code));
    for (int k = 0; k < static_cast<int>(NoticeKind::Count); ++k) {
      const NoticeWording& w = noticeWording(static_cast<NoticeKind>(k));
      const std::string where = code + " kind " + std::to_string(k) + " " + w.key;
      TEST_ASSERT_TRUE_MESSAGE(t.text(w.key) != std::string(w.key), where.c_str());
      std::set<std::string> want;
      if (w.name) want.insert("name");
      if (w.time) want.insert("time");
      TEST_ASSERT_TRUE_MESSAGE(placeholdersOf(t, w.key) == want, where.c_str());
    }
  }
  TEST_ASSERT_EQUAL(0, t.fallbackCount());   // no language leans on English for any of them
  TEST_ASSERT_EQUAL(0, t.missingKeys().size());
}

void test_notice_kinds_have_their_own_keys() {
  std::set<std::string> keys;
  for (int k = 0; k < static_cast<int>(NoticeKind::Count); ++k) {
    TEST_ASSERT_TRUE_MESSAGE(keys.insert(noticeWording(static_cast<NoticeKind>(k)).key).second,
                             noticeWording(static_cast<NoticeKind>(k)).key);
  }
  TEST_ASSERT_EQUAL_STRING("school.until", noticeWording(NoticeKind::SchoolTimeUntil).key);
  TEST_ASSERT_EQUAL_STRING("notice.light-off", noticeWording(NoticeKind::LightOff).key);   // the table ends where the enum does
  TEST_ASSERT_EQUAL_STRING("system.location-queried", noticeWording(NoticeKind::LocationQueried).key);
  TEST_ASSERT_EQUAL_STRING("system.query-refused", noticeWording(NoticeKind::QueryRefused).key);
}

void test_an_out_of_range_kind_still_gives_a_wording_instead_of_reading_past_the_table() {
  const NoticeWording& w = noticeWording(NoticeKind::Count);
  TEST_ASSERT_TRUE(w.key != nullptr);
}

void test_every_system_entry_kind_and_call_reason_has_a_text_in_every_shipped_language() {
  Translator t = languages::loaded();
  const struct { const char* kind; bool reason; } kinds[] = {
      {sysentry::kMissedCall, false}, {sysentry::kCallFailed, true},   {sysentry::kLocationQueried, false},
      {sysentry::kQueryRefused, false}, {sysentry::kSmsRejected, false}};
  const char* reasons[] = {sysentry::kBusy, sysentry::kNoAnswer, sysentry::kNoNetwork, sysentry::kSmsOnly,
                           sysentry::kNotConnected};
  for (const std::string& code : shipped(t)) {
    if (code == i18n::kPseudoCode) continue;
    t.select(code);
    for (const auto& k : kinds) {
      const char* key = systemKey(k.kind);
      TEST_ASSERT_TRUE_MESSAGE(key != nullptr, k.kind);
      std::set<std::string> want = {"name"};
      if (k.reason) want.insert("reason");
      TEST_ASSERT_TRUE_MESSAGE(placeholdersOf(t, key) == want, (code + " " + k.kind).c_str());
    }
    for (const char* r : reasons) {
      const char* key = callReasonKey(r);
      TEST_ASSERT_TRUE_MESSAGE(key != nullptr, r);
      TEST_ASSERT_TRUE_MESSAGE(placeholdersOf(t, key).empty(), (code + " " + r).c_str());
      TEST_ASSERT_TRUE_MESSAGE(t.text(key) != std::string(key), (code + " " + r).c_str());
    }
  }
  TEST_ASSERT_EQUAL(0, t.fallbackCount());
}

void test_each_reason_and_kind_has_its_own_key() {
  TEST_ASSERT_EQUAL_STRING("call.reason-busy", callReasonKey(sysentry::kBusy));
  TEST_ASSERT_EQUAL_STRING("call.reason-no-answer", callReasonKey(sysentry::kNoAnswer));
  TEST_ASSERT_EQUAL_STRING("call.reason-no-network", callReasonKey(sysentry::kNoNetwork));
  TEST_ASSERT_EQUAL_STRING("call.reason-sms-only", callReasonKey(sysentry::kSmsOnly));
  TEST_ASSERT_EQUAL_STRING("call.reason-not-connected", callReasonKey(sysentry::kNotConnected));
  TEST_ASSERT_EQUAL_STRING("system.missed-call", systemKey(sysentry::kMissedCall));
  TEST_ASSERT_EQUAL_STRING("system.call-failed", systemKey(sysentry::kCallFailed));
  TEST_ASSERT_EQUAL_STRING("system.location-queried", systemKey(sysentry::kLocationQueried));
  TEST_ASSERT_EQUAL_STRING("system.query-refused", systemKey(sysentry::kQueryRefused));
  TEST_ASSERT_EQUAL_STRING("system.sms-rejected", systemKey(sysentry::kSmsRejected));
}

void test_an_unknown_system_kind_or_reason_has_no_key() {
  TEST_ASSERT_NULL(systemKey("from-the-future"));
  TEST_ASSERT_NULL(systemKey(""));
  TEST_ASSERT_NULL(callReasonKey("on-fire"));
  TEST_ASSERT_NULL(callReasonKey(""));
}

void test_a_notice_compares_by_kind_peer_and_minute() {
  Notice a;
  TEST_ASSERT_TRUE(a.none());
  Notice b;
  b.kind = NoticeKind::CallBusy;
  b.peer = "+491701111111";
  TEST_ASSERT_FALSE(b.none());
  Notice c = b;
  TEST_ASSERT_TRUE(b == c);
  c.peer = "+491702222222";
  TEST_ASSERT_FALSE(b == c);
  c = b;
  c.minute = 5;
  TEST_ASSERT_FALSE(b == c);
  c = b;
  c.kind = NoticeKind::CallNoAnswer;
  TEST_ASSERT_FALSE(b == c);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_notice_kind_has_a_text_in_every_shipped_language_with_the_right_placeholders);
  RUN_TEST(test_notice_kinds_have_their_own_keys);
  RUN_TEST(test_an_out_of_range_kind_still_gives_a_wording_instead_of_reading_past_the_table);
  RUN_TEST(test_every_system_entry_kind_and_call_reason_has_a_text_in_every_shipped_language);
  RUN_TEST(test_each_reason_and_kind_has_its_own_key);
  RUN_TEST(test_an_unknown_system_kind_or_reason_has_no_key);
  RUN_TEST(test_a_notice_compares_by_kind_peer_and_minute);
  return UNITY_END();
}
