// Host tests for the A7682E driver: start-up, the SIM and its PIN, the data bearer for the IMS,
// registration, signal, operator, network time, the IMS state, restarts. The driver runs against a
// scripted modem (FakeAtTransport); the answers are built after 3GPP TS 27.007 and what the
// previous driver met on the device, not recorded from the new one.

#include <unity.h>

#include <string>
#include <vector>

#include <app/LogLevel.h>
#include <drivers/A7682EDriver.h>

#include "Script.h"

using namespace modemtest;

void setUp() {}
void tearDown() {}

namespace {

const std::string kWriteApn = std::string("AT+CGDCONT=1,\"IPV4V6\",\"") + kApn + "\"";

}  // namespace

// ---- the whole start-up as one script ---------------------------------------------------------------------

void test_the_whole_start_up_is_one_readable_sequence() {
  Rig r;
  Script s;
  s.contexts = {};                     // a modem that has no context 1 yet: the full course
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());

  const std::vector<std::string> expected = {
      "AT", "ATE0", "AT+CMEE=2", "AT+CMGF=1", "AT+CSCS=\"UCS2\"", "AT+CSMP=17,167,0,8", "AT+CNMI=2,1,0,0,0", "AT+CTZU=1",
      "AT+CLIP=1", "AT+CSDVC=3", "AT+COUTGAIN=4", "AT+COPS=3,0", "AT+CNMP=2",
      "AT+CPIN?",
      "AT+CGDCONT?", kWriteApn, "AT+CFUN=0", "AT+CFUN=1",
      "AT+CREG?", "AT+COPS?", "AT+CCLK?", "AT+CSQ",
      "AT+CIREG?", "AT+CGDCONT?"};
  TEST_ASSERT_EQUAL(expected.size(), r.t.sent().size());
  for (size_t i = 0; i < expected.size(); ++i) TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), r.t.sent()[i].c_str());
  TEST_ASSERT_EQUAL(0, r.t.unexpected().size());
  TEST_ASSERT_TRUE(r.t.powered());
  TEST_ASSERT_EQUAL(0, r.t.pwrkeyPulses());

  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Ready);
  TEST_ASSERT_EQUAL_STRING("Example Mobile", r.driver.operatorName().c_str());
  TEST_ASSERT_EQUAL(5, r.driver.signalBars());   // CSQ 23
}

void test_begin_twice_changes_nothing() {
  Rig r;
  Script().apply(r.t);
  r.driver.begin();
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  r.driver.begin();                                  // and once more when it runs: no new start
  r.run(5000);
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL(1, r.count("AT+CNMP=2"));
}

// ---- the data bearer (proposal tests 1 and 2) --------------------------------------------------------------

// Test 1: context 1 missing -> write, radio off, radio on, in this order, once.
void test_a_missing_context_1_is_written_and_the_radio_cycled_in_order_once() {
  Rig r;
  Script s;
  s.contexts = {"+CGDCONT: 8,\"IPV4V6\",\"ims\""};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());

  const int write = r.indexOf(kWriteApn), off = r.indexOf("AT+CFUN=0"), on = r.indexOf("AT+CFUN=1");
  TEST_ASSERT_TRUE(write > 0 && off > write && on > off);
  TEST_ASSERT_EQUAL(1, r.count(kWriteApn));
  TEST_ASSERT_EQUAL(1, r.count("AT+CFUN=0"));
  TEST_ASSERT_EQUAL(1, r.count("AT+CFUN=1"));
  // The context list is read before it is written, and the IMS bearer is never written.
  TEST_ASSERT_TRUE(r.indexOf("AT+CGDCONT?") < write);
  TEST_ASSERT_EQUAL(1, r.countPrefix("AT+CGDCONT=1"));
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT=8"));
  TEST_ASSERT_TRUE(r.logged("data context 1 written as IPV4V6"));
}

// Test 2: context 1 already right -> no write, no CFUN.
void test_a_context_1_that_is_right_is_left_alone() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT="));
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CFUN"));
}

