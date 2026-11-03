// Host tests for the radio's emergency (D-59, D-62): it begins when the SOS is used without a mobile network,
// lasts until the SOS is ended on the device, keeps the modem on with it (D-64), lets anybody write during it,
// and keeps the SOS itself off the radio (D-61).

#include <unity.h>

#include <fstream>
#include <sstream>

#include <CoreFakes.h>
#include <app/AppCore.h>
#include <fakes/FakeWifi.h>

using namespace people;

void setUp() {}
void tearDown() {}

namespace {

uint32_t fixedRandom() { return 12345678u; }

struct Device {
  FakeClock clock;
  FakePower power{&clock};
  FakeModem modem{clock};
  FakeGnss gnss{power, clock};
  FakeRadioLink radio;
  FakeHaptic haptic{clock};
  FakeEventLog log{&clock};
  FakeWifi wifi;
  i18n::Translator translator;
  AppCore core;

  static Profile profile(bool radioAllowed) {
    return ProfileBuilder().childName("Alex").radio(radioAllowed).radioPeer("Node One").build();
  }

  explicit Device(bool radioAllowed = true) : core((init(), clock), power, modem, gnss, haptic, log, translator, profile(radioAllowed)) {
    core.setWifi(wifi, fixedRandom);
    core.start();
  }

  void init() {
    for (const char* file : {"en.toml", "de.toml", "fr.toml"}) {
      std::ifstream in(std::string("lang/") + file);
      std::stringstream ss;
      ss << in.rdbuf();
      const std::string text = ss.str();
      i18n::Catalog catalog;
      TEST_ASSERT_TRUE_MESSAGE(!text.empty() && catalog.parse(text.data(), text.size()), file);
      translator.add(catalog);
    }
    clock.setTzOffsetMinutes(coretest::kZoneMinutes);
    clock.advanceSeconds(100);
    clock.setEpochUtc(coretest::kTuesdayAfternoon);
    power.setEnabled(PowerDomain::Cellular, true);
    modem.begin();
    modem.setNetwork("Example Mobile", 4);
    gnss.fixAfter(2, 48.1, 11.5, 1.5);
  }

  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      clock.advanceSeconds(1);
      core.poll(false, 10000);
    }
  }
  void noNetwork() { modem.setNetwork("", 0); }
  void sos(SosPing::Kind kind = SosPing::Kind::Emergency) {
    TEST_ASSERT_TRUE(core.beginSos());
    TEST_ASSERT_TRUE(core.chooseSos(kind));
  }
  // The radio runs and is attached, as after the host has started it.
  void hostStartsRadio() {
    core.takeRadioStartRequest();
    radio.setUp(true);
    radio.addPeer("Node One", true, 0);
    core.setRadio(&radio);
  }
  bool logged(const std::string& part) const { return log.contains(part); }
};

}  // namespace

void test_an_sos_with_a_network_does_not_begin_the_radio_emergency() {
  Device d;
  d.sos();
  TEST_ASSERT_FALSE(d.core.state().emergencyRadio);
  TEST_ASSERT_FALSE(d.core.takeRadioStartRequest());
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);
}

void test_an_sos_without_a_network_begins_it_and_asks_for_the_radio() {
  Device d;
  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_TRUE(d.core.state().emergencyRadio);
  TEST_ASSERT_TRUE(d.core.takeRadioStartRequest());
  TEST_ASSERT_FALSE(d.core.takeRadioStartRequest());   // once per request
  TEST_ASSERT_TRUE(d.logged("Emergency radio on"));
}

void test_a_pickup_without_a_network_begins_it_too() {
  Device d;
  d.noNetwork();
  d.run(1);
  d.sos(SosPing::Kind::Pickup);
  TEST_ASSERT_TRUE(d.core.state().emergencyRadio);
}

void test_with_the_radio_not_allowed_nothing_of_it_happens() {
  Device d(false);
  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_FALSE(d.core.state().emergencyRadio);
  TEST_ASSERT_FALSE(d.core.takeRadioStartRequest());
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);
}

