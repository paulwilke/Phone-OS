// Host tests for the scripting surface of the test doubles: each thing the tests (and
// later the simulator) can make a fake do. What every implementation must do is the
// business of the contract suites, not of this file.

#include <unity.h>

#include <fakes/FakeClock.h>
#include <fakes/FakeConfigStore.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakeMotion.h>
#include <fakes/FakePower.h>
#include <fakes/FakeRadioLink.h>
#include <fakes/FakeTcpServer.h>
#include <fakes/FakeWifi.h>

void setUp() {}
void tearDown() {}

namespace {

std::vector<CallEventType> drain(FakeModem& m) {
  std::vector<CallEventType> types;
  CallEvent e;
  while (m.pollCallEvent(e)) types.push_back(e.type);
  return types;
}

}  // namespace

// ---- FakeClock ----------------------------------------------------------------------

void test_clock_moves_only_when_told_and_the_epoch_follows_the_uptime() {
  FakeClock c;
  TEST_ASSERT_EQUAL_UINT64(0, c.uptimeMillis());
  TEST_ASSERT_EQUAL_UINT32(0, c.epochUtc());  // unknown until set
  c.advance(1500);
  TEST_ASSERT_EQUAL_UINT32(1, c.uptimeSeconds());
  TEST_ASSERT_TRUE(c.setEpochUtc(1790000000u));
  c.advanceSeconds(10);
  TEST_ASSERT_EQUAL_UINT32(1790000010u, c.epochUtc());
  TEST_ASSERT_TRUE(c.setEpochUtc(0));
  TEST_ASSERT_EQUAL_UINT32(0, c.epochUtc());
}

void test_clock_refuses_early_dates_and_impossible_offsets() {
  FakeClock c;
  TEST_ASSERT_FALSE(c.setEpochUtc(946684800u));  // 2000-01-01, an unsynchronised modem
  TEST_ASSERT_FALSE(c.setEpochUtc(1));
  TEST_ASSERT_EQUAL_UINT32(0, c.epochUtc());
  TEST_ASSERT_TRUE(c.setEpochUtc(Clock::kEarliestEpoch));
  TEST_ASSERT_FALSE(c.setTzOffsetMinutes(841));
  TEST_ASSERT_FALSE(c.setTzOffsetMinutes(-721));
  TEST_ASSERT_TRUE(c.setTzOffsetMinutes(-720));
  TEST_ASSERT_TRUE(c.setTzOffsetMinutes(840));
  TEST_ASSERT_EQUAL(840, c.tzOffsetMinutes());
}

// ---- FakePower ----------------------------------------------------------------------

void test_power_scripting() {
  FakePower p;
  TEST_ASSERT_FALSE(p.isEnabled(PowerDomain::Cellular));
  TEST_ASSERT_TRUE(p.setEnabled(PowerDomain::Cellular, true));
  p.failDomain(PowerDomain::Radio, true);
  TEST_ASSERT_FALSE(p.setEnabled(PowerDomain::Radio, true));
  TEST_ASSERT_FALSE(p.isEnabled(PowerDomain::Radio));

  p.setBattery(42);
  p.setMinutesRemaining(300);
  TEST_ASSERT_EQUAL(42, p.batteryPercent());
  TEST_ASSERT_EQUAL(300, p.batteryMinutesRemaining());
  p.plugIn(ChargeState::Charging);
  TEST_ASSERT_TRUE(p.externalPower());
  TEST_ASSERT_EQUAL(-1, p.batteryMinutesRemaining());  // unknown while charging
  p.plugIn(ChargeState::Full);
  TEST_ASSERT_TRUE(p.chargeState() == ChargeState::Full);
  p.unplug();
  TEST_ASSERT_FALSE(p.externalPower());
  p.setChargerAnswers(false);
  TEST_ASSERT_TRUE(p.chargeState() == ChargeState::Unknown);

  p.allowPowerOff(false);
  TEST_ASSERT_FALSE(p.powerOff());
  p.allowPowerOff(true);
  TEST_ASSERT_TRUE(p.powerOff());
  TEST_ASSERT_EQUAL(2, p.powerOffCalls());
}

void test_power_counts_switch_ons() {
  FakePower p;
  p.setEnabled(PowerDomain::Positioning, true);
  p.setEnabled(PowerDomain::Positioning, true);  // already on: not counted
  p.setEnabled(PowerDomain::Positioning, false);
  p.setEnabled(PowerDomain::Positioning, true);
  TEST_ASSERT_EQUAL(2, p.switchOnCount(PowerDomain::Positioning));
}

