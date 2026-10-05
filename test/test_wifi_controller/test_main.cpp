// Host tests for the Wi-Fi settings and the Wi-Fi controller.

#include <unity.h>

#include <fakes/FakeWifi.h>

#include <app/WifiController.h>
#include <app/WifiSettings.h>

void setUp() {}
void tearDown() {}

namespace {

uint32_t fixedRandom() { return 12345678u; }
uint32_t smallRandom() { return 42u; }

WifiSettings twoNetworks() {
  WifiSettings s;
  s.addNetwork("HomeNet", "secret-pass");
  s.addNetwork("SecondNet", "");
  s.apSsid = "my-phone";
  s.autoOffMinutes = 5;
  return s;
}

struct Rig {
  FakeWifi wifi;
  WifiController ctl{wifi, fixedRandom};
  explicit Rig(const WifiSettings& s = twoNetworks()) { ctl.configure(s); }
};

}  // namespace

// ---- settings -----------------------------------------------------------------------------------

void test_the_settings_are_validated() {
  WifiSettings s;
  TEST_ASSERT_TRUE(WifiSettings::AddResult::Added == s.addNetwork("a", ""));            // open network
  TEST_ASSERT_TRUE(WifiSettings::AddResult::Added == s.addNetwork("b", "12345678"));    // shortest password
  TEST_ASSERT_TRUE(WifiSettings::AddResult::BadPassword == s.addNetwork("c", "1234567"));
  TEST_ASSERT_TRUE(WifiSettings::AddResult::Added == s.addNetwork("d", std::string(63, 'x')));
  TEST_ASSERT_TRUE(WifiSettings::AddResult::Added == s.addNetwork("d2", ""));
  TEST_ASSERT_TRUE(WifiSettings::AddResult::TooMany == s.addNetwork("e", ""));          // the fifth is one too many
  TEST_ASSERT_EQUAL(4, s.networks.size());
  s.networks.pop_back();
  TEST_ASSERT_TRUE(WifiSettings::AddResult::BadPassword == s.addNetwork("f", std::string(64, 'x')));
  TEST_ASSERT_TRUE(WifiSettings::AddResult::BadName == s.addNetwork("", ""));
  TEST_ASSERT_TRUE(WifiSettings::AddResult::Added == s.addNetwork(std::string(32, 'n'), ""));
  s.networks.pop_back();
  TEST_ASSERT_TRUE(WifiSettings::AddResult::BadName == s.addNetwork(std::string(33, 'n'), ""));

  TEST_ASSERT_FALSE(s.setApPassword("1234"));                      // too short: the random one stays in force
  TEST_ASSERT_EQUAL_STRING("", s.apPassword.c_str());
  TEST_ASSERT_TRUE(s.setApPassword("abcdefgh"));
  TEST_ASSERT_TRUE(s.setApPassword(""));                           // clearing it is allowed
  TEST_ASSERT_EQUAL_STRING("", s.apPassword.c_str());
  TEST_ASSERT_FALSE(s.setApSsid(""));
  TEST_ASSERT_EQUAL_STRING("phoneplus", s.apSsid.c_str());
  TEST_ASSERT_TRUE(s.setApSsid("mine"));

  TEST_ASSERT_TRUE(s.setAutoOffMinutes(1));
  TEST_ASSERT_FALSE(s.setAutoOffMinutes(0));
  TEST_ASSERT_EQUAL(1, s.autoOffMinutes);
  TEST_ASSERT_TRUE(s.setAutoOffMinutes(120));
  TEST_ASSERT_FALSE(s.setAutoOffMinutes(9999));
  TEST_ASSERT_EQUAL(120, s.autoOffMinutes);
}

// ---- the controller -----------------------------------------------------------------------------

void test_wifi_is_off_until_someone_asks() {
  Rig r;
  r.ctl.poll(1000);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Off);
  TEST_ASSERT_TRUE(r.wifi.state() == WifiState::Off);
  TEST_ASSERT_FALSE(r.ctl.active());
  TEST_ASSERT_EQUAL(0, r.ctl.secondsLeft(1000));
}