void test_the_reply_to_a_query_does_not_begin_it() {
  Device d;
  d.noNetwork();
  d.modem.injectSms(kParentOne, "#WhereAreYou", 1);   // a query that reached the device just as the network went
  d.run(3);
  TEST_ASSERT_TRUE(d.core.state().pingKind == SosPing::Kind::Query);
  TEST_ASSERT_FALSE(d.core.state().emergencyRadio);
  TEST_ASSERT_FALSE(d.core.takeRadioStartRequest());
}

void test_save_mode_keeps_the_modem_on_and_wifi_off_and_refuses_the_opposite() {
  Device d;
  TEST_ASSERT_TRUE(d.core.setWifiMode(WifiMode::AccessPoint));
  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);    // the modem keeps searching
  TEST_ASSERT_TRUE(d.core.state().wifiPhase == WifiPhase::Off);
  TEST_ASSERT_TRUE(d.logged("Save mode on"));
  TEST_ASSERT_FALSE(d.core.setCellularEnabled(false));
  TEST_ASSERT_FALSE(d.core.setWifiMode(WifiMode::Station));
  TEST_ASSERT_TRUE(d.logged("Cellular off refused"));
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);
}

void test_a_stranger_is_dropped_outside_the_emergency_and_heard_during_it() {
  Device d;
  d.hostStartsRadio();
  d.radio.addPeer("Stranger", false, 0);
  d.radio.deliver("Stranger", "Do you need help?", 1);
  d.run(1);
  TEST_ASSERT_EQUAL(0, d.core.state().unread);        // only [radio] peer may write

  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_TRUE(d.core.state().emergencyRadio);
  d.radio.deliver("Stranger", "Do you need help?", 2);
  d.radio.deliver("Node One", "Where are you?", 3);
  d.run(1);
  TEST_ASSERT_EQUAL(2, d.core.state().unread);
  const Message& first = d.core.history().at(d.core.history().count() - 2);
  TEST_ASSERT_TRUE(first.source == MsgSource::Radio);
  TEST_ASSERT_EQUAL_STRING("Stranger", first.peer);
  TEST_ASSERT_EQUAL_STRING("Do you need help?", first.text);
}

void test_a_stranger_can_be_answered_during_the_emergency() {
  Device d;
  d.hostStartsRadio();
  d.radio.addPeer("Stranger", false, 0);
  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_TRUE(d.core.sendRadio("Stranger", "Yes, please call my parents"));
  TEST_ASSERT_EQUAL(1, d.radio.sent().size());
  TEST_ASSERT_EQUAL_STRING("Stranger", d.radio.sent()[0].peer.c_str());
}

void test_the_sos_text_never_goes_on_the_radio_and_waits_while_there_is_no_network() {
  Device d;
  d.hostStartsRadio();
  d.noNetwork();
  d.run(1);
  d.sos();
  d.run(60);                                           // the position is found and the text is due
  TEST_ASSERT_EQUAL(0, d.radio.sent().size());         // D-61: nothing of the SOS on the radio
  TEST_ASSERT_EQUAL(1, d.core.state().queued);         // it waits, as a text with no path does

  TEST_ASSERT_TRUE(d.core.state().emergencyRadio);     // the emergency goes on meanwhile
}

void test_the_network_coming_back_during_the_emergency_lets_the_sos_text_out_by_sms() {
  Device d;
  d.hostStartsRadio();
  d.noNetwork();
  d.run(1);
  d.sos();
  d.run(60);
  TEST_ASSERT_EQUAL(1, d.core.state().queued);
  TEST_ASSERT_EQUAL(0, d.modem.sent().size());

  d.modem.setNetwork("Example Mobile", 4);              // a network is found while the SOS is still on
  d.run(3);
  TEST_ASSERT_TRUE(d.core.state().emergencyRadio);      // the radio keeps running (D-62)
  TEST_ASSERT_FALSE(d.core.takeRadioStopRequest());
  TEST_ASSERT_EQUAL(0, d.core.state().queued);
  TEST_ASSERT_EQUAL(1, d.modem.sent().size());          // the SOS text left by SMS ...
  TEST_ASSERT_EQUAL(0, d.radio.sent().size());          // ... and the radio sent nothing (D-61)
}

