// Host tests for the notice line.

#include <unity.h>

#include <app/Notices.h>
#include <fakes/FakeClock.h>

void setUp() {}
void tearDown() {}

void test_a_newer_notice_replaces_the_older() {
  FakeClock clock;
  Notices notices(clock);
  TEST_ASSERT_TRUE(notices.current().none());
  notices.set(NoticeKind::MessageWaiting);
  TEST_ASSERT_TRUE(notices.current().kind == NoticeKind::MessageWaiting);
  clock.advanceSeconds(3);
  notices.set(NoticeKind::MessageSent);
  TEST_ASSERT_TRUE(notices.current().kind == NoticeKind::MessageSent);
  // The newer one has its own full time: the older one's deadline does not cut it short.
  clock.advanceSeconds(Notices::kSeconds - 1);
  TEST_ASSERT_TRUE(notices.current().kind == NoticeKind::MessageSent);
}

void test_a_notice_carries_its_values_and_the_next_one_forgets_them() {
  FakeClock clock;
  Notices notices(clock);
  notices.set(NoticeKind::SchoolTimeUntil, "", 15 * 60);
  TEST_ASSERT_EQUAL(900, notices.current().minute);
  notices.set(NoticeKind::CallBusy, "+491701111111");
  TEST_ASSERT_EQUAL_STRING("+491701111111", notices.current().peer.c_str());
  TEST_ASSERT_EQUAL(-1, notices.current().minute);   // not left over from the notice before
  notices.set(NoticeKind::MessageSent);
  TEST_ASSERT_EQUAL_STRING("", notices.current().peer.c_str());
}

void test_a_notice_expires() {
  FakeClock clock;
  Notices notices(clock);
  clock.advanceSeconds(100);
  notices.set(NoticeKind::CallNotAllowed);
  clock.advanceSeconds(Notices::kSeconds - 1);
  TEST_ASSERT_FALSE(notices.current().none());
  clock.advanceSeconds(1);
  TEST_ASSERT_TRUE(notices.current().none());
  TEST_ASSERT_EQUAL_STRING("", notices.current().peer.c_str());
  clock.advanceSeconds(1000);
  TEST_ASSERT_TRUE(notices.current().none());          // and stays gone
}

void test_a_notice_can_be_cleared() {
  FakeClock clock;
  Notices notices(clock);
  notices.set(NoticeKind::QueueFull);
  notices.clear();
  TEST_ASSERT_TRUE(notices.current().none());
  notices.set(NoticeKind::QueueFull);
  TEST_ASSERT_FALSE(notices.current().none());          // a cleared line can be used again
}

void test_every_kind_can_be_shown() {
  FakeClock clock;
  Notices notices(clock);
  for (int k = 0; k < static_cast<int>(NoticeKind::Count); ++k) {
    const NoticeKind kind = static_cast<NoticeKind>(k);
    notices.set(kind, "+491701111111", 810);
    TEST_ASSERT_TRUE_MESSAGE(notices.current().kind == kind, std::to_string(k).c_str());
    TEST_ASSERT_EQUAL_STRING("+491701111111", notices.current().peer.c_str());
    TEST_ASSERT_EQUAL(810, notices.current().minute);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_newer_notice_replaces_the_older);
  RUN_TEST(test_a_notice_carries_its_values_and_the_next_one_forgets_them);
  RUN_TEST(test_every_kind_can_be_shown);
  RUN_TEST(test_a_notice_expires);
  RUN_TEST(test_a_notice_can_be_cleared);
  return UNITY_END();
}
