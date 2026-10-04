#pragma once

// The contract of GnssReceiver. Rig:
//   GnssReceiver& gnss()
//   Power& power()                    the power that feeds the receiver
//   void advanceSeconds(uint32_t)
//   bool scriptFix(lat, lon, hdop, afterSeconds)   after switch-on the receiver finds this
//                                      position `afterSeconds` later; false if not possible
//   bool scriptNoFix()                 it never finds a position; false if not possible

#include <cmath>

#include <hal/GnssReceiver.h>
#include <hal/Power.h>

#include "Contract.h"

template <class Rig>
struct GnssContract {
  static void on(Rig& rig) { TEST_ASSERT_TRUE(rig.power().setEnabled(PowerDomain::Positioning, true)); }
  static void off(Rig& rig) { TEST_ASSERT_TRUE(rig.power().setEnabled(PowerDomain::Positioning, false)); }

  static void assert_sound(const GnssFix& f) {
    TEST_ASSERT_TRUE(std::isfinite(f.lat) && std::isfinite(f.lon));
    TEST_ASSERT_TRUE(std::fabs(f.lat) <= 90.0 && std::fabs(f.lon) <= 180.0);
    TEST_ASSERT_TRUE(std::isfinite(f.hdop) && f.hdop >= 0.0f);
  }

  // Switched off: false, and the caller's variable is left alone.
  static void a_receiver_that_is_off_returns_false_and_leaves_the_fix_alone() {
    Rig rig;
    off(rig);
    GnssFix fix;
    fix.lat = 12.5;
    fix.sats = 9;
    TEST_ASSERT_FALSE(rig.gnss().readFix(fix));
    TEST_ASSERT_TRUE(fix.lat == 12.5);
    TEST_ASSERT_EQUAL(9, fix.sats);
  }

  static void a_receiver_that_runs_returns_true_with_or_without_a_position() {
    Rig rig;
    CONTRACT_NEEDS(rig.scriptNoFix());
    on(rig);
    GnssFix fix;
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_FALSE(fix.valid);
    rig.advanceSeconds(600);
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_FALSE(fix.valid);  // searching never turns into a made-up position
  }

  static void a_valid_fix_has_sound_numbers() {
    Rig rig;
    CONTRACT_NEEDS(rig.scriptFix(48.13745, 11.57550, 1.5f, 5));
    on(rig);
    GnssFix fix;
    for (int t = 0; t < 40; t += 5) {
      TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
      if (fix.valid) assert_sound(fix);
      rig.advanceSeconds(5);
    }
    TEST_ASSERT_TRUE(fix.valid);
  }

  static void no_position_before_the_receiver_has_found_one() {
    Rig rig;
    CONTRACT_NEEDS(rig.scriptFix(48.13745, 11.57550, 1.5f, 20));
    on(rig);
    GnssFix fix;
    rig.advanceSeconds(5);
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_FALSE(fix.valid);
    rig.advanceSeconds(20);
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_TRUE(fix.valid);
  }

  static void switching_off_ends_the_readings() {
    Rig rig;
    CONTRACT_NEEDS(rig.scriptFix(48.13745, 11.57550, 1.5f, 1));
    on(rig);
    rig.advanceSeconds(5);
    GnssFix fix;
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    off(rig);
    TEST_ASSERT_FALSE(rig.gnss().readFix(fix));
  }

  // The device would otherwise report yesterday's position as if it were current.
  static void after_a_restart_the_old_position_is_not_returned() {
    Rig rig;
    CONTRACT_NEEDS(rig.scriptFix(48.13745, 11.57550, 1.5f, 10));
    on(rig);
    rig.advanceSeconds(30);
    GnssFix fix;
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_TRUE(fix.valid);
    off(rig);
    on(rig);
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_FALSE(fix.valid);
  }

  // Off and on between two readings: the receiver still starts afresh.
  static void a_restart_between_two_readings_is_not_missed() {
    Rig rig;
    CONTRACT_NEEDS(rig.scriptFix(48.13745, 11.57550, 1.5f, 10));
    on(rig);
    rig.advanceSeconds(30);
    GnssFix fix;
    rig.gnss().readFix(fix);
    TEST_ASSERT_TRUE(fix.valid);
    off(rig);
    on(rig);
    rig.advanceSeconds(2);
    TEST_ASSERT_TRUE(rig.gnss().readFix(fix));
    TEST_ASSERT_FALSE(fix.valid);
  }
};

#define RUN_GNSS_CONTRACT(Rig)                                                                       \
  RUN_TEST(GnssContract<Rig>::a_receiver_that_is_off_returns_false_and_leaves_the_fix_alone);        \
  RUN_TEST(GnssContract<Rig>::a_receiver_that_runs_returns_true_with_or_without_a_position);         \
  RUN_TEST(GnssContract<Rig>::a_valid_fix_has_sound_numbers);                                        \
  RUN_TEST(GnssContract<Rig>::no_position_before_the_receiver_has_found_one);                        \
  RUN_TEST(GnssContract<Rig>::switching_off_ends_the_readings);                                      \
  RUN_TEST(GnssContract<Rig>::after_a_restart_the_old_position_is_not_returned);                     \
  RUN_TEST(GnssContract<Rig>::a_restart_between_two_readings_is_not_missed)