void test_ending_the_sos_stops_the_radio_ends_save_mode_and_closes_the_door_again() {
  Device d;
  d.hostStartsRadio();
  d.radio.addPeer("Stranger", false, 0);
  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_FALSE(d.core.takeRadioStopRequest());    // not before the SOS is ended
  TEST_ASSERT_TRUE(d.core.endSos());
  TEST_ASSERT_FALSE(d.core.state().emergencyRadio);
  TEST_ASSERT_TRUE(d.core.takeRadioStopRequest());
  TEST_ASSERT_FALSE(d.core.takeRadioStopRequest());    // once per request
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);
  TEST_ASSERT_TRUE(d.logged("Emergency radio off"));
  TEST_ASSERT_TRUE(d.core.setCellularEnabled(false));  // save mode no longer holds
  TEST_ASSERT_TRUE(d.core.setCellularEnabled(true));

  d.radio.deliver("Stranger", "Hello again", 5);
  d.run(1);
  TEST_ASSERT_EQUAL(0, d.core.state().unread);
  TEST_ASSERT_FALSE(d.core.endSos());                  // nothing to end
}

void test_cellular_that_the_child_switched_off_is_on_during_the_emergency_and_off_after() {
  Device d;
  TEST_ASSERT_TRUE(d.core.setCellularEnabled(false));
  d.run(1);
  d.sos();
  TEST_ASSERT_TRUE(d.core.state().emergencyRadio);
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);    // the emergency switches the modem on ...
  d.core.endSos();
  TEST_ASSERT_FALSE(d.core.state().cellularEnabled);   // ... and, it having been off before, off again after
}

void test_a_radio_that_does_not_start_ends_the_emergency() {
  Device d;
  d.noNetwork();
  d.run(1);
  d.sos();
  TEST_ASSERT_TRUE(d.core.takeRadioStartRequest());
  d.core.radioStartFailed();
  TEST_ASSERT_FALSE(d.core.state().emergencyRadio);
  TEST_ASSERT_TRUE(d.core.state().cellularEnabled);    // not left with neither cellular nor radio
  TEST_ASSERT_FALSE(d.core.takeRadioStopRequest());
}

void test_the_radio_chip_stays_powered_while_the_radio_runs_without_a_contact() {
  Device d;
  d.radio.setUp(true);                                 // up, nobody heard yet
  d.core.setRadio(&d.radio);
  const PowerDecision decision = d.core.poll(false, 10000);
  TEST_ASSERT_TRUE(decision.radioShouldBePowered);
  d.core.setRadio(nullptr);
  TEST_ASSERT_FALSE(d.core.poll(false, 10000).radioShouldBePowered);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_an_sos_with_a_network_does_not_begin_the_radio_emergency);
  RUN_TEST(test_an_sos_without_a_network_begins_it_and_asks_for_the_radio);
  RUN_TEST(test_a_pickup_without_a_network_begins_it_too);
  RUN_TEST(test_with_the_radio_not_allowed_nothing_of_it_happens);
  RUN_TEST(test_the_reply_to_a_query_does_not_begin_it);
  RUN_TEST(test_save_mode_keeps_the_modem_on_and_wifi_off_and_refuses_the_opposite);
  RUN_TEST(test_a_stranger_is_dropped_outside_the_emergency_and_heard_during_it);
  RUN_TEST(test_a_stranger_can_be_answered_during_the_emergency);
  RUN_TEST(test_the_sos_text_never_goes_on_the_radio_and_waits_while_there_is_no_network);
  RUN_TEST(test_the_network_coming_back_during_the_emergency_lets_the_sos_text_out_by_sms);
  RUN_TEST(test_ending_the_sos_stops_the_radio_ends_save_mode_and_closes_the_door_again);
  RUN_TEST(test_cellular_that_the_child_switched_off_is_on_during_the_emergency_and_off_after);
  RUN_TEST(test_a_radio_that_does_not_start_ends_the_emergency);
  RUN_TEST(test_the_radio_chip_stays_powered_while_the_radio_runs_without_a_contact);
  return UNITY_END();
}
