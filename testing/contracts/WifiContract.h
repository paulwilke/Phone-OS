#pragma once

// The contract of Wifi. Rig: `Wifi& wifi()`,
//   bool finishConnecting(bool success)   the station attempt ends; false if not possible
//   bool failAccessPoint(bool)            a valid access point fails to start; false if not possible

#include <hal/Wifi.h>

#include "Contract.h"

template <class Rig>
struct WifiContract {
  static void it_starts_off_without_an_address() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
    TEST_ASSERT_EQUAL_STRING("", rig.wifi().address().c_str());
  }

  static void a_station_attempt_is_under_way_and_has_no_address_yet() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.wifi().startStation("Home", "secret"));
    const WifiState s = rig.wifi().state();
    TEST_ASSERT_TRUE(s == WifiState::Connecting || s == WifiState::Connected || s == WifiState::Failed);
    if (s == WifiState::Connecting) TEST_ASSERT_EQUAL_STRING("", rig.wifi().address().c_str());
  }

  static void a_station_that_signs_in_has_an_address() {
    Rig rig;
    rig.wifi().startStation("Home", "secret");
    CONTRACT_NEEDS(rig.finishConnecting(true));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Connected);
    TEST_ASSERT_TRUE(!rig.wifi().address().empty());
  }

  static void a_station_that_cannot_sign_in_fails_without_an_address() {
    Rig rig;
    rig.wifi().startStation("Home", "wrong");
    CONTRACT_NEEDS(rig.finishConnecting(false));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Failed);
    TEST_ASSERT_EQUAL_STRING("", rig.wifi().address().c_str());
  }

  static void an_open_network_needs_no_password() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.wifi().startStation("Cafe", ""));
  }

  static void a_station_without_a_name_is_refused_and_changes_nothing() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.wifi().startStation("", "secret"));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
  }

  static void the_access_point_password_has_8_to_63_characters() {
    const struct { size_t length; bool accepted; } cases[] = {{0, false}, {7, false}, {8, true}, {63, true}, {64, false}};
    for (const auto& c : cases) {
      Rig rig;
      const bool ok = rig.wifi().startAccessPoint("PhonePlus", std::string(c.length, 'p'));
      TEST_ASSERT_EQUAL(c.accepted, ok);
      TEST_ASSERT_TRUE((rig.wifi().state() == WifiState::AccessPoint) == c.accepted);
    }
  }

  static void an_access_point_without_a_name_is_refused() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.wifi().startAccessPoint("", "12345678"));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
  }

  static void a_running_access_point_has_an_address() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.wifi().startAccessPoint("PhonePlus", "12345678"));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::AccessPoint);
    TEST_ASSERT_TRUE(!rig.wifi().address().empty());
  }

  static void an_access_point_that_fails_to_start_leaves_the_state_off() {
    Rig rig;
    CONTRACT_NEEDS(rig.failAccessPoint(true));
    TEST_ASSERT_FALSE(rig.wifi().startAccessPoint("PhonePlus", "12345678"));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
  }

  static void starting_one_mode_stops_the_other() {
    Rig rig;
    rig.wifi().startAccessPoint("PhonePlus", "12345678");
    rig.wifi().startStation("Home", "secret");
    TEST_ASSERT_TRUE(rig.wifi().state() != WifiState::AccessPoint);
    rig.wifi().startAccessPoint("PhonePlus", "12345678");
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::AccessPoint);
  }

  // A refused start must not cost the user the connection that works.
  static void a_refused_start_leaves_a_running_mode_running() {
    Rig rig;
    rig.wifi().startAccessPoint("PhonePlus", "12345678");
    TEST_ASSERT_FALSE(rig.wifi().startAccessPoint("Other", "short"));
    TEST_ASSERT_FALSE(rig.wifi().startStation("", "x"));
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::AccessPoint);
    TEST_ASSERT_TRUE(!rig.wifi().address().empty());
  }

  static void stopping_turns_everything_off_and_may_be_repeated() {
    Rig rig;
    rig.wifi().startAccessPoint("PhonePlus", "12345678");
    rig.wifi().stop();
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
    TEST_ASSERT_EQUAL_STRING("", rig.wifi().address().c_str());
    rig.wifi().stop();
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
    rig.wifi().startStation("Home", "secret");
    rig.wifi().stop();
    TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::Off);
  }

  static void it_can_be_started_again_after_a_stop() {
    Rig rig;
    for (int i = 0; i < 3; ++i) {
      TEST_ASSERT_TRUE(rig.wifi().startAccessPoint("PhonePlus", "12345678"));
      TEST_ASSERT_TRUE(rig.wifi().state() == WifiState::AccessPoint);
      rig.wifi().stop();
    }
  }
};

#define RUN_WIFI_CONTRACT(Rig)                                                              \
  RUN_TEST(WifiContract<Rig>::it_starts_off_without_an_address);                             \
  RUN_TEST(WifiContract<Rig>::a_station_attempt_is_under_way_and_has_no_address_yet);        \
  RUN_TEST(WifiContract<Rig>::a_station_that_signs_in_has_an_address);                       \
  RUN_TEST(WifiContract<Rig>::a_station_that_cannot_sign_in_fails_without_an_address);       \
  RUN_TEST(WifiContract<Rig>::an_open_network_needs_no_password);                            \
  RUN_TEST(WifiContract<Rig>::a_station_without_a_name_is_refused_and_changes_nothing);      \
  RUN_TEST(WifiContract<Rig>::the_access_point_password_has_8_to_63_characters);             \
  RUN_TEST(WifiContract<Rig>::an_access_point_without_a_name_is_refused);                    \
  RUN_TEST(WifiContract<Rig>::a_running_access_point_has_an_address);                        \
  RUN_TEST(WifiContract<Rig>::an_access_point_that_fails_to_start_leaves_the_state_off);     \
  RUN_TEST(WifiContract<Rig>::starting_one_mode_stops_the_other);                            \
  RUN_TEST(WifiContract<Rig>::a_refused_start_leaves_a_running_mode_running);                \
  RUN_TEST(WifiContract<Rig>::stopping_turns_everything_off_and_may_be_repeated);            \
  RUN_TEST(WifiContract<Rig>::it_can_be_started_again_after_a_stop)
