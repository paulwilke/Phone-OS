// Host tests for the messages, sleep, switching off and questions of the A7682E driver: receiving by +CMTI
// and by reading the SIM, deleting only what was read, sending in the GSM alphabet and in UCS2, texts of
// several parts, failures reported once with the modem's reason, a restart in the middle, sleeping and waking,
// AT+CPOF and the diagnostic questions. The driver runs against a scripted modem; nothing here has run against
// the real modem.
// allow-non-english: texts with umlauts are the point of the UCS2 cases.

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
const std::string kOther = "+491702222222";

std::string ucs2(const std::string& text) {
  char hex[2048];
  TEST_ASSERT_TRUE(text::utf8ToUcs2Hex(text.c_str(), hex, sizeof(hex)) >= 0);
  return hex;
}

std::string cmgs(const std::string& number, bool ascii) { return "AT+CMGS=\"" + (ascii ? number : ucs2(number)) + "\""; }

struct Ready {
  Rig r;
  Script s;
  explicit Ready(void (*tweak)(Script&) = nullptr) {
    if (tweak) tweak(s);
    s.apply(r.t);
    TEST_ASSERT_TRUE(r.start());
    r.run(500);
    r.events();
  }
  std::vector<SmsMessage> received() {
    std::vector<SmsMessage> out;
    SmsMessage m;
    while (r.driver.pollSms(m)) out.push_back(m);
    return out;
  }
  std::vector<SmsFailure> failures() {
    std::vector<SmsFailure> out;
    SmsFailure f;
    while (r.driver.pollSendFailure(f)) out.push_back(f);
    return out;
  }
};

std::string header(int slot, const std::string& phone, const std::string& scts = "26/09/15,14:32:10+08") {
  return "+CMGL: " + std::to_string(slot) + ",\"REC UNREAD\",\"" + ucs2(phone) + "\",\"\",\"" + scts + "\"";
}

}  // namespace

// ---- receiving ------------------------------------------------------------------------------------------------------

void test_an_announced_message_is_read_and_then_deleted_from_the_sim() {
  Ready m;
  m.r.injectSms(3, kNumber, "Schule f\xC3\xA4llt aus \xF0\x9F\x98\x80");
  m.r.run(1000);
  const auto got = m.received();
  TEST_ASSERT_EQUAL(1, got.size());
  TEST_ASSERT_EQUAL_STRING(kNumber.c_str(), got[0].phone.c_str());
  TEST_ASSERT_EQUAL_STRING("Schule f\xC3\xA4llt aus \xF0\x9F\x98\x80", got[0].body.c_str());
  TEST_ASSERT_EQUAL_UINT32(1789475530u, got[0].timestamp);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CMGR=3"));
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CMGD=3"));
  TEST_ASSERT_TRUE(m.r.indexOf("AT+CMGR=3") < m.r.indexOf("AT+CMGD=3"));   // read first, then deleted
  TEST_ASSERT_TRUE(m.received().empty());
}

// What lay on the SIM while the device was away is read once the modem is ready: all of it first, then deleted.
void test_the_messages_on_the_sim_are_fetched_after_registration_and_deleted_after_reading() {
  Ready m([](Script& s) {
    s.inbox = {header(1, kNumber, "26/09/15,08:00:00+08"), ucs2("first"), header(2, kOther, "26/09/15,08:01:00+08"), ucs2("second")};
  });
  const auto got = m.received();
  TEST_ASSERT_EQUAL(2, got.size());
  TEST_ASSERT_EQUAL_STRING("first", got[0].body.c_str());
  TEST_ASSERT_EQUAL_STRING("second", got[1].body.c_str());
  TEST_ASSERT_EQUAL_STRING(kOther.c_str(), got[1].phone.c_str());
  TEST_ASSERT_TRUE(m.r.indexOf("AT+CMGL=\"ALL\"") < m.r.indexOf("AT+CMGD=1"));   // all read before any is deleted
  TEST_ASSERT_TRUE(m.r.indexOf("AT+CMGD=1") < m.r.indexOf("AT+CMGD=2"));
}

// More messages than one answer holds: the rest is fetched at once, and nothing is deleted that was not delivered.
void test_a_sim_with_more_messages_than_one_read_holds_is_read_again_until_empty() {
  Rig q;
  q.t.replyLines("AT+CMGL=\"ALL\"", {header(1, kNumber), ucs2("one"), header(2, kNumber), ucs2("two"), header(3, kNumber), ucs2("three")});
  q.t.replyLines("AT+CMGL=\"ALL\"", {header(3, kNumber), ucs2("three")});
  q.t.replyLines("AT+CMGL=\"ALL\"", {});
  Script().apply(q.t);
  TEST_ASSERT_TRUE(q.start());
  q.run(2000);
  SmsMessage msg;
  std::vector<std::string> bodies;
  while (q.driver.pollSms(msg)) bodies.push_back(msg.body);
  TEST_ASSERT_EQUAL(3, bodies.size());
  TEST_ASSERT_EQUAL_STRING("one", bodies[0].c_str());
  TEST_ASSERT_EQUAL_STRING("two", bodies[1].c_str());
  TEST_ASSERT_EQUAL_STRING("three", bodies[2].c_str());
  TEST_ASSERT_EQUAL(1, q.count("AT+CMGD=1"));
  TEST_ASSERT_EQUAL(1, q.count("AT+CMGD=2"));
  TEST_ASSERT_EQUAL(1, q.count("AT+CMGD=3"));
  TEST_ASSERT_EQUAL(0, q.t.unexpected().size());
}