void test_station_connects() {
  Rig r;
  TEST_ASSERT_TRUE(r.ctl.request(WifiMode::Station, 100));
  TEST_ASSERT_EQUAL_STRING("HomeNet", r.wifi.ssid().c_str());
  TEST_ASSERT_EQUAL_STRING("secret-pass", r.wifi.password().c_str());
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Connecting);
  r.wifi.finishConnecting(true);
  r.ctl.poll(105);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Connected);
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", r.ctl.address().c_str());
}

void test_station_tries_the_next_network() {
  Rig r;
  r.ctl.request(WifiMode::Station, 100);
  r.wifi.finishConnecting(false);                                   // wrong password
  r.ctl.poll(103);
  TEST_ASSERT_EQUAL_STRING("SecondNet", r.wifi.ssid().c_str());
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Connecting);
  // Nothing happens for the whole time of an attempt: this one is over as well.
  r.ctl.poll(103 + WifiController::kAttemptSeconds - 1);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Connecting);         // one second short
  r.ctl.poll(103 + WifiController::kAttemptSeconds);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Failed);
  TEST_ASSERT_TRUE(r.ctl.reason() == WifiReason::HomeNetworkUnreachable);
  TEST_ASSERT_TRUE(r.wifi.state() == WifiState::Off);               // the chip is off again
  TEST_ASSERT_FALSE(r.ctl.active());
}

void test_station_without_networks_says_so() {
  Rig r{WifiSettings()};
  TEST_ASSERT_FALSE(r.ctl.request(WifiMode::Station, 100));
  TEST_ASSERT_TRUE(r.ctl.reason() == WifiReason::NoHomeNetwork);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Failed);
  TEST_ASSERT_TRUE(r.wifi.state() == WifiState::Off);
}

void test_access_point_gets_a_random_password() {
  Rig r;
  TEST_ASSERT_TRUE(r.ctl.request(WifiMode::AccessPoint, 100));
  TEST_ASSERT_EQUAL_STRING("my-phone", r.wifi.ssid().c_str());
  TEST_ASSERT_EQUAL_STRING("12345678", r.wifi.password().c_str());
  TEST_ASSERT_EQUAL_STRING("12345678", r.ctl.apPassword().c_str());
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::AccessPoint);
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", r.ctl.address().c_str());
}

void test_the_password_is_padded() {
  FakeWifi wifi;
  WifiController ctl(wifi, smallRandom);
  ctl.configure(twoNetworks());
  ctl.request(WifiMode::AccessPoint, 0);
  TEST_ASSERT_EQUAL_STRING("00000042", ctl.apPassword().c_str());
  TEST_ASSERT_EQUAL_STRING("00000042", wifi.password().c_str());    // the chip accepted it: 8 characters
}

void test_a_configured_password_is_used() {
  WifiSettings s = twoNetworks();
  s.setApPassword("abcdefgh");
  Rig r(s);
  r.ctl.request(WifiMode::AccessPoint, 0);
  TEST_ASSERT_EQUAL_STRING("abcdefgh", r.wifi.password().c_str());
  TEST_ASSERT_EQUAL_STRING("abcdefgh", r.ctl.apPassword().c_str());
}

void test_the_password_is_forgotten_when_off() {
  Rig r;
  r.ctl.request(WifiMode::AccessPoint, 0);
  r.ctl.stop();
  TEST_ASSERT_EQUAL_STRING("", r.ctl.apPassword().c_str());
}

void test_it_switches_itself_off_after_the_idle_time() {
  Rig r;
  r.ctl.request(WifiMode::AccessPoint, 1000);
  r.ctl.poll(1000 + 5 * 60 - 1);
  TEST_ASSERT_TRUE(r.ctl.active());
  r.ctl.poll(1000 + 5 * 60);
  TEST_ASSERT_FALSE(r.ctl.active());
  TEST_ASSERT_TRUE(r.wifi.state() == WifiState::Off);
  TEST_ASSERT_TRUE(r.ctl.reason() == WifiReason::SwitchedOffByItself);
  TEST_ASSERT_EQUAL_STRING("", r.ctl.apPassword().c_str());
}

void test_activity_postpones_the_shutdown() {
  Rig r;
  r.ctl.request(WifiMode::AccessPoint, 0);
  r.ctl.noteActivity(200);
  r.ctl.poll(301);                                                  // without the activity it would be over at 300
  TEST_ASSERT_TRUE(r.ctl.active());
  TEST_ASSERT_EQUAL_UINT32(200 + 300 - 301, r.ctl.secondsLeft(301));
  TEST_ASSERT_EQUAL_UINT32(0, r.ctl.secondsLeft(600));   // past the limit, before the next poll
}