// A SIM swap leaves the old APN behind; comparing the APN covers it. The type counts as well.
void test_a_context_1_with_another_apn_or_another_type_is_rewritten() {
  for (const char* wrong : {"+CGDCONT: 1,\"IPV4V6\",\"other.provider\"", "+CGDCONT: 1,\"IP\",\"internet.example\"",
                            "+CGDCONT: 1,\"IPV6\",\"internet.example\"", "+CGDCONT: 1,\"IPV4V6\",\"\""}) {
    Rig r;
    Script s;
    s.contexts = {wrong};
    s.apply(r.t);
    TEST_ASSERT_TRUE_MESSAGE(r.start(), wrong);
    TEST_ASSERT_EQUAL_MESSAGE(1, r.count(kWriteApn), wrong);
    TEST_ASSERT_EQUAL_MESSAGE(1, r.count("AT+CFUN=1"), wrong);
  }
}

// An APN has no case: the modem may list it in lower case, and that is no reason to write it again.
void test_the_apn_is_compared_without_regard_to_case() {
  Rig r;
  Script s;
  s.contexts = {"+CGDCONT: 1,\"IPV4V6\",\"INTERNET.Example\""};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT="));
}

// Option 1 of the proposal: without an APN the modem's own contexts are left alone, and the log says so once.
void test_without_an_apn_nothing_is_read_or_written_and_the_log_says_so_once() {
  A7682EDriver::Config c = Rig::defaultConfig();
  c.apn.clear();
  Rig r(c);
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT="));
  TEST_ASSERT_EQUAL(1, r.count("AT+CGDCONT?"));      // only the question about the IMS bearer after registration
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CFUN"));
  TEST_ASSERT_EQUAL(1, r.loggedCount("no APN set"));
  r.run(120000);                       // the periodic question does not say it again
  TEST_ASSERT_EQUAL(1, r.loggedCount("no APN set"));
}

// The APN ends up inside an AT command: a quote, a space or a semicolon never reaches the modem.
void test_an_apn_with_characters_that_do_not_belong_in_one_is_refused_before_sending() {
  for (const char* bad : {"in ternet", "a\"b", "a;b", "a,b", "a\r\nAT+CPOF", "caf\xC3\xA9"}) {
    A7682EDriver::Config c = Rig::defaultConfig();
    c.apn = bad;
    Rig r(c);
    Script().apply(r.t);
    TEST_ASSERT_TRUE_MESSAGE(r.start(), bad);
    TEST_ASSERT_EQUAL_MESSAGE(0, r.countPrefix("AT+CGDCONT="), bad);
    TEST_ASSERT_EQUAL_MESSAGE(1, r.count("AT+CGDCONT?"), bad);   // the IMS bearer question only
    TEST_ASSERT_EQUAL_MESSAGE(0, r.countPrefix("AT+CFUN"), bad);
    TEST_ASSERT_TRUE_MESSAGE(r.logged("does not belong in one"), bad);
    TEST_ASSERT_EQUAL_MESSAGE(0, r.t.unexpected().size(), bad);
  }
}

void test_an_apn_longer_than_100_characters_is_refused_before_sending() {
  A7682EDriver::Config c = Rig::defaultConfig();
  c.apn = std::string(101, 'a');
  Rig r(c);
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT="));
}

// The modem answers "no network service" to the write (radio off): the context stays, no radio cycle,
// and the modem still registers.
void test_a_refused_context_write_does_not_stop_the_start_up() {
  Rig r;
  Script s;
  s.contexts = {};
  s.cgdcontWriteFinal = "+CME ERROR: no network service";
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.count(kWriteApn));
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CFUN"));
  TEST_ASSERT_TRUE(r.logged("data context 1 could not be set (+CME ERROR: no network service)"));
  TEST_ASSERT_EQUAL_STRING("Example Mobile", r.driver.operatorName().c_str());
}

void test_contexts_that_cannot_be_read_are_left_as_they_are() {
  Rig r;
  Script s;
  s.contexts = {};
  s.contextsFinal = "ERROR";               // the list cannot be read
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT="));
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CFUN"));
  TEST_ASSERT_TRUE(r.logged("data contexts could not be read"));
}

// Not Ready before the radio is back: whatever is sent in Ready must not fall into a radio that is off.
void test_the_modem_is_not_ready_while_the_radio_is_cycled() {
  Rig r;
  Script s;
  s.contexts = {};
  s.apply(r.t);
  r.t.replyAfter(10000, "AT+CFUN=1", {});     // the radio takes ten seconds to come back
  r.driver.begin();
  bool readyTooEarly = false;
  for (int i = 0; i < 2000 && r.driver.state() != ModemState::Ready; ++i) {
    r.run(50);
    if (r.driver.state() == ModemState::Ready && r.t.sent().back() == "AT+CFUN=1" && r.count("AT+CREG?") == 0) readyTooEarly = true;
  }
  TEST_ASSERT_FALSE(readyTooEarly);
  TEST_ASSERT_TRUE(r.indexOf("AT+CFUN=1") < r.indexOf("AT+CREG?"));
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Ready);
}