// A text that reads "BUSY" is pure hex in UCS2 mode and no call event.
void test_a_message_that_reads_busy_is_no_busy_signal() {
  Ready m;
  m.r.injectSms(1, kNumber, "BUSY");
  m.r.run(1000);
  const auto got = m.received();
  TEST_ASSERT_EQUAL(1, got.size());
  TEST_ASSERT_EQUAL_STRING("BUSY", got[0].body.c_str());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_TRUE(m.r.events().empty());
}

// Several announcements in a row are all read, in order.
void test_several_announced_messages_are_read_in_the_order_announced() {
  Ready m;
  m.r.injectSms(4, kNumber, "four");
  m.r.injectSms(5, kOther, "five");
  m.r.injectSms(6, kNumber, "six");
  m.r.run(3000);
  const auto got = m.received();
  TEST_ASSERT_EQUAL(3, got.size());
  TEST_ASSERT_EQUAL_STRING("four", got[0].body.c_str());
  TEST_ASSERT_EQUAL_STRING("five", got[1].body.c_str());
  TEST_ASSERT_EQUAL_STRING("six", got[2].body.c_str());
  TEST_ASSERT_EQUAL(3, m.r.countPrefix("AT+CMGD="));
}

// A message that cannot be read stays on the SIM, and is not deleted unread; the next read of the SIM finds it.
void test_a_message_that_does_not_parse_stays_on_the_sim_and_is_read_with_the_next_read_of_it() {
  Ready m;
  m.r.t.reply("AT+CMGR=2", {});                         // the modem answers OK and no message
  m.r.urc("+CMTI: \"SM\",2", 1500);
  TEST_ASSERT_TRUE(m.received().empty());
  TEST_ASSERT_EQUAL(0, m.r.count("AT+CMGD=2"));
  TEST_ASSERT_TRUE(m.r.count("AT+CMGL=\"ALL\"") >= 2);  // the read of the whole SIM that follows
}

// The modem does not answer the read: the job times out, the driver goes on, and later sending works.
void test_a_read_that_is_never_answered_times_out_and_the_driver_goes_on() {
  Ready m;
  m.r.t.silence("AT+CMGR=4");
  m.r.urc("+CMTI: \"SM\",4", 100);
  m.r.run(15000);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "after"));
  m.r.run(3000);
  TEST_ASSERT_EQUAL(1, m.r.countPrefix(cmgs(kNumber, true)));
}

void test_a_message_arrives_during_a_call_and_the_call_is_untouched() {
  Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
  m.r.driver.dial(kNumber);
  m.r.run(1500);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  m.r.events();
  m.r.injectSms(7, kOther, "in the call");
  m.r.run(1000);
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::InCall);
  TEST_ASSERT_EQUAL(1, m.received().size());
  TEST_ASSERT_TRUE(m.r.events().empty());
}

// ---- sending -----------------------------------------------------------------------------------------------------------

void test_plain_text_goes_out_in_the_gsm_alphabet_and_the_charset_goes_back() {
  Ready m;
  const int before = m.r.count("AT+CSCS=\"UCS2\"");
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "I will be there at noon"));
  m.r.run(2000);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CSCS=\"GSM\""));
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CSMP=17,167,0,0"));
  TEST_ASSERT_EQUAL(1, m.r.count(cmgs(kNumber, true)));
  TEST_ASSERT_EQUAL(1, m.r.t.bodies().size());
  TEST_ASSERT_EQUAL_STRING("I will be there at noon", m.r.t.bodies()[0].c_str());
  TEST_ASSERT_EQUAL(before + 1, m.r.count("AT+CSCS=\"UCS2\""));          // back, or incoming umlauts would be hex
  TEST_ASSERT_TRUE(m.failures().empty());                                // sent: no failure
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
}

void test_text_with_an_umlaut_goes_out_as_ucs2_with_the_number_in_hex() {
  Ready m;
  const int csmp = m.r.count("AT+CSMP=17,167,0,8");                   // the initialisation sets it once
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "Gr\xC3\xBC\xC3\x9F" "e"));
  m.r.run(2000);
  TEST_ASSERT_EQUAL(csmp + 1, m.r.count("AT+CSMP=17,167,0,8"));
  TEST_ASSERT_EQUAL(1, m.r.count(cmgs(kNumber, false)));
  TEST_ASSERT_EQUAL(1, m.r.t.bodies().size());
  TEST_ASSERT_EQUAL_STRING(ucs2("Gr\xC3\xBC\xC3\x9F" "e").c_str(), m.r.t.bodies()[0].c_str());
}

