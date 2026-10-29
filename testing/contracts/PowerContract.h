#pragma once

// The contract of Power. Rig:
//   Power& power()
//   bool breakDomain(PowerDomain)   make that domain refuse to switch; false if not possible
//   bool plugIn(ChargeState)        Charging or Full on external power; false if not possible
//   bool unplug()                   on battery; false if not possible
//   bool forbidPowerOff()           powerOff() will return false; false if not possible
// (powerOff() is never called when it could switch the device off for real.)

#include <hal/Power.h>

#include "Contract.h"

namespace power_contract {
inline const PowerDomain kDomains[] = {PowerDomain::Cellular, PowerDomain::Positioning, PowerDomain::Frontlight,
                                       PowerDomain::Radio};
}

template <class Rig>
struct PowerContract {
  static void switching_a_domain_shows_in_isEnabled() {
    Rig rig;
    for (PowerDomain d : power_contract::kDomains) {
      if (!rig.power().setEnabled(d, true)) continue;  // a domain that cannot be switched
      TEST_ASSERT_TRUE(rig.power().isEnabled(d));
      TEST_ASSERT_TRUE(rig.power().setEnabled(d, false));
      TEST_ASSERT_FALSE(rig.power().isEnabled(d));
    }
  }

  static void switching_to_the_state_it_has_is_a_success() {
    Rig rig;
    for (PowerDomain d : power_contract::kDomains) {
      if (!rig.power().setEnabled(d, true)) continue;
      TEST_ASSERT_TRUE(rig.power().setEnabled(d, true));
      TEST_ASSERT_TRUE(rig.power().isEnabled(d));
      TEST_ASSERT_TRUE(rig.power().setEnabled(d, false));
      TEST_ASSERT_TRUE(rig.power().setEnabled(d, false));
      TEST_ASSERT_FALSE(rig.power().isEnabled(d));
    }
  }

  static void the_domains_are_independent() {
    Rig rig;
    bool before[4];
    for (int i = 0; i < 4; ++i) before[i] = rig.power().isEnabled(power_contract::kDomains[i]);
    if (!rig.power().setEnabled(PowerDomain::Frontlight, !before[2])) CONTRACT_IGNORE_AND_RETURN("the front light cannot be switched");
    TEST_ASSERT_EQUAL(before[0], rig.power().isEnabled(PowerDomain::Cellular));
    TEST_ASSERT_EQUAL(before[1], rig.power().isEnabled(PowerDomain::Positioning));
    TEST_ASSERT_EQUAL(before[3], rig.power().isEnabled(PowerDomain::Radio));
  }

  // A domain that cannot be switched says so and stays as it was.
  static void a_domain_that_cannot_be_switched_reports_failure_and_keeps_its_state() {
    for (PowerDomain d : power_contract::kDomains) {
      Rig rig;
      CONTRACT_NEEDS(rig.breakDomain(d));
      const bool was = rig.power().isEnabled(d);
      TEST_ASSERT_FALSE(rig.power().setEnabled(d, !was));
      TEST_ASSERT_EQUAL(was, rig.power().isEnabled(d));
    }
  }

  static void the_battery_percent_is_0_to_100_or_minus_one() {
    Rig rig;
    const int percent = rig.power().batteryPercent();
    TEST_ASSERT_TRUE(percent >= -1 && percent <= 100);
  }

  static void the_cell_voltage_is_a_plausible_number_of_millivolts_or_minus_one() {
    Rig rig;
    const int millivolts = rig.power().batteryMillivolts();
    TEST_ASSERT_TRUE(millivolts == -1 || (millivolts >= 2500 && millivolts <= 4500));
  }

  static void the_remaining_time_is_a_number_of_minutes_or_minus_one() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.power().batteryMinutesRemaining() >= -1);
  }

  // Charging and Full need a supply; the remaining time is unknown while charging;
  // on battery there is no supply.
  static void charging_needs_a_supply_and_the_remaining_time_is_unknown_while_charging() {
    Rig rig;
    CONTRACT_NEEDS(rig.plugIn(ChargeState::Charging));
    TEST_ASSERT_TRUE(rig.power().externalPower());
    TEST_ASSERT_TRUE(rig.power().chargeState() == ChargeState::Charging);
    TEST_ASSERT_EQUAL(-1, rig.power().batteryMinutesRemaining());
    CONTRACT_NEEDS(rig.plugIn(ChargeState::Full));
    TEST_ASSERT_TRUE(rig.power().externalPower());  // full is not "no cable"
    TEST_ASSERT_TRUE(rig.power().chargeState() == ChargeState::Full);
  }

  static void on_battery_there_is_no_supply() {
    Rig rig;
    CONTRACT_NEEDS(rig.plugIn(ChargeState::Charging));
    CONTRACT_NEEDS(rig.unplug());
    TEST_ASSERT_FALSE(rig.power().externalPower());
    TEST_ASSERT_TRUE(rig.power().chargeState() == ChargeState::Discharging);
  }

  static void a_refused_power_off_returns_false_and_changes_nothing() {
    Rig rig;
    CONTRACT_NEEDS(rig.forbidPowerOff());
    bool before[4];
    for (int i = 0; i < 4; ++i) before[i] = rig.power().isEnabled(power_contract::kDomains[i]);
    TEST_ASSERT_FALSE(rig.power().powerOff());
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(before[i], rig.power().isEnabled(power_contract::kDomains[i]));
  }
};

#define RUN_POWER_CONTRACT(Rig)                                                                            \
  RUN_TEST(PowerContract<Rig>::switching_a_domain_shows_in_isEnabled);                                     \
  RUN_TEST(PowerContract<Rig>::switching_to_the_state_it_has_is_a_success);                                \
  RUN_TEST(PowerContract<Rig>::the_domains_are_independent);                                              \
  RUN_TEST(PowerContract<Rig>::a_domain_that_cannot_be_switched_reports_failure_and_keeps_its_state);     \
  RUN_TEST(PowerContract<Rig>::the_battery_percent_is_0_to_100_or_minus_one);                             \
  RUN_TEST(PowerContract<Rig>::the_cell_voltage_is_a_plausible_number_of_millivolts_or_minus_one);         \
  RUN_TEST(PowerContract<Rig>::the_remaining_time_is_a_number_of_minutes_or_minus_one);                   \
  RUN_TEST(PowerContract<Rig>::charging_needs_a_supply_and_the_remaining_time_is_unknown_while_charging); \
  RUN_TEST(PowerContract<Rig>::on_battery_there_is_no_supply);                                            \
  RUN_TEST(PowerContract<Rig>::a_refused_power_off_returns_false_and_changes_nothing)