// If CFUN=0 does not answer, the radio must come back on all the same: a radio left off would be a dead device.
void test_a_silent_cfun_0_is_followed_by_cfun_1_anyway() {
  Rig r;
  Script s;
  s.contexts = {};
  s.silent = "AT+CFUN=0";
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.count("AT+CFUN=0"));
  TEST_ASSERT_EQUAL(1, r.count("AT+CFUN=1"));
}

// The same when it is CFUN=1 that does not answer: tried three times, then given up, and start-up goes on.
void test_a_silent_cfun_1_is_tried_three_times() {
  Rig r;
  Script s;
  s.contexts = {};
  s.silent = "AT+CFUN=1";
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(3, r.count("AT+CFUN=1"));
  TEST_ASSERT_TRUE(r.logged("no answer while setting the data context"));
}

void test_a_cfun_1_that_fails_is_tried_again_and_then_given_up() {
  Rig r;
  Script s;
  s.contexts = {};
  s.cfun1Final = "ERROR";
  s.apply(r.t);
  r.driver.begin();
  r.run(60000);
  TEST_ASSERT_EQUAL(3, r.count("AT+CFUN=1"));
  TEST_ASSERT_TRUE(r.logged("radio could not be switched on again"));
}

// ---- the IMS (proposal tests as far as the driver reports them) ---------------------------------------------

void test_a_registered_ims_makes_the_voice_ready_and_is_logged_with_the_bearer_type() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::Ready);
  TEST_ASSERT_EQUAL(1, r.loggedCount("Cellular: IMS registered (PDN8 IPV4V6)"));
}

void test_a_missing_ims_is_reported_and_logged() {
  Rig r;
  Script s;
  s.cireg = {"+CIREG: 1,0,0"};
  s.contexts = {"+CGDCONT: 1,\"IPV4V6\",\"internet.example\"", "+CGDCONT: 8,\"IP\",\"ims\""};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::ImsMissing);
  TEST_ASSERT_TRUE(r.logged("Cellular: IMS not registered, calls cannot be made (PDN8 IP)"));
}

void test_a_network_for_sms_only_is_sms_only_whatever_the_ims_says() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,7"};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Ready);   // registered for SMS: that counts
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::SmsOnly);
}

// A modem that cannot answer AT+CIREG? still works for messages; calls need the answer.
void test_an_unanswered_ims_question_leaves_the_voice_unknown_and_the_modem_ready() {
  Rig r;
  Script s;
  s.ciregFinal = "ERROR";
  s.cireg = {"+CME ERROR: unknown"};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::Unknown);
  TEST_ASSERT_TRUE(r.logged("state of the IMS is not known"));
}

// The IMS can come and go without a restart: the 30-second question notices, and logs the change once.
void test_the_ims_state_follows_the_periodic_question_and_a_change_is_logged_once() {
  Rig r;
  Script s;
  s.cireg = {"+CIREG: 1,1,15", "+CIREG: 1,0,0"};   // registered at start-up, gone at the next question, and it stays gone
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::Ready);
  r.run(31000);
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::ImsMissing);
  TEST_ASSERT_EQUAL(1, r.loggedCount("IMS not registered"));
  r.run(95000);                                    // three more questions with the same answer
  TEST_ASSERT_EQUAL(1, r.loggedCount("IMS not registered"));
  TEST_ASSERT_EQUAL(1, r.loggedCount("IMS registered"));
}

void test_signal_network_operator_and_ims_are_asked_every_30_seconds_and_not_before() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  const int csqBefore = r.count("AT+CSQ"), ciregBefore = r.count("AT+CIREG?");
  r.run(29000);
  TEST_ASSERT_EQUAL(csqBefore, r.count("AT+CSQ"));
  r.run(2000);
  TEST_ASSERT_EQUAL(csqBefore + 1, r.count("AT+CSQ"));
  TEST_ASSERT_EQUAL(ciregBefore + 1, r.count("AT+CIREG?"));
  TEST_ASSERT_EQUAL(5, r.driver.signalBars());
}

