// The Wi-Fi and TCP classes beyond what the contracts say: what is asked of the radio, what is refused before
// it is asked, and what a stop leaves behind.

#include <unity.h>

#include <BoardNetworkRigs.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_a_refused_start_does_not_touch_the_radio_and_leaves_what_runs_running() {
  BoardWifiRig r;
  r.impl.startAccessPoint("PhonePlus", "12345678");
  const int shutdowns = r.radio.shutdowns();
  TEST_ASSERT_FALSE(r.impl.startAccessPoint("Other", "short"));
  TEST_ASSERT_FALSE(r.impl.startStation("", "x"));
  TEST_ASSERT_EQUAL(shutdowns, r.radio.shutdowns());
  TEST_ASSERT_EQUAL(1, r.radio.beginAccessPointCalls());
  TEST_ASSERT_TRUE(r.radio.running());
}

void test_names_of_more_than_32_characters_and_passwords_of_more_than_63_are_refused() {
  BoardWifiRig r;
  TEST_ASSERT_TRUE(r.impl.startStation(std::string(32, 'n'), ""));
  TEST_ASSERT_FALSE(r.impl.startStation(std::string(33, 'n'), ""));
  TEST_ASSERT_FALSE(r.impl.startStation("Home", std::string(64, 'p')));
  TEST_ASSERT_FALSE(r.impl.startAccessPoint(std::string(33, 'n'), "12345678"));
  TEST_ASSERT_TRUE(r.impl.state() == WifiState::Connecting);  // the station that was started still runs
}

void test_an_open_network_is_asked_with_an_empty_password() {
  BoardWifiRig r;
  r.impl.startStation("Cafe", "");
  TEST_ASSERT_EQUAL_STRING("", r.radio.password().c_str());
  TEST_ASSERT_EQUAL_STRING("Cafe", r.radio.ssid().c_str());
}

void test_starting_one_mode_shuts_the_chip_down_before_the_other_comes_up() {
  BoardWifiRig r;
  r.impl.startAccessPoint("PhonePlus", "12345678");
  const int before = r.radio.shutdowns();
  r.impl.startStation("Home", "secret");
  TEST_ASSERT_EQUAL(before + 1, r.radio.shutdowns());
  TEST_ASSERT_TRUE(r.impl.state() == WifiState::Connecting);
}

void test_an_access_point_that_does_not_start_powers_the_chip_down() {
  BoardWifiRig r;
  r.radio.failAccessPoint(true);
  TEST_ASSERT_FALSE(r.impl.startAccessPoint("PhonePlus", "12345678"));
  TEST_ASSERT_FALSE(r.radio.running());
  TEST_ASSERT_TRUE(r.impl.state() == WifiState::Off);
}

void test_stop_powers_the_chip_down_once_and_stopping_when_off_does_nothing() {
  BoardWifiRig r;
  r.impl.stop();
  TEST_ASSERT_EQUAL(0, r.radio.shutdowns());
  r.impl.startStation("Home", "secret");
  r.impl.stop();
  r.impl.stop();
  TEST_ASSERT_EQUAL(1, r.radio.shutdowns());
}

void test_the_address_of_a_station_is_the_one_the_network_gave_and_only_once_signed_in() {
  BoardWifiRig r;
  r.radio.setStationAddress("10.0.0.7");
  r.impl.startStation("Home", "secret");
  TEST_ASSERT_EQUAL_STRING("", r.impl.address().c_str());
  r.radio.finishConnecting(true);
  TEST_ASSERT_EQUAL_STRING("10.0.0.7", r.impl.address().c_str());
}

void test_an_accepted_client_replaces_the_one_before_and_closes_it() {
  BoardTcpServerRig r;
  r.impl.begin(80);
  FakeTcpClient* first = r.listener.connect();
  FakeTcpClient* second = r.listener.connect();
  TcpConnection* a = r.impl.accept();
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_FALSE(first->stopped());
  TcpConnection* b = r.impl.accept();
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_TRUE(first->stopped());
  TEST_ASSERT_FALSE(second->stopped());
}

void test_a_read_never_asks_for_more_than_is_waiting_and_a_write_of_nothing_is_nothing() {
  BoardTcpServerRig r;
  r.impl.begin(80);
  FakeTcpClient* c = r.listener.connect();
  c->send("abc");
  TcpConnection* conn = r.impl.accept();
  uint8_t buf[64];
  TEST_ASSERT_EQUAL(3, conn->read(buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(0, conn->read(buf, 0));
  TEST_ASSERT_EQUAL(0, conn->write(buf, 0));
}

void test_stopping_the_server_closes_the_connection_and_stops_listening() {
  BoardTcpServerRig r;
  r.impl.begin(80);
  FakeTcpClient* c = r.listener.connect();
  TcpConnection* conn = r.impl.accept();
  r.impl.stop();
  TEST_ASSERT_TRUE(c->stopped());
  TEST_ASSERT_FALSE(r.listener.listening());
  uint8_t buf[4];
  TEST_ASSERT_EQUAL(-1, conn->read(buf, sizeof(buf)));
}

void test_beginning_again_restarts_the_listener_on_the_new_port() {
  BoardTcpServerRig r;
  TEST_ASSERT_TRUE(r.impl.begin(80));
  TEST_ASSERT_TRUE(r.impl.begin(8080));
  TEST_ASSERT_EQUAL(8080, r.listener.port());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_refused_start_does_not_touch_the_radio_and_leaves_what_runs_running);
  RUN_TEST(test_names_of_more_than_32_characters_and_passwords_of_more_than_63_are_refused);
  RUN_TEST(test_an_open_network_is_asked_with_an_empty_password);
  RUN_TEST(test_starting_one_mode_shuts_the_chip_down_before_the_other_comes_up);
  RUN_TEST(test_an_access_point_that_does_not_start_powers_the_chip_down);
  RUN_TEST(test_stop_powers_the_chip_down_once_and_stopping_when_off_does_nothing);
  RUN_TEST(test_the_address_of_a_station_is_the_one_the_network_gave_and_only_once_signed_in);
  RUN_TEST(test_an_accepted_client_replaces_the_one_before_and_closes_it);
  RUN_TEST(test_a_read_never_asks_for_more_than_is_waiting_and_a_write_of_nothing_is_nothing);
  RUN_TEST(test_stopping_the_server_closes_the_connection_and_stops_listening);
  RUN_TEST(test_beginning_again_restarts_the_listener_on_the_new_port);
  return UNITY_END();
}
