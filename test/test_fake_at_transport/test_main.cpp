// Host tests for FakeAtTransport, the scripted modem the driver tests run against. A fake that
// answers wrongly would make every driver test wrong, so its scripting surface is pinned here.

#include <unity.h>

#include <string>

#include <drivers/AtParser.h>
#include <fakes/FakeAtTransport.h>

void setUp() {}
void tearDown() {}

namespace {

void send(FakeAtTransport& t, const std::string& command) {
  const std::string line = command + "\r";
  t.write(line.data(), line.size());
}

// Everything that can be read now.
std::string readAll(FakeAtTransport& t) {
  std::string out;
  for (int c = t.read(); c >= 0; c = t.read()) out += (char)c;
  return out;
}

}  // namespace

void test_a_scripted_command_gets_its_lines_and_the_final_line_framed_like_the_modem() {
  FakeAtTransport t;
  t.reply("AT+CPIN?", {"+CPIN: READY"});
  send(t, "AT+CPIN?");
  TEST_ASSERT_EQUAL_STRING("\r\n+CPIN: READY\r\n\r\nOK\r\n", readAll(t).c_str());
  TEST_ASSERT_EQUAL(1, t.sent().size());
  TEST_ASSERT_EQUAL_STRING("AT+CPIN?", t.sent()[0].c_str());
}

void test_a_final_line_can_be_an_error_or_missing() {
  FakeAtTransport t;
  t.reply("AT+CPIN?", {}, "+CME ERROR: SIM failure");
  t.reply("AT+CSQ", {"+CSQ: 20,99"}, "");
  send(t, "AT+CPIN?");
  TEST_ASSERT_EQUAL_STRING("\r\n+CME ERROR: SIM failure\r\n", readAll(t).c_str());
  send(t, "AT+CSQ");
  TEST_ASSERT_EQUAL_STRING("\r\n+CSQ: 20,99\r\n", readAll(t).c_str());
}

void test_silence_gives_no_answer_at_all() {
  FakeAtTransport t;
  t.silence("AT+CFUN=1");
  send(t, "AT+CFUN=1");
  TEST_ASSERT_EQUAL(-1, t.read());
  t.advance(60000);
  TEST_ASSERT_EQUAL(-1, t.read());
  TEST_ASSERT_EQUAL(1, t.sent().size());
  TEST_ASSERT_EQUAL(0, t.unexpected().size());
}

// Replies to one command are used in turn and the last one stays, so a poll keeps getting an answer.
void test_replies_to_the_same_command_come_in_turn_and_the_last_one_stays() {
  FakeAtTransport t;
  t.reply("AT+CIREG?", {"+CIREG: 1,0,0"});
  t.reply("AT+CIREG?", {"+CIREG: 1,1,15"});
  send(t, "AT+CIREG?");
  TEST_ASSERT_TRUE(readAll(t).find("1,0,0") != std::string::npos);
  send(t, "AT+CIREG?");
  TEST_ASSERT_TRUE(readAll(t).find("1,1,15") != std::string::npos);
  send(t, "AT+CIREG?");
  TEST_ASSERT_TRUE(readAll(t).find("1,1,15") != std::string::npos);
}

void test_a_command_nobody_scripted_is_an_error_and_is_recorded() {
  FakeAtTransport t;
  send(t, "AT+CFUN=0");
  TEST_ASSERT_EQUAL_STRING("\r\nERROR\r\n", readAll(t).c_str());
  TEST_ASSERT_EQUAL(1, t.unexpected().size());
  TEST_ASSERT_EQUAL_STRING("AT+CFUN=0", t.unexpected()[0].c_str());
}

void test_a_command_may_arrive_in_pieces_and_with_either_line_end() {
  FakeAtTransport t;
  t.reply("AT", {});
  t.write("A", 1);
  TEST_ASSERT_EQUAL(0, t.sent().size());
  t.write("T\r\n", 3);
  TEST_ASSERT_EQUAL(1, t.sent().size());      // CR LF is one command, not two
  TEST_ASSERT_EQUAL_STRING("AT", t.sent()[0].c_str());
  t.write("\n\n", 2);
  TEST_ASSERT_EQUAL(1, t.sent().size());      // empty lines are nothing
}

void test_echo_repeats_the_command_until_ate0() {
  FakeAtTransport t;
  t.reply("ATE0", {});
  t.reply("AT", {});
  t.setEcho(true);
  send(t, "AT");
  TEST_ASSERT_EQUAL_STRING("AT\r\n\r\nOK\r\n", readAll(t).c_str());
  send(t, "ATE0");
  TEST_ASSERT_EQUAL_STRING("ATE0\r\n\r\nOK\r\n", readAll(t).c_str());
  send(t, "AT");
  TEST_ASSERT_EQUAL_STRING("\r\nOK\r\n", readAll(t).c_str());
}