// ---- FakeGnss -----------------------------------------------------------------------

void test_gnss_finds_a_fix_after_its_delay_and_degrades_on_script() {
  FakeClock clock;
  FakePower power;
  FakeGnss gnss(power, clock);
  gnss.fixAfter(15, 48.13745, 11.57550, 1.5f);
  gnss.degradeAfter(60, 6.0f);
  GnssFix fix;
  TEST_ASSERT_FALSE(gnss.readFix(fix));  // switched off
  power.setEnabled(PowerDomain::Positioning, true);
  TEST_ASSERT_TRUE(gnss.readFix(fix));
  TEST_ASSERT_FALSE(fix.valid);  // runs, no position yet
  clock.advanceSeconds(14);
  gnss.readFix(fix);
  TEST_ASSERT_FALSE(fix.valid);
  clock.advanceSeconds(1);
  gnss.readFix(fix);
  TEST_ASSERT_TRUE(fix.valid);
  TEST_ASSERT_TRUE(fix.hdop == 1.5f);
  TEST_ASSERT_TRUE(fix.lat == 48.13745);
  clock.advanceSeconds(45);
  gnss.readFix(fix);
  TEST_ASSERT_TRUE(fix.hdop == 6.0f);
}

void test_gnss_never_finds_a_fix_when_told() {
  FakeClock clock;
  FakePower power;
  FakeGnss gnss(power, clock);
  gnss.never();
  power.setEnabled(PowerDomain::Positioning, true);
  clock.advanceSeconds(3600);
  GnssFix fix;
  TEST_ASSERT_TRUE(gnss.readFix(fix));
  TEST_ASSERT_FALSE(fix.valid);
}

// Switching off and on again starts the scenario over, even when nobody read in between.
void test_gnss_restarts_its_scenario_on_every_switch_on() {
  FakeClock clock;
  FakePower power;
  FakeGnss gnss(power, clock);
  gnss.fixAfter(10, 48.1, 11.5, 1.0f);
  power.setEnabled(PowerDomain::Positioning, true);
  GnssFix fix;
  gnss.readFix(fix);
  clock.advanceSeconds(20);
  gnss.readFix(fix);
  TEST_ASSERT_TRUE(fix.valid);
  power.setEnabled(PowerDomain::Positioning, false);
  power.setEnabled(PowerDomain::Positioning, true);  // unseen by the receiver
  gnss.readFix(fix);
  TEST_ASSERT_FALSE(fix.valid);
}

// ---- FakeModem ----------------------------------------------------------------------

void test_modem_settles_by_network_and_sim() {
  FakeClock clock;
  FakeModem m(clock);
  TEST_ASSERT_TRUE(m.state() == ModemState::Off);
  m.begin();
  TEST_ASSERT_TRUE(m.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL_STRING("Provider", m.operatorName().c_str());
  TEST_ASSERT_EQUAL(4, m.signalBars());
  m.setNetwork("", 0);
  TEST_ASSERT_TRUE(m.state() == ModemState::Registering);
  TEST_ASSERT_EQUAL(0, m.signalBars());
  m.setNetwork("Provider", 3, false);
  TEST_ASSERT_TRUE(m.state() == ModemState::Ready);
  TEST_ASSERT_FALSE(m.voiceAvailable());
  m.setSimStatus(SimStatus::PinRequired);
  TEST_ASSERT_TRUE(m.state() == ModemState::Error);
  m.setSimStatus(SimStatus::Ready);
  TEST_ASSERT_TRUE(m.state() == ModemState::Ready);
}

void test_modem_reports_a_scripted_send_failure_once() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  m.failNextSms("+CMS ERROR: 304");
  TEST_ASSERT_TRUE(m.sendSms("+491701111111", "one"));
  TEST_ASSERT_TRUE(m.sendSms("+491701111111", "two"));
  SmsFailure f;
  TEST_ASSERT_TRUE(m.pollSendFailure(f));
  TEST_ASSERT_EQUAL_STRING("+CMS ERROR: 304", f.reason.c_str());
  TEST_ASSERT_FALSE(m.pollSendFailure(f));
  TEST_ASSERT_EQUAL(2, m.sent().size());
}

