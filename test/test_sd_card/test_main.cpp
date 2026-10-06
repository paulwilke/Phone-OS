// The SD card as the device sees it: the mount sequence at descending clocks, a missing card that is said and
// not fatal, a card that is pushed in later, and a card that fails and is taken for pulled out.

#include <unity.h>

#include <BoardStorageRigs.h>
#include <board/PinMap.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_a_card_that_answers_is_mounted_at_the_first_clock_and_said_once() {
  SdStack s(false);
  TEST_ASSERT_TRUE(s.card.begin(s.bootLog));
  TEST_ASSERT_TRUE(s.card.ready());
  TEST_ASSERT_EQUAL(1, s.volume.mountAttempts().size());
  TEST_ASSERT_EQUAL(4000000, s.volume.mountAttempts()[0]);
  TEST_ASSERT_EQUAL(1, s.bootLog.lines().size());
  TEST_ASSERT_TRUE(s.bootLog.contains("SD card: ready"));
}

// The card hangs on the bus that the display and the radio share: a slower clock is the next try.
void test_a_card_that_does_not_stand_4_mhz_is_found_at_1_mhz_and_the_volume_is_released_between() {
  SdStack s(false);
  s.volume.mountsOnlyAtOrBelow(1000000);
  TEST_ASSERT_TRUE(s.card.begin(s.bootLog));
  TEST_ASSERT_EQUAL(2, s.volume.mountAttempts().size());
  TEST_ASSERT_EQUAL(1000000, s.volume.mountAttempts()[1]);
  TEST_ASSERT_TRUE(s.bootLog.contains("found only at 1000 kHz"));
  // 100 ms of quiet first, 250 ms before the second try.
  TEST_ASSERT_EQUAL(350, s.delay.elapsedMs());
}

void test_a_card_that_needs_400_khz_is_found_there() {
  SdStack s(false);
  s.volume.mountsOnlyAtOrBelow(400000);
  TEST_ASSERT_TRUE(s.card.begin(s.bootLog));
  TEST_ASSERT_EQUAL(3, s.volume.mountAttempts().size());
}

void test_a_missing_card_is_said_with_the_hint_about_the_partition_table_and_is_not_fatal() {
  SdStack s(false);
  s.volume.removeCard();
  TEST_ASSERT_FALSE(s.card.begin(s.bootLog));
  TEST_ASSERT_FALSE(s.card.ready());
  TEST_ASSERT_EQUAL(3, s.volume.mountAttempts().size());
  TEST_ASSERT_TRUE(s.bootLog.contains("not found or not readable"));
  TEST_ASSERT_TRUE(s.bootLog.contains("GPT"));
  TEST_ASSERT_TRUE(s.bootLog.contains("defaults"));
}

void test_a_card_with_an_unreadable_volume_is_a_missing_card() {
  SdStack s(false);
  s.volume.refuseMount(true);
  TEST_ASSERT_FALSE(s.card.begin(s.bootLog));
}

// Display, card and radio share the bus: none of the other two may listen while the card is asked.
void test_all_three_chip_selects_are_held_high_before_the_card_is_asked() {
  SdStack s(false);
  s.card.begin(s.bootLog);
  for (uint8_t cs : {pin::kSdCs, pin::kDisplayCs, pin::kRadioCs}) {
    TEST_ASSERT_TRUE(s.gpio.mode(cs) == Gpio::Mode::Output);
    TEST_ASSERT_TRUE(s.gpio.level(cs));
  }
}

void test_a_card_pushed_in_later_is_found_by_the_retry_every_30_seconds() {
  SdStack s(false);
  s.volume.removeCard();
  s.card.begin(s.bootLog);
  s.volume.insertCard();
  s.card.poll(10000, s.bootLog);
  TEST_ASSERT_FALSE(s.card.ready());
  s.card.poll(30000, s.bootLog);
  TEST_ASSERT_TRUE(s.card.ready());
}

void test_a_card_that_was_taken_out_is_noticed_and_asked_for_again_later() {
  SdStack s;
  s.volume.removeCard();
  s.card.poll(1000, s.bootLog);
  TEST_ASSERT_FALSE(s.card.ready());
  TEST_ASSERT_TRUE(s.bootLog.contains("SD card: gone"));
  s.volume.insertCard();
  s.card.poll(1000 + 30000, s.bootLog);
  TEST_ASSERT_TRUE(s.card.ready());
}

void test_three_failed_operations_in_a_row_count_as_a_pulled_card_but_a_success_resets_the_count() {
  SdStack s;
  s.card.operationFailed();
  s.card.operationFailed();
  s.card.operationSucceeded();
  s.card.operationFailed();
  s.card.operationFailed();
  TEST_ASSERT_TRUE(s.card.ready());
  s.card.operationFailed();
  TEST_ASSERT_FALSE(s.card.ready());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_card_that_answers_is_mounted_at_the_first_clock_and_said_once);
  RUN_TEST(test_a_card_that_does_not_stand_4_mhz_is_found_at_1_mhz_and_the_volume_is_released_between);
  RUN_TEST(test_a_card_that_needs_400_khz_is_found_there);
  RUN_TEST(test_a_missing_card_is_said_with_the_hint_about_the_partition_table_and_is_not_fatal);
  RUN_TEST(test_a_card_with_an_unreadable_volume_is_a_missing_card);
  RUN_TEST(test_all_three_chip_selects_are_held_high_before_the_card_is_asked);
  RUN_TEST(test_a_card_pushed_in_later_is_found_by_the_retry_every_30_seconds);
  RUN_TEST(test_a_card_that_was_taken_out_is_noticed_and_asked_for_again_later);
  RUN_TEST(test_three_failed_operations_in_a_row_count_as_a_pulled_card_but_a_success_resets_the_count);
  return UNITY_END();
}