// Refused at once and nothing sent: a number that is not a dial string must never be cut short ("0170 123" is not
// 0170), and the text must not contain what the modem takes for a command.
void test_sending_is_refused_for_what_cannot_go_out() {
  Ready m;
  TEST_ASSERT_FALSE(m.r.driver.sendSms("", "text"));
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, ""));
  TEST_ASSERT_FALSE(m.r.driver.sendSms("0170 123", "text"));
  TEST_ASSERT_FALSE(m.r.driver.sendSms("+49170111111111111111111111", "text"));
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, std::string(kMaxSmsBytes + 1, 'x')));
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, "bad \xC3 utf-8"));
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, "ends here\x1A and more"));   // Ctrl-Z would send it early
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, "escape\x1B"));
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, std::string(kMaxSmsBytes, 'x')));
  m.r.run(5000);
  TEST_ASSERT_EQUAL(0, m.r.count(cmgs("0170 123", true)));
  TEST_ASSERT_EQUAL(2, m.r.t.bodies().size());                                     // 320 characters: two parts, nothing else
}

void test_sending_is_refused_while_the_modem_is_not_ready() {
  Rig r;
  Script().apply(r.t);
  TEST_ASSERT_FALSE(r.driver.sendSms(kNumber, "off"));
  r.driver.begin();
  TEST_ASSERT_FALSE(r.driver.sendSms(kNumber, "starting"));
  A7682EDriver::Config c = Rig::defaultConfig();
  c.simPin.clear();
  Rig e(c);
  Script s;
  s.cpin = {{"+CPIN: SIM PIN", "OK"}};
  s.apply(e.t);
  e.driver.begin();
  TEST_ASSERT_TRUE(e.runTo(ModemState::Error));
  TEST_ASSERT_FALSE(e.driver.sendSms(kNumber, "error"));
}

void test_at_most_four_messages_wait_and_they_go_out_in_order() {
  Ready m;
  for (int i = 1; i <= 4; ++i) TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "message " + std::to_string(i)));
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, "message 5"));
  m.r.run(8000);
  TEST_ASSERT_EQUAL(4, m.r.t.bodies().size());
  for (size_t i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_STRING(("message " + std::to_string(i + 1)).c_str(), m.r.t.bodies()[i].c_str());
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "message 5"));       // room again
}

// A refused sending must reach the application, once, with the modem's reason: before, it vanished in the driver and
// the interface said "sent", an emergency message included (2026-10-01).
void test_a_refused_sending_is_reported_exactly_once_with_the_modems_reason_and_the_queue_goes_on() {
  Ready m([](Script& s) { s.smsFinal = "+CMS ERROR: 500"; });
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "first"));
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kOther, "second"));
  m.r.run(6000);
  TEST_ASSERT_EQUAL(2, m.r.countPrefix("AT+CMGS="));
  const auto f = m.failures();
  TEST_ASSERT_EQUAL(2, f.size());
  TEST_ASSERT_EQUAL_STRING(kNumber.c_str(), f[0].phone.c_str());
  TEST_ASSERT_EQUAL_STRING("+CMS ERROR: 500", f[0].reason.c_str());
  TEST_ASSERT_EQUAL_STRING(kOther.c_str(), f[1].phone.c_str());
  TEST_ASSERT_TRUE(m.failures().empty());                               // once
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL(2, m.r.count("AT+CSCS=\"UCS2\"") - 1);              // the charset went back after each
}

void test_an_error_at_the_recipient_is_a_failure_with_that_line() {
  Rig p;
  p.t.reply("AT+CMGS=\"+491701111111\"", {}, "+CMS ERROR: 304");
  Script().apply(p.t);
  TEST_ASSERT_TRUE(p.start());
  TEST_ASSERT_TRUE(p.driver.sendSms(kNumber, "hello"));
  p.run(3000);
  SmsFailure f;
  TEST_ASSERT_TRUE(p.driver.pollSendFailure(f));
  TEST_ASSERT_EQUAL_STRING("+CMS ERROR: 304", f.reason.c_str());
  TEST_ASSERT_EQUAL(0, p.t.bodies().size());
}

// ---- texts of several parts ------------------------------------------------------------------------------------------

void test_a_text_of_320_plain_characters_goes_out_as_two_messages_of_160() {
  Ready m;
  const std::string text(320, 'x');
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, text));
  m.r.run(5000);
  TEST_ASSERT_EQUAL(2, m.r.countPrefix("AT+CMGS="));
  TEST_ASSERT_EQUAL(2, m.r.t.bodies().size());
  TEST_ASSERT_EQUAL(160, m.r.t.bodies()[0].size());
  TEST_ASSERT_EQUAL(160, m.r.t.bodies()[1].size());
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CSCS=\"GSM\""));               // the charset is set once for the whole text
  TEST_ASSERT_TRUE(m.failures().empty());
}

