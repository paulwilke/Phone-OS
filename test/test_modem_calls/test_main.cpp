// Host tests for the calls of the A7682E driver: dialling with the IMS check, the progress through
// AT+CLCC, VOICE CALL BEGIN and END, NO CARRIER, BUSY, NO ANSWER, incoming calls, hanging up, the phantom
// call, a restart of the modem in the middle of a call, the volume and the sound path. The driver runs
// against a scripted modem; the answers are built after 3GPP TS 27.007 and what the previous driver met
// on the device. Nothing here has run against the real modem.

#include <unity.h>

#include <string>
#include <vector>

#include <ModemScript.h>
#include <app/LogLevel.h>
#include <drivers/A7682EDriver.h>
#include <drivers/Ucs2.h>

using namespace modemtest;

void setUp() {}
void tearDown() {}

namespace {

const std::string kNumber = "+491701111111";
const std::string kAtd = "ATD" + kNumber + ";";

// Hex UCS-2 of a text, as the modem writes a number after AT+CSCS="UCS2".
std::string ucs2(const std::string& text) {
  char hex[256];
  TEST_ASSERT_TRUE(text::utf8ToUcs2Hex(text.c_str(), hex, sizeof(hex)) >= 0);
  return hex;
}

bool types(const std::vector<CallEvent>& got, std::initializer_list<CallEventType> want) {
  if (got.size() != want.size()) return false;
  size_t i = 0;
  for (CallEventType t : want)
    if (got[i++].type != t) return false;
  return true;
}

// A started rig with a healthy IMS: the modem is Ready and calls are possible.
struct Ready {
  Rig r;
  Script s;
  explicit Ready(void (*tweak)(Script&) = nullptr) {
    if (tweak) tweak(s);
    s.apply(r.t);
    TEST_ASSERT_TRUE(r.start());
    r.events();
  }
};

}  // namespace

// ---- dialling (proposal tests 3, 4 and 5) -------------------------------------------------------------------

// Test 3: +CIREG: 1,0,0 -> dial() refused, nothing sent.
void test_a_dial_without_ims_is_refused_and_no_atd_is_sent() {
  Ready m([](Script& s) { s.cireg = {"+CIREG: 1,0,0"}; });
  TEST_ASSERT_TRUE(m.r.driver.voiceStatus() == VoiceStatus::ImsMissing);
  TEST_ASSERT_FALSE(m.r.driver.dial(kNumber));
  m.r.run(5000);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL(0, m.r.countPrefix("ATD"));
  TEST_ASSERT_TRUE(m.r.events().empty());
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// Test 4: +CIREG: 1,1,15 -> ATD sent; +CLCC 2 -> 3 -> 0 gives dialling -> ringing -> connected.
void test_a_dial_with_ims_goes_through_dialling_ringing_and_connected() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,2,0,0,\"+491701111111\",145", "+CLCC: 1,0,3,0,0,\"+491701111111\",145", "+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  TEST_ASSERT_TRUE(m.r.driver.dial(kNumber));
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);   // at once, as the capability says
  m.r.run(200);
  TEST_ASSERT_EQUAL(1, m.r.count(kAtd));
  TEST_ASSERT_TRUE(m.r.indexOf("AT+CIREG?") < m.r.indexOf(kAtd));      // asked before dialling, again
  TEST_ASSERT_TRUE(m.r.t.audioToModem());                              // from the first dial tone
  m.r.run(1100);                                                        // CLCC: 2
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  TEST_ASSERT_FALSE(m.r.logged("ringing at the other end"));
  m.r.run(1100);                                                        // CLCC: 3
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  TEST_ASSERT_TRUE(m.r.logged("ringing at the other end"));
  TEST_ASSERT_TRUE(m.r.events().empty());                              // ringing at the other end is no event
  m.r.run(1100);                                                        // CLCC: 0
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Connected}));
  TEST_ASSERT_EQUAL_STRING(kNumber.c_str(), got[0].phone.c_str());
  TEST_ASSERT_TRUE(m.r.t.audioToModem());
}

