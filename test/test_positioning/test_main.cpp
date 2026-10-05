// Host tests for the receiver reasons and the start-up fix, with the fake receiver, power,
// clock and log.

#include <unity.h>

#include <ProfileBuilder.h>
#include <app/Positioning.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakePower.h>

using Reason = Positioning::Reason;

void setUp() {}
void tearDown() {}

namespace {

struct Rig {
  FakeClock clock;
  FakePower power{&clock};
  FakeGnss gnss{power, clock};
  FakeEventLog log{&clock};
  Profile profile = ProfileBuilder().build();
  Logger logger{log, profile};
  Positioning positioning{clock, power, gnss, logger};

  Rig() { clock.setEpochUtc(1789475520); }
  bool on() { return power.isEnabled(PowerDomain::Positioning); }
  // Advances time one second at a time and polls, like the main loop.
  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      clock.advanceSeconds(1);
      positioning.poll();
    }
  }
};

}  // namespace

void test_the_receiver_is_off_without_a_request() {
  Rig r;
  r.run(10000);
  TEST_ASSERT_FALSE(r.on());
  TEST_ASSERT_FALSE(r.positioning.active());
  TEST_ASSERT_EQUAL(0, r.power.switchOnCount(PowerDomain::Positioning));
  GnssFix none;
  TEST_ASSERT_FALSE(r.positioning.lastKnownFix(none));
}

void test_reasons_are_independent() {
  Rig r;
  r.positioning.want(Reason::PositionPage, true);
  r.positioning.want(Reason::RemoteQuery, true);
  TEST_ASSERT_TRUE(r.on());
  r.positioning.want(Reason::PositionPage, false);          // the page closes ...
  TEST_ASSERT_TRUE(r.on());                                 // ... the query keeps the receiver
  r.positioning.want(Reason::SosPing, true);
  r.positioning.want(Reason::RemoteQuery, false);
  TEST_ASSERT_TRUE(r.on());
  r.positioning.want(Reason::SosPing, false);
  TEST_ASSERT_FALSE(r.on());
}

void test_every_reason_alone_holds_the_receiver() {
  for (int i = 0; i < static_cast<int>(Reason::Count); ++i) {
    Rig r;
    const Reason reason = static_cast<Reason>(i);
    r.positioning.want(reason, true);
    TEST_ASSERT_TRUE_MESSAGE(r.on(), std::to_string(i).c_str());
    TEST_ASSERT_TRUE(r.positioning.wants(reason));
    for (int j = 0; j < static_cast<int>(Reason::Count); ++j)
      if (j != i) TEST_ASSERT_FALSE(r.positioning.wants(static_cast<Reason>(j)));
    r.positioning.want(reason, false);
    TEST_ASSERT_FALSE_MESSAGE(r.on(), std::to_string(i).c_str());
  }
}

void test_holding_a_reason_twice_is_holding_it_once() {
  Rig r;
  r.positioning.want(Reason::TestPage, true);
  r.positioning.want(Reason::TestPage, true);
  r.positioning.want(Reason::TestPage, false);          // one release is enough
  TEST_ASSERT_FALSE(r.on());
}

void test_the_last_released_reason_switches_the_receiver_off() {
  Rig r;
  r.positioning.want(Reason::PositionPage, true);
  TEST_ASSERT_TRUE(r.on());
  r.positioning.want(Reason::PositionPage, false);
  TEST_ASSERT_FALSE(r.on());
  r.positioning.want(Reason::PositionPage, false);      // releasing a reason that is not held changes nothing
  TEST_ASSERT_FALSE(r.on());
}

void test_power_is_switched_once_per_change() {
  Rig r;
  r.positioning.want(Reason::SosChoice, true);
  r.positioning.want(Reason::SosPing, true);            // the ping takes over from the choice ...
  r.positioning.want(Reason::SosChoice, false);         // ... and the receiver is never off in between
  TEST_ASSERT_EQUAL(1, r.power.switchOnCount(PowerDomain::Positioning));
  r.positioning.want(Reason::SosPing, false);
  r.positioning.want(Reason::SosPing, true);
  TEST_ASSERT_EQUAL(2, r.power.switchOnCount(PowerDomain::Positioning));
}

