// Host tests for connectivity: cellular, the radio request, Wi-Fi
// and the rules in front of it, and the network label.

#include <unity.h>

#include <CoreFakes.h>
#include <app/Connectivity.h>
#include <fakes/FakeWifi.h>

void setUp() {}
void tearDown() {}

namespace {

uint32_t fixedRandom() { return 12345678u; }

struct Rig : CoreFakes {
  PowerCoordination coordination;
  FakeWifi fakeWifi;
  WifiController controller{fakeWifi, fixedRandom};
  Connectivity net;

  explicit Rig(Profile p = ProfileBuilder().radio(true).build(), bool withWifi = true)
      : CoreFakes(std::move(p), 100), coordination(clock, power, modem, profile, notices, logger),
        net(clock, power, modem, profile, notices, coordination, logger) {
    WifiSettings s;
    s.addNetwork("HomeNet", "secret-pass");
    controller.configure(s);
    if (withWifi) net.setWifi(&controller);
  }

  void attachRadio() {
    radioUp();
    net.setRadio(&radio);
  }
  bool noticeIs(NoticeKind kind) { return notices.current().kind == kind; }
  size_t count(const std::string& part) const {
    size_t n = 0;
    for (const std::string& line : log.lines()) n += line.find(part) != std::string::npos ? 1 : 0;
    return n;
  }
};

}  // namespace

// ---- cellular -----------------------------------------------------------------------------------

void test_cellular_can_be_switched() {
  Rig r;
  TEST_ASSERT_TRUE(r.net.setCellularEnabled(false));
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CellularSwitchedOff));
  TEST_ASSERT_EQUAL(1, r.count("Cellular switched off"));
  TEST_ASSERT_TRUE(r.net.setCellularEnabled(true));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CellularSwitchedOn));
  TEST_ASSERT_EQUAL(1, r.count("Cellular switched on"));

  // A request for the state it is in does nothing: no second log line, no notice.
  r.notices.clear();
  TEST_ASSERT_TRUE(r.net.setCellularEnabled(true));
  TEST_ASSERT_TRUE(r.notices.current().none());
  TEST_ASSERT_EQUAL(1, r.count("Cellular switched on"));

  // A switch the hardware refuses is logged and reported.
  r.power.failDomain(PowerDomain::Cellular, true);
  TEST_ASSERT_FALSE(r.net.setCellularEnabled(false));
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  TEST_ASSERT_EQUAL(1, r.count("Cellular cannot be switched off"));
  TEST_ASSERT_TRUE(r.notices.current().none());                     // no notice for a failed switch
}

void test_a_failed_switch_on_is_logged() {
  Rig r;
  r.net.setCellularEnabled(false);
  r.power.failDomain(PowerDomain::Cellular, true);
  TEST_ASSERT_FALSE(r.net.setCellularEnabled(true));
  TEST_ASSERT_EQUAL(1, r.count("Cellular cannot be switched on"));
}

void test_a_switched_modem_is_not_asleep() {
  Rig r;
  r.coordination.apply(PowerCoordination::Needs(), false, 10000);   // 100 s quiet: asleep
  TEST_ASSERT_TRUE(r.coordination.modemAsleep());
  r.net.setCellularEnabled(false);
  TEST_ASSERT_FALSE(r.coordination.modemAsleep());
  r.coordination.apply(PowerCoordination::Needs(), false, 10000);   // switched off: left alone
  r.net.setCellularEnabled(true);                                   // a fresh start is awake, too
  TEST_ASSERT_FALSE(r.coordination.modemAsleep());
}

void test_registered_needs_power_state_and_operator() {
  Rig r;
  TEST_ASSERT_TRUE(r.net.registered());
  r.networkLost();                                                  // no operator: searching
  TEST_ASSERT_FALSE(r.net.registered());
  r.networkBack();
  TEST_ASSERT_TRUE(r.net.registered());
  r.modem.setSimStatus(SimStatus::PinRequired);                     // an unusable SIM is an error state
  TEST_ASSERT_FALSE(r.net.registered());
  r.modem.setSimStatus(SimStatus::Ready);
  r.net.setCellularEnabled(false);                                  // not powered
  TEST_ASSERT_FALSE(r.net.registered());
}