void test_a_connected_call_ends_with_its_length_when_we_hang_up() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  m.r.run(12000);
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  m.r.run(200);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CHUP"));
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Connected, CallEventType::Ended}));
  TEST_ASSERT_TRUE(got[1].duration >= 12 && got[1].duration <= 14);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
  m.r.run(5000);
  TEST_ASSERT_TRUE(m.r.events().empty());                              // the NO CARRIER after the CHUP is no second end
}

// Test 5: the state of 30 seconds ago said "registered", the modem lost the IMS since: the question right before
// ATD says so, ATD is not sent, and the BEGIN that the modem then sometimes reports is no call.
void test_a_dial_after_the_ims_was_lost_is_a_failed_call_and_a_phantom_begin_is_no_call() {
  Ready m([](Script& s) { s.cireg = {"+CIREG: 1,1,15", "+CIREG: 1,0,0"}; });   // registered at start-up, gone at the dial
  TEST_ASSERT_TRUE(m.r.driver.voiceStatus() == VoiceStatus::Ready);            // what the cache says
  TEST_ASSERT_TRUE(m.r.driver.dial(kNumber));
  m.r.urc("VOICE CALL: BEGIN", 200);                                           // the phantom, right behind the dial
  TEST_ASSERT_EQUAL(0, m.r.countPrefix("ATD"));
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));
  TEST_ASSERT_TRUE(got[0].failReason == DialFailReason::NotReady);
  TEST_ASSERT_TRUE(got[0].reason.find("CIREG 1,0,0") != std::string::npos);    // the IMS facts travel with the failure
  TEST_ASSERT_TRUE(got[0].reason.find("PDN8 IPV4V6") != std::string::npos);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// A BEGIN with no call at all is ignored and said so.
void test_a_begin_without_a_call_is_ignored() {
  Ready m;
  m.r.urc("VOICE CALL: BEGIN");
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_TRUE(m.r.events().empty());
  TEST_ASSERT_TRUE(m.r.logged("VOICE CALL BEGIN without a call, ignored"));
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// Between dial() and ATD nothing has been dialled: a BEGIN then is not the call.
void test_a_begin_before_atd_has_gone_out_is_ignored() {
  Ready m;
  m.r.driver.dial(kNumber);
  m.r.t.unsolicited(m.r.t.now(), "VOICE CALL: BEGIN");
  m.r.driver.poll();                          // reads the BEGIN first; the dial job has not started yet
  m.r.drain();
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  TEST_ASSERT_TRUE(m.r.events().empty());
  TEST_ASSERT_TRUE(m.r.logged("BEGIN without a call, ignored"));
  m.r.run(300);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);   // ATD went out, nothing is connected
  TEST_ASSERT_TRUE(m.r.events().empty());
}

// Test 6: *ATREADY while dialling -> a failed call "modem restarted", then the initialisation again.
void test_a_restart_while_dialling_fails_the_call_and_starts_over() {
  Ready m([](Script& s) { s.silent = "ATD+491701111111;"; });   // ATD hangs the modem
  m.r.driver.dial(kNumber);
  m.r.run(7000);                             // about seven seconds without an answer ...
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  m.r.t.restart();                           // ... then *ATREADY
  m.r.run(100);
  TEST_ASSERT_EQUAL(1, m.r.driver.restartCount());
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));
  TEST_ASSERT_TRUE(got[0].failReason == DialFailReason::ModemRestarted);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
  TEST_ASSERT_TRUE(m.r.runTo(ModemState::Ready));
  TEST_ASSERT_EQUAL(2, m.r.count("AT+CNMP=2"));                         // initialised again
  TEST_ASSERT_TRUE(m.r.events().empty());                                // and no second report
}

// The modem hangs and reports nothing: after the limit (longer than the seven seconds of the real hang, so that
// a restart is recognised as one) the dial failed, and the call is hung up for safety.
void test_an_atd_that_is_never_answered_fails_after_the_limit_and_is_hung_up() {
  Ready m([](Script& s) { s.silent = "ATD+491701111111;"; });
  m.r.driver.dial(kNumber);
  m.r.run(14000);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  m.r.run(2000);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));
  TEST_ASSERT_TRUE(got[0].failReason == DialFailReason::Unknown);
  TEST_ASSERT_TRUE(got[0].reason.find("no answer to ATD") != std::string::npos);
  m.r.run(500);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CHUP"));
}