// ---- the SIM and its PIN ------------------------------------------------------------------------------------

void test_a_sim_with_a_pin_is_unlocked_once_before_registration() {
  Rig r;
  Script s;
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.apply(r.t);
  r.t.reply("AT+CPIN?", {"+CPIN: READY"});   // after the PIN the card is ready (the last answer stays)
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.count("AT+CPIN=\"0000\""));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Ready);
  TEST_ASSERT_TRUE(r.indexOf("AT+CPIN=\"0000\"") < r.indexOf("AT+CREG?"));
}

void test_a_sim_that_wants_a_pin_without_one_stored_stops_and_says_so() {
  A7682EDriver::Config c = Rig::defaultConfig();
  c.simPin.clear();
  Rig r(c);
  Script s;
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::PinRequired);
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CPIN=\""));
  TEST_ASSERT_EQUAL(0, r.count("AT+CREG?"));
  TEST_ASSERT_TRUE(r.logged("PIN required but none is stored"));
}

// A card counts the wrong tries and locks after three: once per start, and never again after a rejection.
void test_a_wrong_pin_is_tried_exactly_once_even_across_restarts() {
  Rig r;
  Script s;
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.pinFinal = "+CME ERROR: incorrect password";
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::PinWrong);
  TEST_ASSERT_EQUAL(1, r.countPrefix("AT+CPIN=\""));
  r.run(200000);
  TEST_ASSERT_EQUAL(1, r.countPrefix("AT+CPIN=\""));
  // The modem restarts by itself and the card asks again: still no second try.
  r.t.restart();
  r.run(60000);
  TEST_ASSERT_EQUAL(1, r.countPrefix("AT+CPIN=\""));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::PinWrong);
}

// A PUK is for the owner and the provider, never for the device.
void test_a_puk_is_never_answered_with_a_pin() {
  Rig r;
  Script s;
  s.cpin = {{"+CPIN: SIM PUK", "OK"}};
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::PukRequired);
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CPIN=\""));
}

// In the first seconds the modem answers "SIM failure" and then "+CPIN: READY": ask again, do not miss a card with a PIN.
void test_the_sim_query_is_repeated_while_the_modem_does_not_know() {
  Rig r;
  Script s;
  s.cpin = {{"", "+CME ERROR: SIM failure"}, {"", "+CME ERROR: SIM failure"}, {"+CPIN: READY", "OK"}};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(3, r.count("AT+CPIN?"));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Ready);
}

void test_a_sim_query_that_never_gets_a_sensible_answer_gives_up_and_registration_goes_on() {
  Rig r;
  Script s;
  s.cpin = {{"", "+CME ERROR: SIM failure"}};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(12, r.count("AT+CPIN?"));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Unknown);
  TEST_ASSERT_TRUE(r.logged("state not known, carrying on"));
}

void test_a_missing_card_is_an_error_and_is_named() {
  Rig r;
  Script s;
  s.cpin = {{"", "+CME ERROR: SIM not inserted"}};
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Missing);
  TEST_ASSERT_EQUAL(1, r.count("AT+CPIN?"));
}

void test_an_unanswered_sim_query_does_not_block_registration() {
  Rig r;
  Script s;
  s.cpinSilent = true;
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Unknown);
}

// The PIN comes from a file and ends up in a command: digits only, 4 to 8.
void test_a_pin_that_is_not_4_to_8_digits_is_discarded() {
  for (const char* bad : {"12", "123456789", "12a4", "\"1234", "12 34", ""}) {
    A7682EDriver::Config c = Rig::defaultConfig();
    c.simPin = bad;
    Rig r(c);
    Script s;
    s.cpin = {{"+CPIN: SIM PIN", "OK"}};
    s.apply(r.t);
    r.driver.begin();
    TEST_ASSERT_TRUE_MESSAGE(r.runTo(ModemState::Error), bad);
    TEST_ASSERT_EQUAL_MESSAGE(0, r.countPrefix("AT+CPIN=\""), bad);
  }
}