void test_an_unsolicited_line_comes_at_its_time_and_not_before() {
  FakeAtTransport t;
  t.unsolicited(5000, "RING");
  TEST_ASSERT_EQUAL(-1, t.read());
  t.advance(4999);
  TEST_ASSERT_EQUAL(-1, t.read());
  t.advance(1);
  TEST_ASSERT_EQUAL_STRING("\r\nRING\r\n", readAll(t).c_str());
  TEST_ASSERT_EQUAL(-1, t.read());
}

void test_an_answer_can_be_late() {
  FakeAtTransport t;
  t.replyAfter(7000, "AT+COPS=?", {"+COPS: (1,\"X\",\"X\",\"26201\",7)"});
  send(t, "AT+COPS=?");
  TEST_ASSERT_EQUAL(-1, t.read());
  t.advance(6999);
  TEST_ASSERT_EQUAL(-1, t.read());
  t.advance(1);
  TEST_ASSERT_TRUE(readAll(t).find("26201") != std::string::npos);
}

void test_a_prompt_takes_the_text_up_to_ctrl_z_and_then_answers() {
  FakeAtTransport t;
  t.replyPrompt("AT+CMGS=\"+491701111111\"");
  t.replyToBody({"+CMGS: 45"});
  send(t, "AT+CMGS=\"+491701111111\"");
  TEST_ASSERT_EQUAL_STRING("\r\n> ", readAll(t).c_str());
  t.write("Hello\r\nline two", 15);          // a line end inside the text is text, not a command
  TEST_ASSERT_EQUAL(1, t.sent().size());
  TEST_ASSERT_EQUAL(-1, t.read());
  const char z = 0x1A;
  t.write(&z, 1);
  TEST_ASSERT_EQUAL_STRING("\r\n+CMGS: 45\r\n\r\nOK\r\n", readAll(t).c_str());
  TEST_ASSERT_EQUAL(1, t.bodies().size());
  TEST_ASSERT_EQUAL_STRING("Hello\r\nline two", t.bodies()[0].c_str());
  send(t, "AT");                             // back to commands
  TEST_ASSERT_EQUAL(1, t.unexpected().size());
}

void test_escape_cancels_the_text_without_an_answer() {
  FakeAtTransport t;
  t.replyPrompt("AT+CMGS=\"+491701111111\"");
  t.replyToBody({});
  send(t, "AT+CMGS=\"+491701111111\"");
  readAll(t);
  t.write("abc", 3);
  const char esc = 0x1B;
  t.write(&esc, 1);
  TEST_ASSERT_EQUAL(-1, t.read());
  TEST_ASSERT_EQUAL(0, t.bodies().size());
}

void test_a_restart_loses_what_was_on_its_way_and_announces_itself() {
  FakeAtTransport t;
  t.replyAfter(1000, "ATD+491701111111;", {});
  t.unsolicited(500, "VOICE CALL: BEGIN");
  send(t, "ATD+491701111111;");
  t.write("AT+C", 4);                         // half a command
  t.restart();
  t.advance(2000);
  TEST_ASSERT_EQUAL_STRING("\r\n*ATREADY: 1\r\n", readAll(t).c_str());
  t.write("SQ\r", 3);                         // the rest of the old half command is a different, unknown command
  TEST_ASSERT_EQUAL(1, t.unexpected().size());
  TEST_ASSERT_EQUAL_STRING("SQ", t.unexpected()[0].c_str());
}

void test_the_clock_starts_anywhere_and_wraps() {
  FakeAtTransport t;
  t.setNow(0xFFFFFFF0u);
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFF0u, t.now());
  t.unsolicited(0x00000010u, "RING");         // 32 ms later, after the wrap
  t.advance(31);
  TEST_ASSERT_EQUAL(-1, t.read());
  t.advance(1);
  TEST_ASSERT_EQUAL_STRING("\r\nRING\r\n", readAll(t).c_str());
}