void test_an_atd_the_modem_refuses_is_a_refused_dial_with_its_line() {
  Ready m([](Script& s) { s.atdFinal = "+CME ERROR: operation not allowed"; });
  m.r.driver.dial(kNumber);
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));
  TEST_ASSERT_TRUE(got[0].failReason == DialFailReason::Refused);
  TEST_ASSERT_TRUE(got[0].reason.find("+CME ERROR: operation not allowed") != std::string::npos);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// ---- a dial is refused ----------------------------------------------------------------------------------------

void test_a_dial_is_refused_for_a_number_that_is_not_a_dial_string_or_a_second_call() {
  Ready m;
  for (const char* bad : {"", "+", "0170 1111", "\"; AT+CPOF", "+4917011111111111111111111", "49+170"}) {
    TEST_ASSERT_FALSE_MESSAGE(m.r.driver.dial(bad), bad);
  }
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_TRUE(m.r.driver.dial(kNumber));
  TEST_ASSERT_FALSE(m.r.driver.dial("+491702222222"));      // already dialling
  TEST_ASSERT_FALSE(m.r.driver.answer());                   // nothing rings
  m.r.run(500);
  TEST_ASSERT_EQUAL(1, m.r.countPrefix("ATD"));
}

void test_a_dial_is_refused_while_the_modem_is_not_ready_or_the_network_is_sms_only() {
  {
    Rig r;
    Script().apply(r.t);
    TEST_ASSERT_FALSE(r.driver.dial(kNumber));              // off
    r.driver.begin();
    TEST_ASSERT_FALSE(r.driver.dial(kNumber));              // starting
  }
  Ready m([](Script& s) { s.creg = {"+CREG: 0,7"}; });
  TEST_ASSERT_TRUE(m.r.driver.voiceStatus() == VoiceStatus::SmsOnly);
  TEST_ASSERT_FALSE(m.r.driver.dial(kNumber));
  m.r.run(2000);
  TEST_ASSERT_EQUAL(0, m.r.countPrefix("ATD"));
}

// ---- how a dialled call ends -------------------------------------------------------------------------------------

void test_busy_and_no_answer_each_end_the_call_with_one_event() {
  for (const char* line : {"BUSY", "NO ANSWER"}) {
    Ready m;
    m.r.driver.dial(kNumber);
    m.r.run(300);
    m.r.urc(line);
    TEST_ASSERT_TRUE_MESSAGE(m.r.driver.state() == ModemState::Ready, line);
    const auto got = m.r.events();
    TEST_ASSERT_TRUE_MESSAGE(types(got, {std::string(line) == "BUSY" ? CallEventType::Busy : CallEventType::NoAnswer}), line);
    TEST_ASSERT_FALSE_MESSAGE(m.r.t.audioToModem(), line);
  }
}

// The network refuses the call (a SIM for SMS only, registration 7): NO CARRIER before it ever connected. That is a
// failed call, not a conversation that ended, and it is reported after the network was asked why (AT+CEER).
void test_a_refused_call_is_a_dial_failure_with_the_network_cause_and_the_lines_before() {
  Ready m([](Script& s) { s.ceer = "NORMAL_CALL_CLEARING"; });
  m.r.driver.dial(kNumber);
  m.r.run(300);
  m.r.urc("NO CARRIER", 500);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CEER"));
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));
  TEST_ASSERT_EQUAL_STRING(kNumber.c_str(), got[0].phone.c_str());
  TEST_ASSERT_TRUE(got[0].reason.find("NO CARRIER") == 0);
  TEST_ASSERT_TRUE(got[0].reason.find("lines: NO CARRIER") != std::string::npos);
  TEST_ASSERT_TRUE(got[0].reason.find("CIREG 1,1,15 PDN8 IPV4V6") != std::string::npos);
  TEST_ASSERT_TRUE(got[0].reason.find("NORMAL_CALL_CLEARING") != std::string::npos);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// The event is held back until the network has been asked: the reason is part of it.