void test_the_network_label_is_data() {
  Rig r;
  using Kind = Connectivity::NetworkLabel::Kind;
  r.modem.setNetwork("F-Operator Telecom", 3);
  TEST_ASSERT_TRUE(Kind::Operator == r.net.networkLabel().kind);
  TEST_ASSERT_EQUAL_STRING("Operator", r.net.networkLabel().name.c_str());   // shortened for the status line

  r.networkLost();
  TEST_ASSERT_TRUE(Kind::None == r.net.networkLabel().kind);        // nothing at all
  r.attachRadio();
  TEST_ASSERT_TRUE(Kind::Radio == r.net.networkLabel().kind);
  r.networkBack();                                                  // an operator wins over everything
  r.net.setCellularEnabled(true);
  TEST_ASSERT_TRUE(Kind::Operator == r.net.networkLabel().kind);
}

// Owner, 2026-10-08: after switching on, the status line said "Network" before the operator's name. While the
// modem looks for its first network since it was switched on, the label is Connecting; a network lost after
// that is no network, so that the child sees nobody can reach the device.
void test_the_label_says_connecting_until_the_first_network_since_switching_on() {
  Rig r;
  using Kind = Connectivity::NetworkLabel::Kind;
  r.modem.shutdown();
  TEST_ASSERT_TRUE(Kind::None == r.net.networkLabel().kind);        // off is not connecting
  r.networkLost();
  r.modem.begin();                                                  // on, searching, never named a network
  TEST_ASSERT_TRUE(Kind::Connecting == r.net.networkLabel().kind);
  TEST_ASSERT_FALSE(r.net.registered());                            // connecting is not registered
  r.networkBack();
  TEST_ASSERT_TRUE(Kind::Operator == r.net.networkLabel().kind);
  r.networkLost();                                                  // lost after it had one: no network
  TEST_ASSERT_TRUE(Kind::None == r.net.networkLabel().kind);
  r.modem.shutdown();                                               // switched off and on again: connecting
  TEST_ASSERT_TRUE(Kind::None == r.net.networkLabel().kind);
  r.modem.begin();
  TEST_ASSERT_TRUE(Kind::Connecting == r.net.networkLabel().kind);
  r.modem.setSimStatus(SimStatus::Missing);                         // a modem in error is not on its way
  TEST_ASSERT_TRUE(Kind::None == r.net.networkLabel().kind);
  r.modem.setSimStatus(SimStatus::Ready);
  r.net.setCellularEnabled(false);                                  // switched off by the device
  TEST_ASSERT_TRUE(Kind::None == r.net.networkLabel().kind);
}

// ---- radio and restart --------------------------------------------------------------------------

// CON-20: save mode switches the modem off at once and keeps Wi-Fi off; cellular and Wi-Fi by hand are refused.
void test_save_mode_switches_the_modem_off_wifi_off_and_refuses_the_opposite() {
  Rig r;
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::Station));
  r.net.enterSaveMode();
  TEST_ASSERT_TRUE(r.net.saveMode());
  TEST_ASSERT_FALSE(r.net.cellularEnabled());             // off between the searches
  TEST_ASSERT_FALSE(r.net.wifiActive());
  TEST_ASSERT_FALSE(r.net.setCellularEnabled(true));      // by hand: refused, both ways
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.count("Cellular on refused") == 1);
  TEST_ASSERT_FALSE(r.net.setWifiMode(WifiMode::AccessPoint));
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::Off));     // off always works
  r.net.leaveSaveMode();
  TEST_ASSERT_FALSE(r.net.saveMode());
  TEST_ASSERT_TRUE(r.net.cellularEnabled());              // as before: it was on
  TEST_ASSERT_TRUE(r.net.setCellularEnabled(false));      // free again
}

// CON-21: cellular that was off stays off, and is refused to be switched on, then is off again after.
void test_save_mode_with_cellular_off_before_leaves_it_off() {
  Rig r;
  TEST_ASSERT_TRUE(r.net.setCellularEnabled(false));
  r.net.enterSaveMode();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_FALSE(r.net.setCellularEnabled(false));     // refused, too
  TEST_ASSERT_TRUE(r.count("Cellular off refused") == 1);
  r.net.leaveSaveMode();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
}