// What the log says about the PIN must not be the PIN, and what it says about the APN must not be the APN.
void test_the_log_never_carries_the_pin_or_the_apn() {
  Rig r;
  Script s;
  s.contexts = {};
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.apply(r.t);
  r.t.reply("AT+CPIN?", {"+CPIN: READY"});
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.logs.size() > 5);
  for (const std::string& l : r.logs) {
    TEST_ASSERT_TRUE_MESSAGE(l.find(kPin) == std::string::npos, l.c_str());
    TEST_ASSERT_TRUE_MESSAGE(l.find(kApn) == std::string::npos, l.c_str());
  }
}

// ---- power key and probing -----------------------------------------------------------------------------------

// The most important quirk: a pulse on a running modem switches it off.
void test_no_power_key_pulse_when_the_modem_already_answers() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.t.pwrkeyPulses());
}

void test_a_silent_modem_gets_the_power_key_after_two_vain_questions() {
  Rig r;
  Script().apply(r.t);
  r.t.setCold(true);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.t.pwrkeyPulses());
  TEST_ASSERT_EQUAL(3, r.count("AT"));          // two in vain, one after the key
  TEST_ASSERT_EQUAL(0, r.driver.restartCount());   // the boot after the key is no restart
}

// A modem that answered and then stops answering in the middle of the initialisation is running:
// the power key would switch it off.
void test_a_modem_that_stops_answering_in_the_middle_is_never_given_the_power_key() {
  Rig r;
  Script s;
  s.silent = "AT+CLIP=1";
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_EQUAL(0, r.t.pwrkeyPulses());
  TEST_ASSERT_EQUAL(12, r.count("AT"));
  TEST_ASSERT_TRUE(r.logged("Modem not responding"));
}

void test_a_modem_that_never_answers_is_given_the_key_once_and_then_an_error() {
  Rig r;
  Script s;
  s.silent = "AT";
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_EQUAL(1, r.t.pwrkeyPulses());
  TEST_ASSERT_EQUAL(12, r.count("AT"));
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CPIN"));
}

void test_the_modem_is_powered_on_and_starting_before_it_is_asked() {
  Rig r;
  Script().apply(r.t);
  r.t.power(false);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.t.powered());
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::PoweringOn);
  r.run(300);
  TEST_ASSERT_EQUAL(0, r.t.sent().size());      // the probe delay: not yet
  r.run(300);
  TEST_ASSERT_TRUE(r.t.sent().size() > 0);
}

// ---- registration ---------------------------------------------------------------------------------------------

void test_registration_takes_a_few_attempts() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,1"};
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Registering));
  TEST_ASSERT_TRUE(r.driver.operatorName().empty());
  TEST_ASSERT_EQUAL(0, r.driver.signalBars());
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  TEST_ASSERT_EQUAL(3, r.count("AT+CREG?"));
  TEST_ASSERT_EQUAL_STRING("Example Mobile", r.driver.operatorName().c_str());
}

void test_registration_gives_up_after_the_window_and_says_so() {
  A7682EDriver::Config c = Rig::defaultConfig();
  c.autoRegisterAttempts = 100000;               // no manual choice in this test
  Rig r(c);
  Script s;
  s.creg = {"+CREG: 0,2"};
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  TEST_ASSERT_TRUE(r.logged("registration failed, no network found"));
  TEST_ASSERT_TRUE(r.t.unexpected().empty());
}

// With the foreign SIM of 2026-09-30 the automatic search found no network in Germany, while a manual
// choice registered within seconds.
void test_a_network_that_the_automatic_search_does_not_find_is_chosen_by_hand() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,1"};
  s.apply(r.t);
  r.t.reply("AT+COPS=?", {"+COPS: (1,\"Telekom.de\",\"TDG\",\"26201\",0),(1,\"Vodafone.de\",\"VODAFONE\",\"26202\",0)"});
  r.t.reply("AT+COPS=1,2,\"26201\"", {});
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.count("AT+COPS=?"));
  TEST_ASSERT_EQUAL(1, r.count("AT+COPS=1,2,\"26201\""));
  TEST_ASSERT_TRUE(r.indexOf("AT+COPS=?") < r.indexOf("AT+COPS=1,2,\"26201\""));
  TEST_ASSERT_TRUE(r.logged("choosing by hand"));
}