void test_the_failed_call_is_reported_only_after_the_network_was_asked_why() {
  Ready m([](Script& s) { s.ceer = "USER_BUSY"; });
  m.r.driver.dial(kNumber);
  m.r.run(300);
  m.r.t.unsolicited(m.r.t.now(), "NO CARRIER");
  m.r.driver.poll();                          // reads NO CARRIER and sends AT+CEER in the same pass
  m.r.driver.poll();
  const auto got = m.r.events();
  TEST_ASSERT_EQUAL(1, got.size());
  TEST_ASSERT_TRUE(got[0].reason.find("USER_BUSY") != std::string::npos);
}

// END followed by NO CARRIER of an outgoing call that never connected is one failed call, not two events.
void test_end_and_no_carrier_together_are_one_failed_call() {
  Ready m([](Script& s) { s.ceer = "USER_BUSY"; });
  m.r.driver.dial(kNumber);
  m.r.run(300);
  m.r.t.unsolicited(m.r.t.now(), "VOICE CALL: END: 000002");
  m.r.t.unsolicited(m.r.t.now(), "NO CARRIER");
  m.r.run(600);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));      // never connected: failed, with no length
  TEST_ASSERT_EQUAL_UINT32(0, got[0].duration);
}

void test_the_end_of_a_connected_call_gives_its_length_from_the_modem() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  m.r.urc("VOICE CALL: END: 0:42");
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Connected, CallEventType::Ended}));
  TEST_ASSERT_EQUAL_UINT32(42, got[1].duration);
  m.r.urc("NO CARRIER");                                          // and the NO CARRIER behind it is nothing new
  TEST_ASSERT_TRUE(m.r.events().empty());
}

void test_the_length_is_measured_when_the_modem_does_not_say_it() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  m.r.run(20000);
  m.r.urc("NO CARRIER");
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Connected, CallEventType::Ended}));
  TEST_ASSERT_TRUE(got[1].duration >= 19 && got[1].duration <= 22);
}

// A call that was listed and then is gone from the list, with no word from the modem, is over.
void test_a_call_that_vanishes_from_the_call_list_is_a_failed_dial() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,2,0,0,\"+491701111111\",145", ""}; });
  m.r.driver.dial(kNumber);
  m.r.run(1300);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  m.r.run(1300);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  m.r.run(500);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::DialFailed}));
  TEST_ASSERT_TRUE(got[0].reason.find("no longer in the call list") != std::string::npos);
}

// An empty list before the call was ever listed is just too early, not the end.
void test_an_empty_call_list_before_the_call_appears_is_not_the_end() {
  Ready m;
  m.r.driver.dial(kNumber);
  m.r.run(5000);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Dialing);
  TEST_ASSERT_TRUE(m.r.events().empty());
}

// ---- hanging up before and while dialling ---------------------------------------------------------------------------------

void test_a_dial_that_is_cancelled_before_atd_goes_out_sends_nothing() {
  Ready m;
  m.r.driver.dial(kNumber);
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  m.r.run(3000);
  TEST_ASSERT_EQUAL(0, m.r.countPrefix("ATD"));
  TEST_ASSERT_EQUAL(0, m.r.count("AT+CHUP"));                      // nothing was sent, nothing to hang up
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Ended}));
  TEST_ASSERT_EQUAL_UINT32(0, got[0].duration);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// Between the IMS question and ATD: the question is answered, but the call is already gone.
void test_a_dial_cancelled_while_the_ims_question_is_out_sends_no_atd() {
  Ready m;
  m.r.driver.dial(kNumber);
  m.r.driver.poll();                                              // the job has started and asked
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  m.r.run(3000);
  TEST_ASSERT_EQUAL(0, m.r.countPrefix("ATD"));
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_TRUE(types(m.r.events(), {CallEventType::Ended}));
}

void test_a_dial_cancelled_after_atd_is_hung_up_and_a_late_connection_is_ignored() {
  Ready m;
  m.r.driver.dial(kNumber);
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count(kAtd));
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CHUP"));
  m.r.urc("VOICE CALL: BEGIN");                                   // the call connected before the CHUP arrived
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Ended}));
}

// The first dial is cancelled while its job is still out: a new dial must not start a second job on top of it.
void test_a_new_dial_is_refused_while_the_cancelled_dial_is_still_being_set_up() {
  Ready m;
  m.r.driver.dial(kNumber);
  m.r.driver.poll();                                              // the dial job has asked about the IMS
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  TEST_ASSERT_FALSE(m.r.driver.dial(kNumber));
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.driver.dial(kNumber));                     // once the job is over, it is possible again
}