// The cut is after a space where there is one, and nothing is lost or invented.
void test_the_parts_are_cut_after_a_space_and_together_are_the_text() {
  Ready m;
  std::string text;
  while (text.size() < 300) text += "wordy ";               // 6 characters: a cut at 160 would fall inside a word
  text.resize(299);
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, text));
  m.r.run(5000);
  TEST_ASSERT_EQUAL(2, m.r.t.bodies().size());
  TEST_ASSERT_TRUE(m.r.t.bodies()[0].size() <= 160);
  TEST_ASSERT_EQUAL(' ', m.r.t.bodies()[0].back());
  TEST_ASSERT_EQUAL_STRING(text.c_str(), (m.r.t.bodies()[0] + m.r.t.bodies()[1]).c_str());
}

void test_a_ucs2_text_is_cut_at_70_characters_and_never_inside_one() {
  Ready m;
  std::string text;
  for (int i = 0; i < 100; ++i) text += "\xC3\xA4";                  // 100 times a-umlaut: 200 bytes
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, text));
  m.r.run(5000);
  TEST_ASSERT_EQUAL(2, m.r.t.bodies().size());
  TEST_ASSERT_EQUAL_STRING(ucs2(text.substr(0, 140)).c_str(), m.r.t.bodies()[0].c_str());   // 70 characters
  TEST_ASSERT_EQUAL_STRING(ucs2(text.substr(140)).c_str(), m.r.t.bodies()[1].c_str());
}

// An emoji takes two places of the 70, and is never cut in the middle of its pair.
void test_an_emoji_counts_double_and_is_never_split() {
  Ready m;
  std::string text;
  for (int i = 0; i < 40; ++i) text += "\xF0\x9F\x98\x80";            // 40 emoji = 80 places
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, text));
  m.r.run(5000);
  TEST_ASSERT_EQUAL(2, m.r.t.bodies().size());
  TEST_ASSERT_EQUAL(35 * 8, m.r.t.bodies()[0].size());              // 35 emoji = 70 places = 280 hex digits
  TEST_ASSERT_EQUAL(5 * 8, m.r.t.bodies()[1].size());
}

// "{" is in the GSM alphabet only through its extension and takes two places.
void test_the_extension_characters_of_the_gsm_alphabet_take_two_places() {
  Ready m;
  const std::string text(100, '{');
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, text));
  m.r.run(5000);
  TEST_ASSERT_EQUAL(2, m.r.t.bodies().size());
  TEST_ASSERT_EQUAL(80, m.r.t.bodies()[0].size());
}

void test_a_failing_second_part_is_one_failure_that_names_the_part() {
  Ready m;
  m.r.t.replyToBody({"+CMGS: 12"});
  m.r.t.replyToBody({}, "+CMS ERROR: 331");
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, std::string(320, 'x')));
  m.r.run(5000);
  const auto f = m.failures();
  TEST_ASSERT_EQUAL(1, f.size());
  TEST_ASSERT_EQUAL_STRING("part 2 of 2: +CMS ERROR: 331", f[0].reason.c_str());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
}

// ---- when the modem does not answer ----------------------------------------------------------------------------------------

void test_a_sending_that_gets_no_prompt_fails_leaves_the_prompt_and_the_queue_goes_on() {
  Rig p;
  Script s;
  s.silent = cmgs(kNumber, true);
  s.apply(p.t);
  TEST_ASSERT_TRUE(p.start());
  TEST_ASSERT_TRUE(p.driver.sendSms(kNumber, "first"));
  TEST_ASSERT_TRUE(p.driver.sendSms(kOther, "second"));
  p.run(12000);
  SmsFailure f;
  TEST_ASSERT_TRUE(p.driver.pollSendFailure(f));
  TEST_ASSERT_EQUAL(1, p.t.escapes());                                 // the prompt is left with Escape
  TEST_ASSERT_EQUAL_STRING("no prompt from the modem", f.reason.c_str());
  TEST_ASSERT_EQUAL_STRING(kNumber.c_str(), f.phone.c_str());
  TEST_ASSERT_EQUAL(1, p.t.bodies().size());
  TEST_ASSERT_EQUAL_STRING("second", p.t.bodies()[0].c_str());        // the next message was not held up
  TEST_ASSERT_FALSE(p.driver.pollSendFailure(f));
}