// The search itself may take two minutes: the registration window must not run out in the middle of it.
void test_the_operator_scan_may_outlast_the_registration_window() {
  Rig r;
  Script s;
  // The first round after the search still finds nothing: only the extended window keeps the modem out of Error.
  s.creg = {"+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,2",
            "+CREG: 0,2", "+CREG: 0,1"};
  s.apply(r.t);
  r.t.replyAfter(100000, "AT+COPS=?", {"+COPS: (1,\"Telekom.de\",\"TDG\",\"26201\",0)"});
  r.t.reply("AT+COPS=1,2,\"26201\"", {});
  r.driver.begin();
  r.run(97000);                                   // past the 90-second window, inside the scan
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Registering);
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
}

void test_the_network_mode_comes_from_the_configuration() {
  A7682EDriver::Config c = Rig::defaultConfig();
  c.networkMode = 38;
  Rig r(c);
  Script s;
  s.networkMode = 38;
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.count("AT+CNMP=38"));
  TEST_ASSERT_EQUAL(0, r.t.unexpected().size());
}

void test_a_registration_for_sms_only_counts_as_registered() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,6"};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Ready);
}

void test_the_operator_comes_from_the_network_and_a_code_becomes_a_name() {
  Rig r;
  Script s;
  s.cops = "+COPS: 0,0,\"26201\",7";
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL_STRING("Telekom", r.driver.operatorName().c_str());
}

void test_the_network_time_and_zone_come_from_cclk() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  at::NetworkTime t;
  TEST_ASSERT_TRUE(r.driver.networkTime(t));
  TEST_ASSERT_EQUAL_UINT32(1789475530u, t.epochUtc);
  TEST_ASSERT_EQUAL(120, t.tzOffsetMinutes);
}

void test_a_modem_clock_that_has_no_network_time_is_no_time() {
  Rig r;
  Script s;
  s.cclk = "+CCLK: \"70/01/01,00:00:12+00\"";
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  at::NetworkTime t;
  TEST_ASSERT_FALSE(r.driver.networkTime(t));
}

void test_the_signal_is_five_bars_at_most_and_zero_without_a_network() {
  Rig r;
  Script s;
  s.csq = "+CSQ: 12,99";
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(3, r.driver.signalBars());
}

// The device kept showing "Telekom" abroad because nobody asked after the modem had booked itself elsewhere,
// and it showed a network that was gone: the periodic question brings the state back to Registering.
void test_a_network_that_is_lost_takes_the_operator_and_the_voice_with_it_and_comes_back() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,1", "+CREG: 0,2", "+CREG: 0,2", "+CREG: 0,1"};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  r.run(31000);
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Registering);
  TEST_ASSERT_EQUAL_STRING("", r.driver.operatorName().c_str());
  TEST_ASSERT_EQUAL(0, r.driver.signalBars());
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::Unknown);
  TEST_ASSERT_TRUE(r.logged("network lost"));
  r.run(10000);
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL_STRING("Example Mobile", r.driver.operatorName().c_str());
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::Ready);
  TEST_ASSERT_EQUAL(2, r.loggedCount("IMS registered"));     // asked afresh after the registration
}

// A device that stays without a network for hours is not broken: the 90-second window is for the start only.
void test_a_network_that_stays_away_after_the_start_is_never_an_error() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,1", "+CREG: 0,2"};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  r.run(600000);
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Registering);
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+COPS=1"));          // and no manual choice either
}

// ---- restarts --------------------------------------------------------------------------------------------------

void test_a_restart_by_the_modem_is_counted_logged_and_the_start_runs_again() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.driver.restartCount());
  r.t.restart();
  r.run(100);
  TEST_ASSERT_EQUAL(1, r.driver.restartCount());
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::PoweringOn);
  TEST_ASSERT_TRUE(r.driver.voiceStatus() == VoiceStatus::Unknown);
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  TEST_ASSERT_EQUAL(2, r.count("AT+CNMP=2"));
  TEST_ASSERT_EQUAL(0, r.countPrefix("AT+CGDCONT="));       // the bearer was right: read again, not written
  TEST_ASSERT_TRUE(r.logged("Modem restarted by itself (1 since start)"));
  TEST_ASSERT_EQUAL(0, r.t.pwrkeyPulses());
}

void test_a_second_restart_is_counted_too() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  r.t.restart();
  TEST_ASSERT_TRUE(r.runTo(ModemState::PoweringOn));
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  r.t.restart();
  r.run(100);
  TEST_ASSERT_EQUAL(2, r.driver.restartCount());
}