// ---- incoming calls ------------------------------------------------------------------------------------------------------

void test_an_incoming_call_is_announced_again_with_its_number_answered_and_ended() {
  Ready m;
  m.r.urc("RING");
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::RingingIn);
  TEST_ASSERT_TRUE(m.r.t.audioToModem());                          // from the first ring on
  m.r.urc("+CLIP: \"" + ucs2(kNumber) + "\",145,,,,0");
  auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Incoming, CallEventType::Incoming}));
  TEST_ASSERT_EQUAL_STRING("", got[0].phone.c_str());
  TEST_ASSERT_EQUAL_STRING(kNumber.c_str(), got[1].phone.c_str());

  TEST_ASSERT_TRUE(m.r.driver.answer());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::RingingIn);   // until the modem says OK to ATA
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("ATA"));
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  m.r.urc("VOICE CALL: BEGIN");                                    // and the BEGIN behind it is not a second connection
  m.r.run(5000);
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CHUP"));
  got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Connected, CallEventType::Ended}));
  TEST_ASSERT_TRUE(got[1].duration >= 4 && got[1].duration <= 7);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// RING comes again every few seconds: one ringing call, one event.
void test_a_ring_that_repeats_is_one_incoming_call() {
  Ready m;
  m.r.urc("RING");
  m.r.urc("RING");
  m.r.urc("RING");
  TEST_ASSERT_TRUE(types(m.r.events(), {CallEventType::Incoming}));
}

void test_a_caller_who_gives_up_is_a_missed_call_and_the_sound_path_goes_back() {
  Ready m;
  m.r.urc("RING");
  TEST_ASSERT_TRUE(m.r.t.audioToModem());
  m.r.urc("NO CARRIER");
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_TRUE(types(m.r.events(), {CallEventType::Incoming, CallEventType::Missed}));
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

void test_a_ringing_call_that_we_refuse_is_missed_and_hung_up() {
  Ready m;
  m.r.urc("RING");
  TEST_ASSERT_TRUE(m.r.driver.hangup());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CHUP"));
  TEST_ASSERT_TRUE(types(m.r.events(), {CallEventType::Incoming, CallEventType::Missed}));
}

// If the modem refuses ATA the call is not connected and keeps ringing: no Connected, no lie.
void test_an_answer_that_the_modem_refuses_connects_nothing() {
  Ready m([](Script& s) { s.ataFinal = "+CME ERROR: operation not allowed"; });
  m.r.urc("RING");
  m.r.events();
  m.r.driver.answer();
  m.r.run(300);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::RingingIn);
  TEST_ASSERT_TRUE(m.r.events().empty());
  TEST_ASSERT_TRUE(m.r.logged("refused to answer"));
}

// Anything that is not a ring in Ready is nothing: a RING during a call is a call waiting and does not turn it into another.
void test_a_ring_during_a_call_changes_nothing() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  m.r.events();
  m.r.urc("RING");
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  TEST_ASSERT_TRUE(m.r.events().empty());
}

void test_busy_or_no_carrier_or_end_without_a_call_are_nothing() {
  Ready m;
  for (const char* line : {"BUSY", "NO ANSWER", "NO CARRIER", "VOICE CALL: END: 0:42"}) {
    m.r.urc(line);
    TEST_ASSERT_TRUE_MESSAGE(m.r.driver.state() == ModemState::Ready, line);
    TEST_ASSERT_TRUE_MESSAGE(m.r.events().empty(), line);
  }
}

// ---- a restart in a call -------------------------------------------------------------------------------------------------------

void test_a_restart_in_a_connected_call_ends_it_once() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  m.r.events();
  m.r.run(8000);
  m.r.t.restart();
  m.r.run(100);
  const auto got = m.r.events();
  TEST_ASSERT_TRUE(types(got, {CallEventType::Ended}));
  TEST_ASSERT_TRUE(got[0].duration >= 7 && got[0].duration <= 10);
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
  TEST_ASSERT_TRUE(m.r.runTo(ModemState::Ready));
  TEST_ASSERT_TRUE(m.r.events().empty());
}