// A sending whose result never comes: after the limit it is a failure (it may or may not have gone out), once.
void test_a_sending_whose_result_never_comes_fails_after_the_limit() {
  Rig r;
  Script s;
  s.apply(r.t);
  r.t.setDefaultBodyAnswer({}, "");                      // the text is taken, and nothing follows
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.driver.sendSms(kNumber, "hello"));
  r.run(50000);
  SmsFailure f;
  TEST_ASSERT_FALSE(r.driver.pollSendFailure(f));        // inside the 60 seconds
  r.run(15000);
  TEST_ASSERT_TRUE(r.driver.pollSendFailure(f));
  TEST_ASSERT_EQUAL_STRING("no answer from the modem", f.reason.c_str());
  TEST_ASSERT_FALSE(r.driver.pollSendFailure(f));
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Ready);
}

// ---- a restart --------------------------------------------------------------------------------------------------------------

// A message that was in the middle of being sent is reported (it may have gone out, so it is not sent again).
void test_a_restart_while_sending_reports_that_message_once_and_does_not_send_it_again() {
  Ready m;
  m.r.t.setDefaultBodyAnswer({}, "");                    // the result never comes ...
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "in flight"));
  m.r.run(2000);
  TEST_ASSERT_EQUAL(1, m.r.t.bodies().size());
  m.r.t.setDefaultBodyAnswer({"+CMGS: 12"});
  m.r.t.restart();                                       // ... the modem restarts instead
  TEST_ASSERT_TRUE(m.r.runTo(ModemState::PoweringOn));
  TEST_ASSERT_TRUE(m.r.runTo(ModemState::Ready));
  m.r.run(3000);
  const auto f = m.failures();
  TEST_ASSERT_EQUAL(1, f.size());
  TEST_ASSERT_EQUAL_STRING("modem restarted while sending", f[0].reason.c_str());
  TEST_ASSERT_EQUAL(1, m.r.t.bodies().size());           // not sent a second time
}

// What was only queued is not lost by the restart, and the radio cycle of the data bearer runs before it goes out.
void test_a_message_queued_before_a_restart_goes_out_after_the_radio_cycle() {
  Rig r;
  Script s;
  s.contexts = {};                                       // the modem lost context 1: the bearer is written again
  s.apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  const int cyclesBefore = r.count("AT+CFUN=1");
  TEST_ASSERT_TRUE(r.driver.sendSms(kNumber, "queued"));
  r.t.restart();                                         // before the job has begun
  TEST_ASSERT_TRUE(r.runTo(ModemState::PoweringOn));
  TEST_ASSERT_TRUE(r.runTo(ModemState::Ready));
  r.run(3000);
  TEST_ASSERT_EQUAL(cyclesBefore + 1, r.count("AT+CFUN=1"));
  TEST_ASSERT_EQUAL(1, r.t.bodies().size());
  TEST_ASSERT_EQUAL_STRING("queued", r.t.bodies()[0].c_str());
  // The second CFUN=1 stands before the sending: the cycle would have dropped a message in flight.
  int lastCfun = -1;
  for (size_t i = 0; i < r.t.sent().size(); ++i)
    if (r.t.sent()[i] == "AT+CFUN=1") lastCfun = (int)i;
  TEST_ASSERT_TRUE(lastCfun >= 0 && lastCfun < r.indexOf(cmgs(kNumber, true)));
  SmsFailure f;
  TEST_ASSERT_FALSE(r.driver.pollSendFailure(f));
}

// ---- sleeping and waking --------------------------------------------------------------------------------------------------------

void test_sleeping_keeps_the_state_stops_the_periodic_questions_and_waking_brings_them_back() {
  Ready m;
  m.r.driver.sleep();
  m.r.run(500);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CSCLK=1"));
  TEST_ASSERT_TRUE(m.r.t.dtr());                         // high: the modem may sleep
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  const int csq = m.r.count("AT+CSQ");
  m.r.run(95000);
  TEST_ASSERT_EQUAL(csq, m.r.count("AT+CSQ"));           // nothing is asked of a sleeping modem
  m.r.driver.wake();
  m.r.run(500);
  TEST_ASSERT_FALSE(m.r.t.dtr());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
  m.r.run(2000);
  TEST_ASSERT_TRUE(m.r.count("AT+CSQ") > csq);           // and the network is asked again after waking
}

void test_a_message_to_send_wakes_the_modem_first() {
  Ready m;
  m.r.driver.sleep();
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.t.dtr());
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "hello"));
  m.r.run(3000);
  TEST_ASSERT_FALSE(m.r.t.dtr());                        // woken to send
  TEST_ASSERT_EQUAL(1, m.r.t.bodies().size());
}

void test_an_announced_message_wakes_the_modem_and_is_not_lost() {
  Ready m;
  m.r.driver.sleep();
  m.r.run(500);
  m.r.injectSms(2, kNumber, "while asleep");
  m.r.run(2000);
  TEST_ASSERT_FALSE(m.r.t.dtr());
  const auto got = m.received();
  TEST_ASSERT_EQUAL(1, got.size());
  TEST_ASSERT_EQUAL_STRING("while asleep", got[0].body.c_str());
}