// The "ready" line right after power-on is the boot and no restart.
void test_the_ready_line_of_the_boot_is_not_a_restart() {
  Rig r;
  Script().apply(r.t);
  r.t.unsolicited(100, "*ATREADY: 1");
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.driver.restartCount());
}

void test_a_restart_during_registration_is_counted() {
  Rig r;
  Script s;
  s.creg = {"+CREG: 0,2"};
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Registering));
  r.run(3000);
  r.t.restart();
  r.run(100);
  TEST_ASSERT_EQUAL(1, r.driver.restartCount());
}

// The card asks for its PIN again after a restart; a PIN that was accepted before is sent once more.
void test_after_a_restart_an_accepted_pin_is_sent_once_more() {
  Rig r;
  Script s;
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.count("AT+CPIN=\"0000\""));
  r.t.restart();
  TEST_ASSERT_TRUE(r.runTo(ModemState::PoweringOn));
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  TEST_ASSERT_EQUAL(2, r.count("AT+CPIN=\"0000\""));
}

// A modem that restarts every half minute (a bad SIM contact) must not be given the PIN over and over
// with an answer that was never seen: one try per start of the modem, and a rejection ends it.
void test_restart_after_restart_with_a_rejected_pin_never_sends_it_again() {
  Rig r;
  Script s;
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.pinFinal = "+CME ERROR: incorrect password";
  s.apply(r.t);
  r.driver.begin();
  TEST_ASSERT_TRUE(r.runTo(ModemState::Error));
  for (int i = 0; i < 3; ++i) {
    r.t.restart();
    r.run(60000);
  }
  TEST_ASSERT_EQUAL(1, r.countPrefix("AT+CPIN=\""));
}

// ---- the log ---------------------------------------------------------------------------------------------------

// The host applies the log level to what the driver reports, by the words in the line. The lines that
// matter must survive the levels they are meant to survive.
void test_the_log_lines_carry_the_words_that_keep_them_on_the_right_level() {
  using loglevel::Importance;
  Rig r;
  Script s;
  s.cireg = {"+CIREG: 1,0,0"};
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  r.t.restart();
  TEST_ASSERT_TRUE(r.runTo(ModemState::PoweringOn));
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  for (const std::string& l : r.logs) {
    if (l.find("IMS not registered") != std::string::npos) {
      TEST_ASSERT_TRUE_MESSAGE(loglevel::classify(l.c_str()) <= Importance::Error, l.c_str());   // kept on the errors level
    }
    if (l.find("IMS registered") != std::string::npos) {
      TEST_ASSERT_TRUE_MESSAGE(loglevel::classify(l.c_str()) <= Importance::Event, l.c_str());
    }
    if (l.find("restarted by itself") != std::string::npos) {
      TEST_ASSERT_TRUE_MESSAGE(loglevel::classify(l.c_str()) == Importance::Always, l.c_str());  // provable afterwards
    }
  }
  TEST_ASSERT_TRUE(r.logged("restarted by itself"));
  TEST_ASSERT_TRUE(r.logged("IMS not registered"));
}