void test_a_restart_while_ringing_is_a_missed_call() {
  Ready m;
  m.r.urc("RING");
  m.r.events();
  m.r.t.restart();
  m.r.run(100);
  TEST_ASSERT_TRUE(types(m.r.events(), {CallEventType::Missed}));
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

// ---- the volume ----------------------------------------------------------------------------------------------------------------------

void test_the_volume_uses_coutgain_is_clamped_and_works_in_a_call() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  TEST_ASSERT_EQUAL(4, m.r.driver.callVolume());
  TEST_ASSERT_TRUE(m.r.driver.setCallVolume(6));
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+COUTGAIN=6"));
  TEST_ASSERT_EQUAL(6, m.r.driver.callVolume());
  m.r.driver.setCallVolume(99);
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+COUTGAIN=7"));
  TEST_ASSERT_EQUAL(7, m.r.driver.callVolume());
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  m.r.driver.setCallVolume(-3);
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+COUTGAIN=0"));       // at once, in the middle of the call
  TEST_ASSERT_EQUAL(0, m.r.driver.callVolume());
  TEST_ASSERT_EQUAL(0, m.r.countPrefix("AT+CLVL"));       // the A76XX series does not know it
}

void test_the_same_volume_is_not_sent_again() {
  Ready m;
  m.r.driver.setCallVolume(4);
  m.r.run(300);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+COUTGAIN=4"));       // the one of the initialisation
}

// A restart puts the modem back to its default volume; what the child chose comes back.
void test_the_chosen_volume_comes_back_after_a_restart() {
  Ready m;
  m.r.driver.setCallVolume(6);
  m.r.run(300);
  m.r.t.restart();
  TEST_ASSERT_TRUE(m.r.runTo(ModemState::PoweringOn));
  TEST_ASSERT_TRUE(m.r.runTo(ModemState::Ready));
  m.r.run(300);
  TEST_ASSERT_EQUAL(2, m.r.count("AT+COUTGAIN=6"));
  TEST_ASSERT_EQUAL(2, m.r.count("AT+COUTGAIN=4"));
  TEST_ASSERT_TRUE(m.r.indexOf("AT+COUTGAIN=6") < (int)m.r.t.sent().size());
}

// ---- the log of calls ------------------------------------------------------------------------------------------------------------------

// A log line may say that a call was made, never to whom.
void test_no_log_line_of_a_call_carries_the_number() {
  Ready m([](Script& s) { s.ceer = "NORMAL_CALL_CLEARING"; });
  m.r.driver.dial(kNumber);
  m.r.run(300);
  m.r.urc("NO CARRIER", 500);
  m.r.urc("RING");
  m.r.urc("+CLIP: \"" + ucs2(kNumber) + "\",145,,,,0");
  m.r.urc("NO CARRIER");
  for (const std::string& l : m.r.logs) {
    TEST_ASSERT_TRUE_MESSAGE(l.find("1701111111") == std::string::npos, l.c_str());
    TEST_ASSERT_TRUE_MESSAGE(l.find(ucs2("1111")) == std::string::npos, l.c_str());
  }
  TEST_ASSERT_TRUE(m.r.logged("Call: dialling"));
  TEST_ASSERT_TRUE(m.r.logged("Call: incoming"));
}

// The host applies the log level by the words of a line: a call is an event, a failed call is an error and survives the
// errors level.
void test_the_call_log_lines_carry_the_words_that_keep_them_on_the_right_level() {
  using loglevel::Importance;
  Ready m;
  m.r.driver.dial(kNumber);
  m.r.run(300);
  m.r.urc("NO CARRIER", 500);
  m.r.urc("RING");
  for (const std::string& l : m.r.logs) {
    if (l.find("Call: dial failed") != std::string::npos) TEST_ASSERT_TRUE_MESSAGE(loglevel::classify(l.c_str()) <= Importance::Error, l.c_str());
    if (l.find("Call: dialling") != std::string::npos || l.find("Call: incoming") != std::string::npos)
      TEST_ASSERT_TRUE_MESSAGE(loglevel::classify(l.c_str()) <= Importance::Event, l.c_str());
  }
  TEST_ASSERT_TRUE(m.r.logged("Call: dial failed"));
}

// ---- the sound path -------------------------------------------------------------------------------------------------------------------