void test_a_lost_connection_is_retried() {
  Rig r;
  r.ctl.request(WifiMode::Station, 0);
  r.wifi.finishConnecting(true);
  r.ctl.poll(2);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Connected);
  r.wifi.stop();                                                    // the router goes off
  r.ctl.poll(10);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Connecting);
  TEST_ASSERT_EQUAL_STRING("HomeNet", r.wifi.ssid().c_str());       // from the first network again
}

void test_a_lost_connection_starts_again_from_the_first_network() {
  Rig r;
  r.ctl.request(WifiMode::Station, 0);
  r.wifi.finishConnecting(false);
  r.ctl.poll(1);                                                    // the second network is tried ...
  r.wifi.finishConnecting(true);
  r.ctl.poll(2);                                                    // ... and connects
  TEST_ASSERT_EQUAL_STRING("SecondNet", r.wifi.ssid().c_str());
  r.wifi.stop();
  r.ctl.poll(3);
  TEST_ASSERT_EQUAL_STRING("HomeNet", r.wifi.ssid().c_str());
}

void test_an_access_point_failure_is_reported() {
  Rig r;
  r.wifi.failAccessPoint(true);
  TEST_ASSERT_FALSE(r.ctl.request(WifiMode::AccessPoint, 0));
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Failed);
  TEST_ASSERT_TRUE(r.ctl.reason() == WifiReason::AccessPointFailed);
  r.wifi.failAccessPoint(false);
  TEST_ASSERT_TRUE(r.ctl.request(WifiMode::AccessPoint, 1));        // a new request starts clean
  TEST_ASSERT_TRUE(r.ctl.reason() == WifiReason::None);
}

void test_requesting_off_stops() {
  Rig r;
  r.ctl.request(WifiMode::AccessPoint, 0);
  TEST_ASSERT_TRUE(r.ctl.request(WifiMode::Off, 1));
  TEST_ASSERT_FALSE(r.ctl.active());
  TEST_ASSERT_TRUE(r.wifi.state() == WifiState::Off);
  TEST_ASSERT_TRUE(r.ctl.mode() == WifiMode::Off);
}

// The access point is lost while it runs (the chip stops): reported, not silently kept.
void test_a_lost_access_point_is_reported() {
  Rig r;
  r.ctl.request(WifiMode::AccessPoint, 0);
  r.wifi.stop();
  r.ctl.poll(5);
  TEST_ASSERT_TRUE(r.ctl.phase() == WifiPhase::Failed);
  TEST_ASSERT_TRUE(r.ctl.reason() == WifiReason::AccessPointLost);
}

// Somebody who has just connected wants to use it: the idle time starts at the connection.
void test_the_idle_time_starts_when_the_station_has_connected() {
  Rig r;
  r.ctl.request(WifiMode::Station, 0);
  r.wifi.finishConnecting(true);
  r.ctl.poll(15);
  r.ctl.poll(15 + 5 * 60 - 1);
  TEST_ASSERT_TRUE(r.ctl.active());
  r.ctl.poll(15 + 5 * 60);
  TEST_ASSERT_FALSE(r.ctl.active());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_settings_are_validated);
  RUN_TEST(test_wifi_is_off_until_someone_asks);
  RUN_TEST(test_station_connects);
  RUN_TEST(test_station_tries_the_next_network);
  RUN_TEST(test_station_without_networks_says_so);
  RUN_TEST(test_access_point_gets_a_random_password);
  RUN_TEST(test_the_password_is_padded);
  RUN_TEST(test_a_configured_password_is_used);
  RUN_TEST(test_the_password_is_forgotten_when_off);
  RUN_TEST(test_it_switches_itself_off_after_the_idle_time);
  RUN_TEST(test_activity_postpones_the_shutdown);
  RUN_TEST(test_a_lost_connection_is_retried);
  RUN_TEST(test_a_lost_connection_starts_again_from_the_first_network);
  RUN_TEST(test_an_access_point_failure_is_reported);
  RUN_TEST(test_requesting_off_stops);
  RUN_TEST(test_a_lost_access_point_is_reported);
  RUN_TEST(test_the_idle_time_starts_when_the_station_has_connected);
  return UNITY_END();
}