void test_a_call_wakes_the_modem_before_it_is_dialled_or_answered() {
  Ready m;
  m.r.driver.sleep();
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.t.dtr());
  TEST_ASSERT_TRUE(m.r.driver.dial(kNumber));
  m.r.run(800);
  TEST_ASSERT_FALSE(m.r.t.dtr());
  TEST_ASSERT_EQUAL(1, m.r.count("ATD" + kNumber + ";"));
  m.r.driver.hangup();
  m.r.run(500);
  m.r.driver.sleep();
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.t.dtr());
  m.r.urc("RING");                                       // an incoming call: the modem woke itself, the driver wakes the line
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::RingingIn);
  TEST_ASSERT_TRUE(m.r.driver.answer());
  m.r.run(500);
  TEST_ASSERT_FALSE(m.r.t.dtr());
  TEST_ASSERT_EQUAL(1, m.r.count("ATA"));
}

void test_the_modem_is_not_put_to_sleep_with_work_waiting_or_in_a_call() {
  Ready m;
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "first"));
  m.r.driver.sleep();
  m.r.run(3000);
  TEST_ASSERT_EQUAL(1, m.r.t.bodies().size());           // sent, not slept over
  TEST_ASSERT_EQUAL(0, m.r.count("AT+CSCLK=1"));
  m.r.urc("RING");
  m.r.driver.sleep();
  m.r.run(2000);
  TEST_ASSERT_EQUAL(0, m.r.count("AT+CSCLK=1"));         // not in a call
  TEST_ASSERT_FALSE(m.r.t.dtr());
}

// ---- switching off --------------------------------------------------------------------------------------------------------------------

void test_shutdown_sends_cpof_cuts_the_supply_and_the_modem_can_be_started_again() {
  Ready m;
  m.r.driver.shutdown();
  m.r.run(1000);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CPOF"));
  TEST_ASSERT_FALSE(m.r.t.powered());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Off);
  TEST_ASSERT_EQUAL(0, m.r.driver.signalBars());
  TEST_ASSERT_EQUAL_STRING("", m.r.driver.operatorName().c_str());
  TEST_ASSERT_TRUE(m.r.driver.voiceStatus() == VoiceStatus::Unknown);
  TEST_ASSERT_TRUE(m.r.start());
  TEST_ASSERT_TRUE(m.r.t.powered());
}

void test_shutdown_ends_a_call_as_the_capability_says() {
  {
    Ready m([](Script& s) { s.clcc = {"+CLCC: 1,0,0,0,0,\"+491701111111\",145"}; });
    m.r.driver.dial(kNumber);
    m.r.run(1500);
    m.r.events();
    m.r.run(5000);
    m.r.driver.shutdown();
    const auto got = m.r.events();
    TEST_ASSERT_EQUAL(1, got.size());
    TEST_ASSERT_TRUE(got[0].type == CallEventType::Ended);
    TEST_ASSERT_TRUE(got[0].duration >= 4);
    m.r.run(1000);
    TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Off);
    TEST_ASSERT_FALSE(m.r.t.audioToModem());
  }
  {
    Ready m;
    m.r.urc("RING");
    m.r.events();
    m.r.driver.shutdown();
    const auto got = m.r.events();
    TEST_ASSERT_EQUAL(2, got.size());                       // Incoming (the number never came), then Missed
    TEST_ASSERT_TRUE(got[1].type == CallEventType::Missed);
  }
  {
    Ready m;
    m.r.driver.dial(kNumber);
    m.r.run(300);
    m.r.driver.shutdown();
    const auto got = m.r.events();
    TEST_ASSERT_EQUAL(1, got.size());
    TEST_ASSERT_TRUE(got[0].type == CallEventType::Ended);
    TEST_ASSERT_EQUAL_UINT32(0, got[0].duration);
  }
}

void test_shutdown_reports_what_was_queued_and_not_sent_and_refuses_new_work() {
  Ready m;
  TEST_ASSERT_TRUE(m.r.driver.sendSms(kNumber, "never sent"));
  m.r.driver.shutdown();
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, "too late"));
  TEST_ASSERT_FALSE(m.r.driver.dial(kNumber));
  TEST_ASSERT_FALSE(m.r.driver.query("AT+CSCLK?"));
  const auto f = m.failures();
  TEST_ASSERT_EQUAL(1, f.size());
  TEST_ASSERT_EQUAL_STRING("modem switched off", f[0].reason.c_str());
  m.r.run(1000);
  TEST_ASSERT_EQUAL(0, m.r.t.bodies().size());
  TEST_ASSERT_FALSE(m.r.driver.sendSms(kNumber, "off"));
  TEST_ASSERT_FALSE(m.r.driver.dial(kNumber));
}

// A modem that does not answer AT+CPOF is cut from its supply all the same.
void test_a_silent_cpof_does_not_keep_the_supply_on() {
  Ready m;
  m.r.t.silence("AT+CPOF");
  m.r.driver.shutdown();
  m.r.run(8000);
  TEST_ASSERT_FALSE(m.r.t.powered());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Off);
}