void test_a_switch_that_fails_is_logged_and_tried_again_at_the_next_poll() {
  Rig r;
  r.power.failDomain(PowerDomain::Positioning, true);
  r.positioning.want(Reason::PositionPage, true);
  TEST_ASSERT_FALSE(r.on());
  TEST_ASSERT_TRUE(r.log.contains("cannot be switched on"));
  r.power.failDomain(PowerDomain::Positioning, false);
  r.positioning.poll();
  TEST_ASSERT_TRUE(r.on());
  // The same on the way out: the receiver must not stay on because one switch-off failed.
  r.power.failDomain(PowerDomain::Positioning, true);
  r.positioning.want(Reason::PositionPage, false);
  TEST_ASSERT_TRUE(r.on());
  TEST_ASSERT_TRUE(r.log.contains("cannot be switched off"));
  r.power.failDomain(PowerDomain::Positioning, false);
  r.positioning.poll();
  TEST_ASSERT_FALSE(r.on());
}

void test_the_startup_fix_ends_on_a_usable_fix() {
  Rig r;
  r.gnss.fixAfter(5, 48.1, 11.5, 1.2f);
  r.positioning.startStartupFix();
  TEST_ASSERT_TRUE(r.on());
  r.run(4);
  TEST_ASSERT_TRUE(r.on());                              // no fix yet
  r.run(2);
  TEST_ASSERT_FALSE(r.on());                             // the first usable result is enough
  TEST_ASSERT_FALSE(r.positioning.wants(Reason::StartupFix));
  GnssFix last;
  TEST_ASSERT_TRUE(r.positioning.lastKnownFix(last));
  TEST_ASSERT_TRUE(last.lat > 48.09 && last.lat < 48.11);
}

void test_the_hdop_limit_is_inclusive() {
  Rig r;
  r.gnss.fixAfter(0, 48.1, 11.5, 5.0f);
  r.positioning.startStartupFix();
  r.run(1);
  TEST_ASSERT_FALSE(r.on());                             // HDOP 5.0 is usable
  Rig s;
  s.gnss.fixAfter(0, 48.1, 11.5, 5.1f);
  s.positioning.startStartupFix();
  s.run(1);
  TEST_ASSERT_TRUE(s.on());                              // 5.1 is not
}

void test_an_inaccurate_fix_is_taken_after_a_minute() {
  Rig r;
  r.gnss.fixAfter(2, 48.1, 11.5, 9.0f);
  r.positioning.startStartupFix();
  r.run(Positioning::kAcceptAnySeconds - 1);
  TEST_ASSERT_TRUE(r.on());
  r.run(1);
  TEST_ASSERT_FALSE(r.on());
  GnssFix last;
  TEST_ASSERT_TRUE(r.positioning.lastKnownFix(last));
  TEST_ASSERT_TRUE(last.hdop > 8.9f);
}

void test_the_startup_fix_gives_up_after_three_minutes() {
  Rig r;
  r.gnss.never();
  r.positioning.startStartupFix();
  r.run(Positioning::kStartupFixSeconds - 1);
  TEST_ASSERT_TRUE(r.on());
  r.run(1);
  TEST_ASSERT_FALSE(r.on());
  GnssFix none;
  TEST_ASSERT_FALSE(r.positioning.lastKnownFix(none));
  TEST_ASSERT_TRUE(r.log.contains("without result"));
}

void test_the_position_page_keeps_the_receiver_after_the_startup_fix_ends() {
  Rig r;
  r.gnss.never();
  r.positioning.startStartupFix();
  r.positioning.want(Reason::PositionPage, true);
  r.run(Positioning::kStartupFixSeconds);
  TEST_ASSERT_FALSE(r.positioning.wants(Reason::StartupFix));
  TEST_ASSERT_TRUE(r.on());
  r.positioning.want(Reason::PositionPage, false);
  TEST_ASSERT_FALSE(r.on());
}

void test_the_last_fix_keeps_its_age_whoever_asked_for_the_receiver() {
  Rig r;
  r.gnss.fixAfter(0, 48.1, 11.5, 1.2f);
  r.positioning.want(Reason::PositionPage, true);       // not the start-up fix
  r.run(3);
  r.positioning.want(Reason::PositionPage, false);
  GnssFix last;
  uint32_t age = 99;
  TEST_ASSERT_TRUE(r.positioning.lastKnownFix(last, &age));
  TEST_ASSERT_EQUAL(0, age);
  r.clock.advanceSeconds(60);
  TEST_ASSERT_TRUE(r.positioning.lastKnownFix(last, &age));
  TEST_ASSERT_EQUAL(60, age);                            // it ages although the receiver is off
  TEST_ASSERT_TRUE(r.positioning.lastKnownFix(last));    // the age is optional
}