void test_modem_incoming_call_events() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  TEST_ASSERT_TRUE(m.injectIncomingCall("+491701111111"));
  TEST_ASSERT_TRUE(m.state() == ModemState::RingingIn);
  TEST_ASSERT_TRUE(m.answer());
  clock.advanceSeconds(42);
  m.remoteHangup();
  CallEvent e;
  TEST_ASSERT_TRUE(m.pollCallEvent(e));
  TEST_ASSERT_TRUE(e.type == CallEventType::Incoming);
  TEST_ASSERT_TRUE(m.pollCallEvent(e));
  TEST_ASSERT_TRUE(e.type == CallEventType::Connected);
  TEST_ASSERT_TRUE(m.pollCallEvent(e));
  TEST_ASSERT_TRUE(e.type == CallEventType::Ended);
  TEST_ASSERT_EQUAL_UINT32(42, e.duration);
  TEST_ASSERT_EQUAL_STRING("+491701111111", e.phone.c_str());
  TEST_ASSERT_FALSE(m.pollCallEvent(e));
  TEST_ASSERT_TRUE(m.state() == ModemState::Ready);
}

void test_modem_refused_and_unanswered_incoming_calls_are_missed() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  m.injectIncomingCall("+491701111111");
  m.hangup();  // we refuse
  m.injectIncomingCall("+491701111111");
  m.remoteHangup();  // the caller gives up
  const auto types = drain(m);
  TEST_ASSERT_EQUAL(4, types.size());
  TEST_ASSERT_TRUE(types[1] == CallEventType::Missed);
  TEST_ASSERT_TRUE(types[3] == CallEventType::Missed);
}

void test_modem_outgoing_call_outcomes() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  m.dial("+491701111111");
  m.remoteBusy();
  m.dial("+491701111111");
  m.remoteNoAnswer();
  m.dial("+491701111111");
  m.failDial("+CME ERROR: 30");
  m.dial("+491701111111");
  m.hangup();  // we cancel
  m.dial("+491701111111");
  m.remoteAnswer();
  m.hangup();
  const auto types = drain(m);
  const CallEventType want[] = {CallEventType::Busy, CallEventType::NoAnswer, CallEventType::DialFailed,
                                CallEventType::Ended, CallEventType::Connected, CallEventType::Ended};
  TEST_ASSERT_EQUAL(6, types.size());
  for (size_t i = 0; i < 6; ++i) TEST_ASSERT_TRUE(types[i] == want[i]);
}

void test_modem_dial_failure_carries_the_reason() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  m.dial("+491701111111");
  m.failDial("+CME ERROR: 30");
  CallEvent e;
  TEST_ASSERT_TRUE(m.pollCallEvent(e));
  TEST_ASSERT_EQUAL_STRING("+CME ERROR: 30", e.reason.c_str());
}

void test_modem_shutdown_ends_a_call() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  m.dial("+491701111111");
  m.remoteAnswer();
  m.shutdown();
  const auto types = drain(m);
  TEST_ASSERT_EQUAL(2, types.size());
  TEST_ASSERT_TRUE(types[1] == CallEventType::Ended);
  TEST_ASSERT_TRUE(m.state() == ModemState::Off);
}

void test_modem_queries_and_volume() {
  FakeClock clock;
  FakeModem m(clock);
  m.begin();
  m.setQueryAnswer("AT+CSCLK?", "+CSCLK: 1|OK");
  TEST_ASSERT_TRUE(m.query("AT+CSCLK?"));
  std::string answer;
  TEST_ASSERT_TRUE(m.pollQueryResult(answer));
  TEST_ASSERT_EQUAL_STRING("+CSCLK: 1|OK", answer.c_str());
  TEST_ASSERT_FALSE(m.pollQueryResult(answer));
  TEST_ASSERT_TRUE(m.query("AT+CGMR?"));  // not scripted
  m.pollQueryResult(answer);
  TEST_ASSERT_EQUAL_STRING("OK", answer.c_str());

  TEST_ASSERT_TRUE(m.setCallVolume(99));
  TEST_ASSERT_EQUAL(7, m.callVolume());
  m.setVolumeSupported(false);
  TEST_ASSERT_FALSE(m.setCallVolume(3));
  TEST_ASSERT_EQUAL(-1, m.callVolume());
}

// ---- FakeEventLog, FakeHaptic, FakeMotion ---------------------------------------------