void test_the_sound_path_is_switched_exactly_when_a_call_begins_and_ends() {
  Ready m;
  const int before = m.r.t.audioSwitches();
  m.r.urc("RING");
  m.r.driver.answer();
  m.r.run(300);                                           // Ringing -> InCall: still on, no second switch
  m.r.urc("NO CARRIER");
  TEST_ASSERT_EQUAL(before + 2, m.r.t.audioSwitches());   // on once, off once
  TEST_ASSERT_FALSE(m.r.t.audioToModem());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_dial_without_ims_is_refused_and_no_atd_is_sent);
  RUN_TEST(test_a_dial_with_ims_goes_through_dialling_ringing_and_connected);
  RUN_TEST(test_a_connected_call_ends_with_its_length_when_we_hang_up);
  RUN_TEST(test_a_dial_after_the_ims_was_lost_is_a_failed_call_and_a_phantom_begin_is_no_call);
  RUN_TEST(test_a_begin_without_a_call_is_ignored);
  RUN_TEST(test_a_begin_before_atd_has_gone_out_is_ignored);
  RUN_TEST(test_a_restart_while_dialling_fails_the_call_and_starts_over);
  RUN_TEST(test_an_atd_that_is_never_answered_fails_after_the_limit_and_is_hung_up);
  RUN_TEST(test_an_atd_the_modem_refuses_is_a_refused_dial_with_its_line);
  RUN_TEST(test_a_dial_is_refused_for_a_number_that_is_not_a_dial_string_or_a_second_call);
  RUN_TEST(test_a_dial_is_refused_while_the_modem_is_not_ready_or_the_network_is_sms_only);
  RUN_TEST(test_busy_and_no_answer_each_end_the_call_with_one_event);
  RUN_TEST(test_a_refused_call_is_a_dial_failure_with_the_network_cause_and_the_lines_before);
  RUN_TEST(test_the_failed_call_is_reported_only_after_the_network_was_asked_why);
  RUN_TEST(test_end_and_no_carrier_together_are_one_failed_call);
  RUN_TEST(test_the_end_of_a_connected_call_gives_its_length_from_the_modem);
  RUN_TEST(test_the_length_is_measured_when_the_modem_does_not_say_it);
  RUN_TEST(test_a_call_that_vanishes_from_the_call_list_is_a_failed_dial);
  RUN_TEST(test_an_empty_call_list_before_the_call_appears_is_not_the_end);
  RUN_TEST(test_a_dial_that_is_cancelled_before_atd_goes_out_sends_nothing);
  RUN_TEST(test_a_dial_cancelled_while_the_ims_question_is_out_sends_no_atd);
  RUN_TEST(test_a_dial_cancelled_after_atd_is_hung_up_and_a_late_connection_is_ignored);
  RUN_TEST(test_a_new_dial_is_refused_while_the_cancelled_dial_is_still_being_set_up);
  RUN_TEST(test_an_incoming_call_is_announced_again_with_its_number_answered_and_ended);
  RUN_TEST(test_a_ring_that_repeats_is_one_incoming_call);
  RUN_TEST(test_a_caller_who_gives_up_is_a_missed_call_and_the_sound_path_goes_back);
  RUN_TEST(test_a_ringing_call_that_we_refuse_is_missed_and_hung_up);
  RUN_TEST(test_an_answer_that_the_modem_refuses_connects_nothing);
  RUN_TEST(test_a_ring_during_a_call_changes_nothing);
  RUN_TEST(test_busy_or_no_carrier_or_end_without_a_call_are_nothing);
  RUN_TEST(test_a_restart_in_a_connected_call_ends_it_once);
  RUN_TEST(test_a_restart_while_ringing_is_a_missed_call);
  RUN_TEST(test_the_volume_uses_coutgain_is_clamped_and_works_in_a_call);
  RUN_TEST(test_the_same_volume_is_not_sent_again);
  RUN_TEST(test_the_chosen_volume_comes_back_after_a_restart);
  RUN_TEST(test_no_log_line_of_a_call_carries_the_number);
  RUN_TEST(test_the_call_log_lines_carry_the_words_that_keep_them_on_the_right_level);
  RUN_TEST(test_the_sound_path_is_switched_exactly_when_a_call_begins_and_ends);
  return UNITY_END();
}