// CON-23: a search on demand: modem on, off again after 90 s without a network; the timer starts again then.
void test_a_search_on_demand_ends_after_the_window_without_a_network() {
  Rig r;
  r.modem.setNetwork("", 0);
  r.net.enterSaveMode();
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.searchNow());
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.net.saveSearching());
  TEST_ASSERT_FALSE(r.net.searchNow());                   // already on
  r.clock.advanceSeconds(Connectivity::kSaveSearchWindowSeconds - 1);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  r.clock.advanceSeconds(1);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_FALSE(r.net.saveSearching());
}

// CON-24: nothing searches by itself in save mode when the timer is 0, however long it takes.
void test_timer_zero_never_searches_by_itself() {
  Profile p = ProfileBuilder().radio(true).radioCellularRetry(0).build();
  Rig r(std::move(p));
  r.net.enterSaveMode();
  for (int i = 0; i < 24 * 60; ++i) {
    r.clock.advanceSeconds(60);
    r.net.poll();
  }
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.net.searchNow());                    // on demand still works
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
}

// CON-25: the timer searches at the set interval and the window ends without a network; then the interval starts again.
void test_the_timer_searches_at_the_set_interval() {
  Profile p = ProfileBuilder().radio(true).radioCellularRetry(15).build();
  Rig r(std::move(p));
  r.modem.setNetwork("", 0);
  r.net.enterSaveMode();
  r.clock.advanceSeconds(15 * 60 - 1);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());             // not yet
  r.clock.advanceSeconds(1);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());              // the timer switched it on ...
  r.clock.advanceSeconds(Connectivity::kSaveSearchWindowSeconds);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());             // ... and off again: no network
  r.clock.advanceSeconds(15 * 60 - 1);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());             // the interval counts from the switching off
  r.clock.advanceSeconds(1);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
}

// CON-26: a network found during a search keeps the modem on; a network lost starts a new window, then off again.
void test_a_network_found_keeps_the_modem_on_and_a_lost_one_starts_the_search_again() {
  Rig r;
  r.modem.setNetwork("", 0);
  r.net.enterSaveMode();
  TEST_ASSERT_TRUE(r.net.searchNow());
  r.clock.advanceSeconds(30);
  r.modem.setNetwork("Example Mobile", 4);
  r.net.poll();
  r.clock.advanceSeconds(3600);                           // long past the window: it keeps the network
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.net.registered());
  r.modem.setNetwork("", 0);                              // lost
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());              // a new window
  r.clock.advanceSeconds(Connectivity::kSaveSearchWindowSeconds);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
}

// CON-27: ending save mode leaves no timer: the modem is as before and nothing switches it afterwards.
void test_leaving_save_mode_stops_the_timer() {
  Rig r;
  r.net.enterSaveMode();
  r.net.leaveSaveMode();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.net.setCellularEnabled(false));
  r.clock.advanceSeconds(3 * 3600);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());             // the timer did not switch it on
  TEST_ASSERT_FALSE(r.net.searchNow());                   // and a search outside save mode is nothing
}

void test_the_radio_stop_is_requested_once_and_only_for_a_radio_that_runs() {
  Rig r;
  TEST_ASSERT_FALSE(r.net.requestRadioStop());            // nothing is attached
  r.attachRadio();
  TEST_ASSERT_TRUE(r.net.requestRadioStop());
  TEST_ASSERT_TRUE(r.net.takeRadioStopRequest());
  TEST_ASSERT_FALSE(r.net.takeRadioStopRequest());
}


void test_the_radio_starts_only_on_request_and_if_permitted() {
  Rig r;
  TEST_ASSERT_FALSE(r.net.takeRadioStartRequest());                 // nobody asked
  TEST_ASSERT_TRUE(r.net.requestRadioStart());
  TEST_ASSERT_TRUE(r.net.takeRadioStartRequest());
  TEST_ASSERT_FALSE(r.net.takeRadioStartRequest());                 // once per request

  Rig forbidden(ProfileBuilder().radio(false).build());
  TEST_ASSERT_FALSE(forbidden.net.requestRadioStart());
  TEST_ASSERT_FALSE(forbidden.net.takeRadioStartRequest());

  Rig running;
  running.attachRadio();
  TEST_ASSERT_TRUE(running.net.radioAttached());
  TEST_ASSERT_FALSE(running.net.requestRadioStart());               // it runs already
  TEST_ASSERT_FALSE(running.net.takeRadioStartRequest());
}

void test_a_restart_can_be_requested() {
  Rig r;
  TEST_ASSERT_FALSE(r.net.restartRequested());
  r.net.requestRestart();
  TEST_ASSERT_TRUE(r.net.restartRequested());
}