void test_event_log_records_lines_with_the_time_it_knew() {
  FakeClock clock;
  FakeEventLog log(&clock);
  log.log("before the time is known");
  clock.setEpochUtc(1790000000u);
  log.log("line one\nline two");
  TEST_ASSERT_EQUAL(2, log.entries().size());
  TEST_ASSERT_EQUAL_UINT32(0, log.entries()[0].epoch);
  TEST_ASSERT_EQUAL_UINT32(1790000000u, log.entries()[1].epoch);
  TEST_ASSERT_EQUAL_STRING("line one|line two", log.lines()[1].c_str());
  TEST_ASSERT_TRUE(log.contains("before"));
  TEST_ASSERT_FALSE(log.contains("nothing like this"));
}

void test_haptic_fires_the_real_rhythm_on_the_fake_clock() {
  FakeClock clock;
  FakeHaptic h(clock);
  clock.advance(1000);
  h.play(HapticPattern::Sent);
  h.service();
  clock.advance(150);
  h.service();
  clock.advance(150);
  h.service();
  TEST_ASSERT_EQUAL(1, h.played().size());
  TEST_ASSERT_EQUAL(3, h.fired().size());
  TEST_ASSERT_EQUAL_UINT64(1300, h.fired()[2].atMs);
  TEST_ASSERT_FALSE(h.busy());
}

void test_motion_scripting() {
  FakeMotion m;
  MotionSample s;
  TEST_ASSERT_FALSE(m.read(s));  // not running
  m.failNextStart();
  TEST_ASSERT_FALSE(m.start());
  TEST_ASSERT_TRUE(m.start());
  TEST_ASSERT_TRUE(m.read(s));
  TEST_ASSERT_TRUE(s.az == 1.0f);  // lying flat
  m.shake();
  TEST_ASSERT_TRUE(m.read(s));
  TEST_ASSERT_TRUE(s.ax > 2.0f);
  m.read(s);
  m.read(s);
  TEST_ASSERT_TRUE(s.ax == 0.0f);  // rested again
  m.clearSample();
  TEST_ASSERT_FALSE(m.read(s));
  m.setAvailable(false);
  m.stop();
  TEST_ASSERT_FALSE(m.start());
}

// ---- FakeConfigStore ----------------------------------------------------------------

void test_config_store_failed_write_keeps_the_old_file_and_hides_the_scratch_file() {
  FakeConfigStore store;
  TEST_ASSERT_TRUE(store.write("/config/settings.toml", "old"));
  store.failNextWrite();
  TEST_ASSERT_FALSE(store.write("/config/settings.toml", "new content"));
  std::string text;
  TEST_ASSERT_TRUE(store.read("/config/settings.toml", text, 100));
  TEST_ASSERT_EQUAL_STRING("old", text.c_str());
  TEST_ASSERT_EQUAL(1, store.scratchFileCount());  // half of it is there, but hidden
  std::vector<FileEntry> entries;
  TEST_ASSERT_TRUE(store.list("/config", entries));
  TEST_ASSERT_EQUAL(1, entries.size());
  TEST_ASSERT_TRUE(store.write("/config/settings.toml", "new content"));  // the next one works
  TEST_ASSERT_EQUAL(0, store.scratchFileCount());
}

void test_config_store_a_full_card_refuses_every_write() {
  FakeConfigStore store;
  store.put("/a.txt", "x");
  store.failAllWrites(true);
  TEST_ASSERT_FALSE(store.write("/a.txt", "y"));
  TEST_ASSERT_FALSE(store.write("/b.txt", "y"));
  TEST_ASSERT_EQUAL(1, store.fileSize("/a.txt"));
  store.failAllWrites(false);
  TEST_ASSERT_TRUE(store.write("/b.txt", "y"));
}

// ---- FakeWifi, FakeTcpServer, FakeRadioLink ------------------------------------------

void test_wifi_station_stays_connecting_until_told() {
  FakeWifi w;
  TEST_ASSERT_TRUE(w.startStation("Home", "secret"));
  TEST_ASSERT_TRUE(w.state() == WifiState::Connecting);
  TEST_ASSERT_EQUAL_STRING("", w.address().c_str());
  w.finishConnecting(true);
  TEST_ASSERT_TRUE(w.state() == WifiState::Connected);
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", w.address().c_str());
  TEST_ASSERT_TRUE(w.startStation("Home", "wrong"));
  w.finishConnecting(false);
  TEST_ASSERT_TRUE(w.state() == WifiState::Failed);
  w.failAccessPoint(true);
  TEST_ASSERT_FALSE(w.startAccessPoint("PhonePlus", "12345678"));
  TEST_ASSERT_TRUE(w.state() == WifiState::Off);
}