void test_a_fix_from_before_the_last_switch_on_is_not_used() {
  Rig r;
  r.gnss.fixAfter(0, 48.1, 11.5, 1.0f);
  r.positioning.want(Reason::PositionPage, true);
  r.run(2);
  r.positioning.want(Reason::PositionPage, false);

  // Switched on again, the receiver needs 30 s for a fix. The remembered old fix must not end a
  // new start-up fix, and the receiver must not hand it out as current.
  r.gnss.fixAfter(30, 48.2, 11.6, 1.0f);
  r.positioning.startStartupFix();
  GnssFix now;
  TEST_ASSERT_TRUE(r.positioning.readFix(now));
  TEST_ASSERT_FALSE(now.valid);
  r.run(20);
  TEST_ASSERT_TRUE(r.on());                              // still searching, in spite of the earlier fix
  r.run(11);
  TEST_ASSERT_FALSE(r.on());
  GnssFix last;
  TEST_ASSERT_TRUE(r.positioning.lastKnownFix(last));
  TEST_ASSERT_TRUE(last.lat > 48.19);                    // the new position replaced the old one
}

void test_the_startup_fix_is_logged() {
  Rig r;
  r.gnss.fixAfter(0, 48.1, 11.5, 1.0f);
  r.positioning.startStartupFix();
  TEST_ASSERT_TRUE(r.log.contains("start-up fix requested"));
  r.run(1);
  TEST_ASSERT_TRUE(r.log.contains("start-up fix received"));
}

void test_the_receiver_answers_for_the_sos_ping_only_while_it_runs() {
  Rig r;
  GnssFix fix;
  TEST_ASSERT_FALSE(r.positioning.readFix(fix));        // off: false
  r.gnss.fixAfter(0, 48.1, 11.5, 1.0f);
  r.positioning.want(Reason::SosPing, true);
  TEST_ASSERT_TRUE(r.positioning.readFix(fix));
  TEST_ASSERT_TRUE(fix.valid);
}

void test_a_start_up_fix_runs_out_its_time_even_if_the_receiver_cannot_be_switched_on() {
  Rig r;
  r.power.failDomain(PowerDomain::Positioning, true);
  r.positioning.startStartupFix();
  r.run(Positioning::kStartupFixSeconds);
  TEST_ASSERT_FALSE(r.positioning.wants(Reason::StartupFix));       // not retried for ever
  r.power.failDomain(PowerDomain::Positioning, false);
  r.positioning.poll();
  TEST_ASSERT_FALSE(r.on());
}

void test_the_end_of_a_startup_fix_is_not_claimed_by_other_reasons() {
  Rig r;
  r.gnss.fixAfter(0, 48.1, 11.5, 1.0f);
  r.clock.advanceSeconds(1000);                           // long after start, no start-up fix requested
  r.positioning.want(Reason::PositionPage, true);
  r.run(5);
  TEST_ASSERT_FALSE(r.log.contains("start-up fix"));      // neither "received" nor "without result"
  TEST_ASSERT_TRUE(r.on());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_receiver_is_off_without_a_request);
  RUN_TEST(test_reasons_are_independent);
  RUN_TEST(test_every_reason_alone_holds_the_receiver);
  RUN_TEST(test_holding_a_reason_twice_is_holding_it_once);
  RUN_TEST(test_the_last_released_reason_switches_the_receiver_off);
  RUN_TEST(test_power_is_switched_once_per_change);
  RUN_TEST(test_a_switch_that_fails_is_logged_and_tried_again_at_the_next_poll);
  RUN_TEST(test_the_startup_fix_ends_on_a_usable_fix);
  RUN_TEST(test_the_hdop_limit_is_inclusive);
  RUN_TEST(test_an_inaccurate_fix_is_taken_after_a_minute);
  RUN_TEST(test_the_startup_fix_gives_up_after_three_minutes);
  RUN_TEST(test_the_position_page_keeps_the_receiver_after_the_startup_fix_ends);
  RUN_TEST(test_the_last_fix_keeps_its_age_whoever_asked_for_the_receiver);
  RUN_TEST(test_a_fix_from_before_the_last_switch_on_is_not_used);
  RUN_TEST(test_the_startup_fix_is_logged);
  RUN_TEST(test_a_start_up_fix_runs_out_its_time_even_if_the_receiver_cannot_be_switched_on);
  RUN_TEST(test_the_end_of_a_startup_fix_is_not_claimed_by_other_reasons);
  RUN_TEST(test_the_receiver_answers_for_the_sos_ping_only_while_it_runs);
  return UNITY_END();
}