// ---- Wi-Fi --------------------------------------------------------------------------------------

void test_wifi_is_off_until_someone_asks() {
  Rig r;
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.wifiActive());
  TEST_ASSERT_TRUE(r.fakeWifi.state() == WifiState::Off);
}

void test_wifi_is_refused_on_a_nearly_empty_battery() {
  Rig r;
  r.power.setBattery(19);
  TEST_ASSERT_FALSE(r.net.setWifiMode(WifiMode::Station));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::WifiBatteryLow));
  TEST_ASSERT_EQUAL(1, r.count("Wi-Fi refused"));
  TEST_ASSERT_FALSE(r.net.wifiActive());
  r.power.setBattery(20);                                           // 20 percent is enough
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::Station));
  r.power.setBattery(5);
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::Off));               // switching it off always works
  TEST_ASSERT_FALSE(r.net.wifiActive());

  r.power.plugIn(ChargeState::Charging);                            // on the cable the battery does not matter
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::AccessPoint));
}

void test_an_unknown_battery_does_not_refuse_wifi() {
  Rig r;
  r.power.setBattery(-1);
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::Station));
}

void test_without_wifi_nothing_happens() {
  Rig r(ProfileBuilder().build(), false);
  TEST_ASSERT_FALSE(r.net.hasWifi());
  TEST_ASSERT_FALSE(r.net.setWifiMode(WifiMode::Station));
  TEST_ASSERT_FALSE(r.net.wifiActive());
  TEST_ASSERT_TRUE(r.notices.current().none());
  r.net.poll();                                                     // and polling is harmless
}

void test_wifi_requests_are_logged() {
  Rig r;
  r.net.setWifiMode(WifiMode::Station);
  TEST_ASSERT_EQUAL(1, r.count("Wi-Fi home network"));
  r.net.setWifiMode(WifiMode::AccessPoint);
  TEST_ASSERT_EQUAL(1, r.count("Wi-Fi access point"));
  r.net.setWifiMode(WifiMode::Off);
  TEST_ASSERT_EQUAL(1, r.count("Wi-Fi off"));
  r.fakeWifi.failAccessPoint(true);
  TEST_ASSERT_FALSE(r.net.setWifiMode(WifiMode::AccessPoint));
  TEST_ASSERT_EQUAL(1, r.count("Wi-Fi access point failed"));
}

void test_poll_keeps_wifi_going() {
  Rig r;
  r.net.setWifiMode(WifiMode::Station);
  r.fakeWifi.finishConnecting(true);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.wifi()->phase() == WifiPhase::Connected);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cellular_can_be_switched);
  RUN_TEST(test_a_failed_switch_on_is_logged);
  RUN_TEST(test_a_switched_modem_is_not_asleep);
  RUN_TEST(test_registered_needs_power_state_and_operator);
  RUN_TEST(test_the_network_label_is_data);
  RUN_TEST(test_the_label_says_connecting_until_the_first_network_since_switching_on);
  RUN_TEST(test_the_radio_starts_only_on_request_and_if_permitted);
  RUN_TEST(test_save_mode_switches_the_modem_off_wifi_off_and_refuses_the_opposite);
  RUN_TEST(test_save_mode_with_cellular_off_before_leaves_it_off);
  RUN_TEST(test_a_search_on_demand_ends_after_the_window_without_a_network);
  RUN_TEST(test_timer_zero_never_searches_by_itself);
  RUN_TEST(test_the_timer_searches_at_the_set_interval);
  RUN_TEST(test_a_network_found_keeps_the_modem_on_and_a_lost_one_starts_the_search_again);
  RUN_TEST(test_leaving_save_mode_stops_the_timer);
  RUN_TEST(test_the_radio_stop_is_requested_once_and_only_for_a_radio_that_runs);
  RUN_TEST(test_a_restart_can_be_requested);
  RUN_TEST(test_wifi_is_off_until_someone_asks);
  RUN_TEST(test_wifi_is_refused_on_a_nearly_empty_battery);
  RUN_TEST(test_an_unknown_battery_does_not_refuse_wifi);
  RUN_TEST(test_without_wifi_nothing_happens);
  RUN_TEST(test_wifi_requests_are_logged);
  RUN_TEST(test_poll_keeps_wifi_going);
  return UNITY_END();
}