void test_shutdown_twice_or_when_off_changes_nothing() {
  Rig r;
  Script().apply(r.t);
  r.driver.shutdown();
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Off);
  TEST_ASSERT_EQUAL(0, r.t.sent().size());
  Ready m;
  m.r.driver.shutdown();
  m.r.driver.shutdown();
  m.r.run(2000);
  TEST_ASSERT_EQUAL(1, m.r.count("AT+CPOF"));
}

// ---- questions -------------------------------------------------------------------------------------------------------------------------

// Only reading passes this way: nothing that sets, executes or chains commands.
void test_only_pure_questions_are_accepted() {
  for (const char* good : {"AT+CPSMS?", "AT+CEDRXS=?", "AT+CFUN=?", "AT+CSCLK?", "AT+CMGF?"})
    TEST_ASSERT_TRUE_MESSAGE(A7682EDriver::isQuery(good), good);
  for (const char* bad : {"", "AT", "AT+", "AT+CFUN=0", "AT+CFUN=1?", "ATD+491701111111;", "AT+CPOF?;AT+CPOF", "AT+CMGS=\"+491701111111\"?",
                          "AT+CPOF;AT+CSCLK?", "AT?", "at+csclk?", "+CSCLK?", "AT+CSCLK?\r\nAT+CPOF", "AT+CGMR", "AT+CSCLK=1", "AT+C SCLK?",
                          "AT+CSCLKCSCLKCSCLKCSCLKCSCLKCSCLKCSCLKCSCLKCSCLK?"})
    TEST_ASSERT_FALSE_MESSAGE(A7682EDriver::isQuery(bad), bad);
}

void test_a_question_is_asked_and_its_answer_comes_once() {
  Ready m;
  m.r.t.reply("AT+CPSMS?", {"+CPSMS: 0"});
  TEST_ASSERT_FALSE(m.r.driver.query("AT+CFUN=0"));
  TEST_ASSERT_EQUAL(0, m.r.count("AT+CFUN=0"));
  TEST_ASSERT_TRUE(m.r.driver.query("AT+CPSMS?"));
  TEST_ASSERT_FALSE(m.r.driver.query("AT+CSCLK?"));          // one at a time
  m.r.run(500);
  std::string answer;
  TEST_ASSERT_TRUE(m.r.driver.pollQueryResult(answer));
  TEST_ASSERT_EQUAL_STRING("+CPSMS: 0", answer.c_str());
  TEST_ASSERT_FALSE(m.r.driver.pollQueryResult(answer));
}

void test_the_answer_lines_are_joined_and_an_error_says_so() {
  Ready m;
  m.r.t.reply("AT+CEDRXS=?", {"+CEDRXS: (0,1,2,3)", "+CEDRXS: 5,(0-15)"});
  m.r.t.reply("AT+CSCLK?", {}, "ERROR");
  std::string answer;
  m.r.driver.query("AT+CEDRXS=?");
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.driver.pollQueryResult(answer));
  TEST_ASSERT_EQUAL_STRING("+CEDRXS: (0,1,2,3)|+CEDRXS: 5,(0-15)", answer.c_str());
  m.r.driver.query("AT+CSCLK?");
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.driver.pollQueryResult(answer));
  TEST_ASSERT_EQUAL_STRING("ERROR", answer.c_str());
}

// The lines the driver reads for itself (a +CREG: line) are part of a question about them as well.
void test_a_question_about_what_the_driver_reads_itself_still_gets_its_answer() {
  Ready m;
  std::string answer;
  m.r.driver.query("AT+CREG?");
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.driver.pollQueryResult(answer));
  TEST_ASSERT_EQUAL_STRING("+CREG: 0,1", answer.c_str());
  TEST_ASSERT_TRUE(m.r.driver.state() == ModemState::Ready);
}

void test_a_question_wakes_a_sleeping_modem() {
  Ready m;
  m.r.driver.sleep();
  m.r.run(500);
  TEST_ASSERT_TRUE(m.r.t.dtr());
  m.r.t.reply("AT+CPSMS?", {"+CPSMS: 0"});
  m.r.driver.query("AT+CPSMS?");
  m.r.run(1000);
  TEST_ASSERT_FALSE(m.r.t.dtr());
  std::string answer;
  TEST_ASSERT_TRUE(m.r.driver.pollQueryResult(answer));
}

// ---- the log ---------------------------------------------------------------------------------------------------------------------------