void test_tcp_server_scripting() {
  FakeTcpServer server;
  TEST_ASSERT_NULL(server.connect());  // refused: not listening
  TEST_ASSERT_TRUE(server.begin(8080));
  FakeTcpConnection* client = server.connect();
  TEST_ASSERT_NOT_NULL(client);
  client->fromPeer("GET / HTTP/1.1\r\n\r\n");
  TcpConnection* c = server.accept();
  TEST_ASSERT_TRUE(c == client);
  uint8_t buf[8];
  TEST_ASSERT_EQUAL(8, c->read(buf, sizeof(buf)));
  client->limitWritesTo(3);
  TEST_ASSERT_EQUAL(3, c->write((const uint8_t*)"hello", 5));
  TEST_ASSERT_EQUAL_STRING("hel", client->sentToPeer().c_str());
  client->peerClose();
  TEST_ASSERT_TRUE(c->read(buf, sizeof(buf)) > 0);  // the rest of what it sent
}

void test_radio_confirms_the_oldest_open_message_first() {
  FakeRadioLink r;
  r.setUp(true);
  r.addPeer("Node One", true, 1790000000u);
  r.addPeer("Node Two", false, 1790000000u);
  r.setDefaultPeer("Node One");
  TEST_ASSERT_TRUE(r.ready());
  TEST_ASSERT_TRUE(r.send("", "first"));
  TEST_ASSERT_TRUE(r.send("Node Two", "second"));
  TEST_ASSERT_TRUE(r.confirm(true));
  TEST_ASSERT_TRUE(r.confirm(false));
  TEST_ASSERT_FALSE(r.confirm(true));  // nothing open
  RadioDelivery d;
  TEST_ASSERT_TRUE(r.pollDelivery(d));
  TEST_ASSERT_TRUE(d.confirmed);
  TEST_ASSERT_EQUAL_STRING("Node One", d.peer.c_str());
  TEST_ASSERT_TRUE(r.pollDelivery(d));
  TEST_ASSERT_FALSE(d.confirmed);
  TEST_ASSERT_EQUAL_STRING("Node Two", d.peer.c_str());
  TEST_ASSERT_FALSE(r.pollDelivery(d));
  TEST_ASSERT_EQUAL(2, r.sent().size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_clock_moves_only_when_told_and_the_epoch_follows_the_uptime);
  RUN_TEST(test_clock_refuses_early_dates_and_impossible_offsets);
  RUN_TEST(test_power_scripting);
  RUN_TEST(test_power_counts_switch_ons);
  RUN_TEST(test_gnss_finds_a_fix_after_its_delay_and_degrades_on_script);
  RUN_TEST(test_gnss_never_finds_a_fix_when_told);
  RUN_TEST(test_gnss_restarts_its_scenario_on_every_switch_on);
  RUN_TEST(test_modem_settles_by_network_and_sim);
  RUN_TEST(test_modem_reports_a_scripted_send_failure_once);
  RUN_TEST(test_modem_incoming_call_events);
  RUN_TEST(test_modem_refused_and_unanswered_incoming_calls_are_missed);
  RUN_TEST(test_modem_outgoing_call_outcomes);
  RUN_TEST(test_modem_dial_failure_carries_the_reason);
  RUN_TEST(test_modem_shutdown_ends_a_call);
  RUN_TEST(test_modem_queries_and_volume);
  RUN_TEST(test_event_log_records_lines_with_the_time_it_knew);
  RUN_TEST(test_haptic_fires_the_real_rhythm_on_the_fake_clock);
  RUN_TEST(test_motion_scripting);
  RUN_TEST(test_config_store_failed_write_keeps_the_old_file_and_hides_the_scratch_file);
  RUN_TEST(test_config_store_a_full_card_refuses_every_write);
  RUN_TEST(test_wifi_station_stays_connecting_until_told);
  RUN_TEST(test_tcp_server_scripting);
  RUN_TEST(test_radio_confirms_the_oldest_open_message_first);
  return UNITY_END();
}