void test_a_cold_modem_ignores_everything_until_the_power_key_wakes_it() {
  FakeAtTransport t;
  t.reply("AT", {});
  t.setCold(true);
  send(t, "AT");
  TEST_ASSERT_EQUAL(-1, t.read());
  TEST_ASSERT_EQUAL(1, t.sent().size());      // it was sent, nobody answered
  TEST_ASSERT_EQUAL(0, t.unexpected().size());
  t.pwrkeyPulse();
  TEST_ASSERT_EQUAL(1, t.pwrkeyPulses());
  TEST_ASSERT_EQUAL_STRING("\r\n*ATREADY: 1\r\n", readAll(t).c_str());   // the boot
  send(t, "AT");
  TEST_ASSERT_EQUAL_STRING("\r\nOK\r\n", readAll(t).c_str());
}

// What the driver must never do to a running modem.
void test_the_power_key_switches_a_running_modem_off() {
  FakeAtTransport t;
  t.reply("AT", {});
  t.pwrkeyPulse();
  send(t, "AT");
  TEST_ASSERT_EQUAL(-1, t.read());
  t.pwrkeyPulse();                            // and on again
  readAll(t);
  send(t, "AT");
  TEST_ASSERT_EQUAL_STRING("\r\nOK\r\n", readAll(t).c_str());
}

void test_the_sound_path_is_recorded() {
  FakeAtTransport t;
  TEST_ASSERT_FALSE(t.audioToModem());
  TEST_ASSERT_EQUAL(0, t.audioSwitches());
  t.setAudioToModem(true);
  TEST_ASSERT_TRUE(t.audioToModem());
  t.setAudioToModem(false);
  TEST_ASSERT_FALSE(t.audioToModem());
  TEST_ASSERT_EQUAL(2, t.audioSwitches());
}

void test_a_modem_without_supply_does_not_answer() {
  FakeAtTransport t;
  t.reply("AT", {});
  TEST_ASSERT_TRUE(t.powered());              // a bench modem is on until the driver says otherwise
  t.power(false);
  TEST_ASSERT_FALSE(t.powered());
  send(t, "AT");
  TEST_ASSERT_EQUAL(-1, t.read());
  t.power(true);
  send(t, "AT");
  TEST_ASSERT_EQUAL_STRING("\r\nOK\r\n", readAll(t).c_str());
}

// The fake and the parser together: a stored message read through the byte protocol.
void test_a_scripted_inbox_is_read_through_the_line_reader_and_the_sms_parser() {
  FakeAtTransport t;
  t.reply("AT+CMGL=\"REC UNREAD\"", {"+CMGL: 2,\"REC UNREAD\",\"+491701111111\",\"\",\"26/09/15,14:32:10+08\"", "On my way"});
  send(t, "AT+CMGL=\"REC UNREAD\"");
  at::ParsedSms out[2];
  at::SmsResponseParser parser(at::Charset::Gsm, out, 2);
  at::LineReader reader;
  for (int c = t.read(); c >= 0; c = t.read())
    if (reader.feed((char)c)) parser.feedLine(reader.line());
  TEST_ASSERT_TRUE(parser.ok());
  TEST_ASSERT_EQUAL(1, parser.count());
  TEST_ASSERT_EQUAL(2, out[0].index);
  TEST_ASSERT_EQUAL_STRING("On my way", out[0].msg.body.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_scripted_command_gets_its_lines_and_the_final_line_framed_like_the_modem);
  RUN_TEST(test_a_final_line_can_be_an_error_or_missing);
  RUN_TEST(test_silence_gives_no_answer_at_all);
  RUN_TEST(test_replies_to_the_same_command_come_in_turn_and_the_last_one_stays);
  RUN_TEST(test_a_command_nobody_scripted_is_an_error_and_is_recorded);
  RUN_TEST(test_a_command_may_arrive_in_pieces_and_with_either_line_end);
  RUN_TEST(test_echo_repeats_the_command_until_ate0);
  RUN_TEST(test_an_unsolicited_line_comes_at_its_time_and_not_before);
  RUN_TEST(test_an_answer_can_be_late);
  RUN_TEST(test_a_prompt_takes_the_text_up_to_ctrl_z_and_then_answers);
  RUN_TEST(test_escape_cancels_the_text_without_an_answer);
  RUN_TEST(test_a_restart_loses_what_was_on_its_way_and_announces_itself);
  RUN_TEST(test_the_clock_starts_anywhere_and_wraps);
  RUN_TEST(test_a_cold_modem_ignores_everything_until_the_power_key_wakes_it);
  RUN_TEST(test_the_power_key_switches_a_running_modem_off);
  RUN_TEST(test_the_sound_path_is_recorded);
  RUN_TEST(test_a_modem_without_supply_does_not_answer);
  RUN_TEST(test_a_scripted_inbox_is_read_through_the_line_reader_and_the_sms_parser);
  return UNITY_END();
}