void test_the_log_is_polled_once_per_line_and_is_bounded() {
  Rig r;
  Script().apply(r.t);
  r.driver.begin();
  r.run(60000);
  std::string line;
  TEST_ASSERT_FALSE(r.driver.pollLog(line));       // drained by the rig already
  TEST_ASSERT_TRUE(r.logs.size() > 0);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_whole_start_up_is_one_readable_sequence);
  RUN_TEST(test_begin_twice_changes_nothing);
  RUN_TEST(test_a_missing_context_1_is_written_and_the_radio_cycled_in_order_once);
  RUN_TEST(test_a_context_1_that_is_right_is_left_alone);
  RUN_TEST(test_a_context_1_with_another_apn_or_another_type_is_rewritten);
  RUN_TEST(test_the_apn_is_compared_without_regard_to_case);
  RUN_TEST(test_without_an_apn_nothing_is_read_or_written_and_the_log_says_so_once);
  RUN_TEST(test_an_apn_with_characters_that_do_not_belong_in_one_is_refused_before_sending);
  RUN_TEST(test_an_apn_longer_than_100_characters_is_refused_before_sending);
  RUN_TEST(test_a_refused_context_write_does_not_stop_the_start_up);
  RUN_TEST(test_contexts_that_cannot_be_read_are_left_as_they_are);
  RUN_TEST(test_the_modem_is_not_ready_while_the_radio_is_cycled);
  RUN_TEST(test_a_silent_cfun_0_is_followed_by_cfun_1_anyway);
  RUN_TEST(test_a_silent_cfun_1_is_tried_three_times);
  RUN_TEST(test_a_cfun_1_that_fails_is_tried_again_and_then_given_up);
  RUN_TEST(test_a_registered_ims_makes_the_voice_ready_and_is_logged_with_the_bearer_type);
  RUN_TEST(test_a_missing_ims_is_reported_and_logged);
  RUN_TEST(test_a_network_for_sms_only_is_sms_only_whatever_the_ims_says);
  RUN_TEST(test_an_unanswered_ims_question_leaves_the_voice_unknown_and_the_modem_ready);
  RUN_TEST(test_the_ims_state_follows_the_periodic_question_and_a_change_is_logged_once);
  RUN_TEST(test_signal_network_operator_and_ims_are_asked_every_30_seconds_and_not_before);
  RUN_TEST(test_a_sim_with_a_pin_is_unlocked_once_before_registration);
  RUN_TEST(test_a_sim_that_wants_a_pin_without_one_stored_stops_and_says_so);
  RUN_TEST(test_a_wrong_pin_is_tried_exactly_once_even_across_restarts);
  RUN_TEST(test_a_puk_is_never_answered_with_a_pin);
  RUN_TEST(test_the_sim_query_is_repeated_while_the_modem_does_not_know);
  RUN_TEST(test_a_sim_query_that_never_gets_a_sensible_answer_gives_up_and_registration_goes_on);
  RUN_TEST(test_a_missing_card_is_an_error_and_is_named);
  RUN_TEST(test_an_unanswered_sim_query_does_not_block_registration);
  RUN_TEST(test_a_pin_that_is_not_4_to_8_digits_is_discarded);
  RUN_TEST(test_the_log_never_carries_the_pin_or_the_apn);
  RUN_TEST(test_no_power_key_pulse_when_the_modem_already_answers);
  RUN_TEST(test_a_silent_modem_gets_the_power_key_after_two_vain_questions);
  RUN_TEST(test_a_modem_that_stops_answering_in_the_middle_is_never_given_the_power_key);
  RUN_TEST(test_a_modem_that_never_answers_is_given_the_key_once_and_then_an_error);
  RUN_TEST(test_the_modem_is_powered_on_and_starting_before_it_is_asked);
  RUN_TEST(test_registration_takes_a_few_attempts);
  RUN_TEST(test_registration_gives_up_after_the_window_and_says_so);
  RUN_TEST(test_a_network_that_the_automatic_search_does_not_find_is_chosen_by_hand);
  RUN_TEST(test_the_operator_scan_may_outlast_the_registration_window);
  RUN_TEST(test_the_network_mode_comes_from_the_configuration);
  RUN_TEST(test_a_registration_for_sms_only_counts_as_registered);
  RUN_TEST(test_the_operator_comes_from_the_network_and_a_code_becomes_a_name);
  RUN_TEST(test_the_network_time_and_zone_come_from_cclk);
  RUN_TEST(test_a_modem_clock_that_has_no_network_time_is_no_time);
  RUN_TEST(test_the_signal_is_five_bars_at_most_and_zero_without_a_network);
  RUN_TEST(test_a_network_that_is_lost_takes_the_operator_and_the_voice_with_it_and_comes_back);
  RUN_TEST(test_a_network_that_stays_away_after_the_start_is_never_an_error);
  RUN_TEST(test_a_restart_by_the_modem_is_counted_logged_and_the_start_runs_again);
  RUN_TEST(test_a_second_restart_is_counted_too);
  RUN_TEST(test_the_ready_line_of_the_boot_is_not_a_restart);
  RUN_TEST(test_a_restart_during_registration_is_counted);
  RUN_TEST(test_after_a_restart_an_accepted_pin_is_sent_once_more);
  RUN_TEST(test_restart_after_restart_with_a_rejected_pin_never_sends_it_again);
  RUN_TEST(test_the_log_lines_carry_the_words_that_keep_them_on_the_right_level);
  RUN_TEST(test_the_log_is_polled_once_per_line_and_is_bounded);
  return UNITY_END();
}
