// Host tests for connectivity: cellular, the radio request, save mode and the cell probe, Wi-Fi
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
  r.net.setSaveMode(true);
  TEST_ASSERT_TRUE(Kind::RadioOnly == r.net.networkLabel().kind);
  r.networkBack();                                                  // an operator wins over everything
  r.net.setCellularEnabled(true);
  TEST_ASSERT_TRUE(Kind::Operator == r.net.networkLabel().kind);
}

// ---- radio and restart --------------------------------------------------------------------------

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

// ---- save mode ----------------------------------------------------------------------------------

void test_save_mode_needs_the_radio() {
  Rig r;
  TEST_ASSERT_FALSE(r.net.setSaveMode(true));
  TEST_ASSERT_FALSE(r.net.saveMode());
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::SaveModeNeedsRadio));
  TEST_ASSERT_TRUE(r.net.cellularEnabled());                        // nothing was switched off
  TEST_ASSERT_TRUE(r.net.setSaveMode(false));                       // leaving what is not on is fine
}

void test_save_mode_switches_cellular_and_wifi_off() {
  Rig r;
  r.attachRadio();
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::AccessPoint));
  TEST_ASSERT_TRUE(r.net.wifiActive());
  TEST_ASSERT_TRUE(r.net.setSaveMode(true));
  TEST_ASSERT_TRUE(r.net.saveMode());
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_FALSE(r.net.wifiActive());
  TEST_ASSERT_TRUE(r.fakeWifi.state() == WifiState::Off);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::SaveModeOn));
  TEST_ASSERT_EQUAL(1, r.count("Save mode on"));
  TEST_ASSERT_TRUE(r.net.setSaveMode(true));                        // asking again changes nothing
  TEST_ASSERT_EQUAL(1, r.count("Save mode on"));

  TEST_ASSERT_TRUE(r.net.setSaveMode(false));
  TEST_ASSERT_FALSE(r.net.saveMode());
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  TEST_ASSERT_EQUAL(1, r.count("Save mode off"));
}

void test_the_cell_probe_ends_by_itself() {
  Rig r;
  r.attachRadio();
  r.net.setSaveMode(true);
  TEST_ASSERT_TRUE(r.net.probeCellular());
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
  TEST_ASSERT_EQUAL_UINT32(Connectivity::kProbeSeconds, r.net.probeSecondsLeft());
  r.advance(Connectivity::kProbeSeconds - 1);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());                        // one second short
  TEST_ASSERT_EQUAL_UINT32(1, r.net.probeSecondsLeft());
  r.advance(1);
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.cellularEnabled());
  TEST_ASSERT_TRUE(r.net.saveMode());                               // save mode stays
  TEST_ASSERT_EQUAL_UINT32(0, r.net.probeSecondsLeft());
  TEST_ASSERT_EQUAL(1, r.count("cell probe ended"));
  r.advance(60);
  r.net.poll();                                                     // and it is not ended twice
  TEST_ASSERT_EQUAL(1, r.count("cell probe ended"));
}

void test_a_probe_can_be_started_again_and_leaving_save_mode_ends_it() {
  Rig r;
  r.attachRadio();
  r.net.setSaveMode(true);
  r.net.probeCellular();
  r.advance(100);
  r.net.probeCellular();                                            // starts the 180 seconds anew
  TEST_ASSERT_EQUAL_UINT32(Connectivity::kProbeSeconds, r.net.probeSecondsLeft());
  r.net.setSaveMode(false);
  TEST_ASSERT_EQUAL_UINT32(0, r.net.probeSecondsLeft());
  r.advance(500);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());                        // the old probe does not switch it off
}

void test_the_probe_works_only_in_save_mode() {
  Rig r;
  TEST_ASSERT_FALSE(r.net.probeCellular());
  TEST_ASSERT_EQUAL_UINT32(0, r.net.probeSecondsLeft());
  r.advance(500);
  r.net.poll();
  TEST_ASSERT_TRUE(r.net.cellularEnabled());
}

// ---- Wi-Fi --------------------------------------------------------------------------------------

void test_wifi_is_off_until_someone_asks() {
  Rig r;
  r.net.poll();
  TEST_ASSERT_FALSE(r.net.wifiActive());
  TEST_ASSERT_TRUE(r.fakeWifi.state() == WifiState::Off);
}

void test_wifi_is_refused_in_save_mode() {
  Rig r;
  r.attachRadio();
  r.net.setSaveMode(true);
  TEST_ASSERT_FALSE(r.net.setWifiMode(WifiMode::Station));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::SaveModeNoWifi));
  TEST_ASSERT_TRUE(r.fakeWifi.state() == WifiState::Off);
  TEST_ASSERT_TRUE(r.net.setWifiMode(WifiMode::Off));               // off always works
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
  RUN_TEST(test_the_radio_starts_only_on_request_and_if_permitted);
  RUN_TEST(test_a_restart_can_be_requested);
  RUN_TEST(test_save_mode_needs_the_radio);
  RUN_TEST(test_save_mode_switches_cellular_and_wifi_off);
  RUN_TEST(test_the_cell_probe_ends_by_itself);
  RUN_TEST(test_a_probe_can_be_started_again_and_leaving_save_mode_ends_it);
  RUN_TEST(test_the_probe_works_only_in_save_mode);
  RUN_TEST(test_wifi_is_off_until_someone_asks);
  RUN_TEST(test_wifi_is_refused_in_save_mode);
  RUN_TEST(test_wifi_is_refused_on_a_nearly_empty_battery);
  RUN_TEST(test_an_unknown_battery_does_not_refuse_wifi);
  RUN_TEST(test_without_wifi_nothing_happens);
  RUN_TEST(test_wifi_requests_are_logged);
  RUN_TEST(test_poll_keeps_wifi_going);
  return UNITY_END();
}