// A log line may say that a message went wrong and why, never to whom and never what it said.
void test_no_log_line_carries_a_number_or_the_text_of_a_message() {
  Ready m([](Script& s) { s.smsFinal = "+CMS ERROR: 500"; });
  m.r.driver.sendSms(kNumber, "secret words");
  m.r.injectSms(2, kOther, "private text");
  m.r.run(4000);
  TEST_ASSERT_TRUE(m.r.logged("sending failed: +CMS ERROR: 500"));
  for (const std::string& l : m.r.logs)                       // a failed sending survives the errors level
    if (l.find("sending failed") != std::string::npos) TEST_ASSERT_TRUE(loglevel::classify(l.c_str()) <= loglevel::Importance::Error);
  for (const std::string& l : m.r.logs) {
    TEST_ASSERT_TRUE_MESSAGE(l.find("1701111111") == std::string::npos, l.c_str());
    TEST_ASSERT_TRUE_MESSAGE(l.find("1702222222") == std::string::npos, l.c_str());
    TEST_ASSERT_TRUE_MESSAGE(l.find("secret") == std::string::npos, l.c_str());
    TEST_ASSERT_TRUE_MESSAGE(l.find("private") == std::string::npos, l.c_str());
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_an_announced_message_is_read_and_then_deleted_from_the_sim);
  RUN_TEST(test_the_messages_on_the_sim_are_fetched_after_registration_and_deleted_after_reading);
  RUN_TEST(test_a_sim_with_more_messages_than_one_read_holds_is_read_again_until_empty);
  RUN_TEST(test_a_message_that_reads_busy_is_no_busy_signal);
  RUN_TEST(test_several_announced_messages_are_read_in_the_order_announced);
  RUN_TEST(test_a_message_that_does_not_parse_stays_on_the_sim_and_is_read_with_the_next_read_of_it);
  RUN_TEST(test_a_read_that_is_never_answered_times_out_and_the_driver_goes_on);
  RUN_TEST(test_a_message_arrives_during_a_call_and_the_call_is_untouched);
  RUN_TEST(test_plain_text_goes_out_in_the_gsm_alphabet_and_the_charset_goes_back);
  RUN_TEST(test_text_with_an_umlaut_goes_out_as_ucs2_with_the_number_in_hex);
  RUN_TEST(test_sending_is_refused_for_what_cannot_go_out);
  RUN_TEST(test_sending_is_refused_while_the_modem_is_not_ready);
  RUN_TEST(test_at_most_four_messages_wait_and_they_go_out_in_order);
  RUN_TEST(test_a_refused_sending_is_reported_exactly_once_with_the_modems_reason_and_the_queue_goes_on);
  RUN_TEST(test_an_error_at_the_recipient_is_a_failure_with_that_line);
  RUN_TEST(test_a_text_of_320_plain_characters_goes_out_as_two_messages_of_160);
  RUN_TEST(test_the_parts_are_cut_after_a_space_and_together_are_the_text);
  RUN_TEST(test_a_ucs2_text_is_cut_at_70_characters_and_never_inside_one);
  RUN_TEST(test_an_emoji_counts_double_and_is_never_split);
  RUN_TEST(test_the_extension_characters_of_the_gsm_alphabet_take_two_places);
  RUN_TEST(test_a_failing_second_part_is_one_failure_that_names_the_part);
  RUN_TEST(test_a_sending_that_gets_no_prompt_fails_leaves_the_prompt_and_the_queue_goes_on);
  RUN_TEST(test_a_sending_whose_result_never_comes_fails_after_the_limit);
  RUN_TEST(test_a_restart_while_sending_reports_that_message_once_and_does_not_send_it_again);
  RUN_TEST(test_a_message_queued_before_a_restart_goes_out_after_the_radio_cycle);
  RUN_TEST(test_sleeping_keeps_the_state_stops_the_periodic_questions_and_waking_brings_them_back);
  RUN_TEST(test_a_message_to_send_wakes_the_modem_first);
  RUN_TEST(test_an_announced_message_wakes_the_modem_and_is_not_lost);
  RUN_TEST(test_a_call_wakes_the_modem_before_it_is_dialled_or_answered);
  RUN_TEST(test_the_modem_is_not_put_to_sleep_with_work_waiting_or_in_a_call);
  RUN_TEST(test_shutdown_sends_cpof_cuts_the_supply_and_the_modem_can_be_started_again);
  RUN_TEST(test_shutdown_ends_a_call_as_the_capability_says);
  RUN_TEST(test_shutdown_reports_what_was_queued_and_not_sent_and_refuses_new_work);
  RUN_TEST(test_a_silent_cpof_does_not_keep_the_supply_on);
  RUN_TEST(test_shutdown_twice_or_when_off_changes_nothing);
  RUN_TEST(test_only_pure_questions_are_accepted);
  RUN_TEST(test_a_question_is_asked_and_its_answer_comes_once);
  RUN_TEST(test_the_answer_lines_are_joined_and_an_error_says_so);
  RUN_TEST(test_a_question_about_what_the_driver_reads_itself_still_gets_its_answer);
  RUN_TEST(test_a_question_wakes_a_sleeping_modem);
  RUN_TEST(test_no_log_line_carries_a_number_or_the_text_of_a_message);
  return UNITY_END();
}
