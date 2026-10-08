// Host tests for the AT parser of the modem driver: lines out of the byte stream, what a line is,
// the answers the driver asks for, and the SMS answers. The answers are built after 3GPP TS 27.005
// and 27.007 and the formats the previous driver met on the device; real captures belong here as
// further cases as soon as there are any.
// allow-non-english: texts with umlauts are the point of the UCS2 cases.

#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <drivers/AtParser.h>
#include <drivers/Ucs2.h>

using namespace at;

namespace {

// Sends a raw stream byte by byte through the line reader and the parser, as the driver does with
// the UART.
struct Feeder {
  LineReader reader;
  std::string unsolicited;  // the lines the parser handed back, separated by '|'

  void run(SmsResponseParser& parser, const char* stream) {
    for (const char* p = stream; *p; ++p) {
      if (!reader.feed(*p)) continue;
      if (parser.feedLine(reader.line()) == Feed::Unsolicited) {
        unsolicited += reader.line();
        unsolicited += '|';
      }
    }
  }
};

std::string cmglHeader(int index, const char* phone, const char* scts) {
  char buf[160];
  snprintf(buf, sizeof(buf), "+CMGL: %d,\"REC UNREAD\",\"%s\",\"\",\"%s\"\r\n", index, phone, scts);
  return buf;
}

std::string ucs2(const char* utf8) {
  char buf[2048];
  TEST_ASSERT_TRUE(text::utf8ToUcs2Hex(utf8, buf, sizeof(buf)) >= 0);
  return buf;
}

const char* kScts = "26/09/15,14:32:10+08";
const uint32_t kSctsEpoch = 1789475530;  // 2026-09-15 12:32:10 UTC

}  // namespace

void setUp() {}
void tearDown() {}

// ---- LineReader ---------------------------------------------------------------------------------

void test_line_reader_splits_crlf_lines() {
  LineReader r;
  std::string lines;
  for (const char* p = "\r\nRING\r\n\r\n+CLIP: \"+491701111111\",145\r\n"; *p; ++p)
    if (r.feed(*p)) lines += std::string(r.line()) + '|';
  TEST_ASSERT_EQUAL_STRING("|RING||+CLIP: \"+491701111111\",145|", lines.c_str());
}

void test_line_reader_flags_an_overlong_line_and_recovers() {
  LineReader r;
  std::string longLine(LineReader::kMaxLine + 50, 'x');
  longLine += "\r\n";
  bool got = false;
  for (char c : longLine) got = r.feed(c) || got;
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_TRUE(r.truncated());
  TEST_ASSERT_EQUAL(LineReader::kMaxLine, strlen(r.line()));
  for (const char* p = "OK\r\n"; *p; ++p) r.feed(*p);
  TEST_ASSERT_FALSE(r.truncated());
  TEST_ASSERT_EQUAL_STRING("OK", r.line());
}

void test_line_reader_prompt_only_in_prompt_mode() {
  LineReader r;
  TEST_ASSERT_FALSE(r.feed('>'));
  TEST_ASSERT_FALSE(r.feed(' '));  // without the mode a normal start of a line
  r.reset();
  r.setPromptMode(true);
  TEST_ASSERT_FALSE(r.feed('>'));
  TEST_ASSERT_TRUE(r.feed(' '));
  TEST_ASSERT_EQUAL(LineType::Prompt, classify(r.line()));
}

// Half a line is no line; the rest completes it, however the bytes were split.
void test_a_line_that_arrives_in_pieces_is_one_line() {
  LineReader r;
  TEST_ASSERT_FALSE(r.feed('+'));
  for (const char* p = "CSQ: 2"; *p; ++p) TEST_ASSERT_FALSE(r.feed(*p));
  TEST_ASSERT_FALSE(r.feed('3'));
  TEST_ASSERT_FALSE(r.feed('\r'));
  TEST_ASSERT_TRUE(r.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("+CSQ: 23", r.line());
}

// Line noise on a UART: NUL bytes would cut a C string short, so they are dropped.
void test_a_nul_byte_is_dropped_and_does_not_end_the_line() {
  LineReader r;
  const char noisy[] = "O\0K\r\n";
  bool got = false;
  for (size_t i = 0; i < sizeof(noisy) - 1; ++i) got = r.feed(noisy[i]) || got;
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_STRING("OK", r.line());
}

// After a restart the half line that was being received belongs to the old life.
void test_reset_forgets_a_partial_line() {
  LineReader r;
  for (const char* p = "+CLIP: \"+4917"; *p; ++p) r.feed(*p);
  r.reset();
  for (const char* p = "RING\r\n"; *p; ++p) r.feed(*p);
  TEST_ASSERT_EQUAL_STRING("RING", r.line());
}

// ---- Deadline -----------------------------------------------------------------------------------

void test_a_deadline_expires_at_its_time_and_not_before() {
  Deadline d;
  TEST_ASSERT_FALSE(d.armed());
  TEST_ASSERT_FALSE(d.expired(1000000));  // not set: never expires
  d.set(1000, 500);
  TEST_ASSERT_TRUE(d.armed());
  TEST_ASSERT_FALSE(d.expired(1499));
  TEST_ASSERT_TRUE(d.expired(1500));
  TEST_ASSERT_TRUE(d.expired(9000));
  d.clear();
  TEST_ASSERT_FALSE(d.expired(9000));
}

// The millisecond counter wraps after 49 days; a deadline set just before must not fire at once or never.
void test_a_deadline_survives_the_wrap_of_the_clock() {
  Deadline d;
  d.set(0xFFFFFF00u, 1000);  // lands at 0x000002E8 after the wrap
  TEST_ASSERT_FALSE(d.expired(0xFFFFFF00u));
  TEST_ASSERT_FALSE(d.expired(0xFFFFFFFFu));
  TEST_ASSERT_FALSE(d.expired(0x000002E7u));
  TEST_ASSERT_TRUE(d.expired(0x000002E8u));
}

// ---- classification -----------------------------------------------------------------------------

void test_classify_matches_whole_lines_only() {
  TEST_ASSERT_EQUAL(LineType::Busy, classify("BUSY"));
  TEST_ASSERT_EQUAL(LineType::Other, classify("I am BUSY, call later"));
  TEST_ASSERT_EQUAL(LineType::Other, classify("NO CARRIER today"));
  TEST_ASSERT_EQUAL(LineType::Ring, classify("RING"));
  TEST_ASSERT_EQUAL(LineType::Other, classify("RINGTONE"));
  TEST_ASSERT_EQUAL(LineType::Error, classify("+CMS ERROR: 321"));
  TEST_ASSERT_EQUAL(LineType::Error, classify("+CME ERROR: SIM not inserted"));
  TEST_ASSERT_EQUAL(LineType::Empty, classify(""));
  TEST_ASSERT_EQUAL(LineType::Empty, classify(nullptr));
}

void test_classify_knows_the_lines_the_driver_reads() {
  TEST_ASSERT_EQUAL(LineType::AtReady, classify("*ATREADY: 1"));
  TEST_ASSERT_EQUAL(LineType::Cireg, classify("+CIREG: 1,1,15"));
  TEST_ASSERT_EQUAL(LineType::Cgdcont, classify("+CGDCONT: 1,\"IPV4V6\",\"internet.example\""));
  TEST_ASSERT_EQUAL(LineType::Clcc, classify("+CLCC: 1,0,2,0,0"));
  TEST_ASSERT_EQUAL(LineType::Cpin, classify("+CPIN: READY"));
  TEST_ASSERT_EQUAL(LineType::Creg, classify("+CREG: 0,1"));
  TEST_ASSERT_EQUAL(LineType::Cops, classify("+COPS: 0"));
  TEST_ASSERT_EQUAL(LineType::Cclk, classify("+CCLK: \"26/09/15,14:32:10+08\""));
  TEST_ASSERT_EQUAL(LineType::VoiceCallBegin, classify("VOICE CALL: BEGIN"));
  TEST_ASSERT_EQUAL(LineType::Other, classify("SMS DONE"));
  TEST_ASSERT_EQUAL(LineType::Other, classify("+CIREGX: 1,1"));   // the colon is part of the name
}

// ---- single lines -------------------------------------------------------------------------------

void test_parse_clip_gsm_and_ucs2() {
  char phone[kPhoneSize];
  TEST_ASSERT_TRUE(parseClip("+CLIP: \"+491701111111\",145,,,,0", Charset::Gsm, phone, sizeof(phone)));
  TEST_ASSERT_EQUAL_STRING("+491701111111", phone);
  const std::string line = "+CLIP: \"" + ucs2("+491701111111") + "\",145,,,,0";
  TEST_ASSERT_TRUE(parseClip(line.c_str(), Charset::Ucs2, phone, sizeof(phone)));
  TEST_ASSERT_EQUAL_STRING("+491701111111", phone);
}

void test_parse_clip_refuses_an_overlong_or_missing_number() {
  char phone[kPhoneSize];
  TEST_ASSERT_FALSE(parseClip("+CLIP: \"+49170111111111111111111\",145", Charset::Gsm, phone, sizeof(phone)));
  TEST_ASSERT_EQUAL_STRING("", phone);
  TEST_ASSERT_FALSE(parseClip("+CLIP: \"\",145,,,,0", Charset::Gsm, phone, sizeof(phone)));
  TEST_ASSERT_FALSE(parseClip("RING", Charset::Gsm, phone, sizeof(phone)));
}

void test_parse_cmti() {
  int idx = -1;
  TEST_ASSERT_TRUE(parseCmti("+CMTI: \"SM\",7", &idx));
  TEST_ASSERT_EQUAL(7, idx);
  TEST_ASSERT_FALSE(parseCmti("+CMTI: \"SM\",", &idx));
  TEST_ASSERT_FALSE(parseCmti("+CMTI: \"SM\",12abc", &idx));
  TEST_ASSERT_FALSE(parseCmti("+CMTI: \"SM\",99999999999", &idx));   // no overflow into a small number
}

void test_voice_call_end_gives_the_duration_in_all_its_forms() {
  uint32_t secs = 99;
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END: 1:12", &secs));
  TEST_ASSERT_EQUAL_UINT32(72, secs);
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END: 1:02:03", &secs));
  TEST_ASSERT_EQUAL_UINT32(3723, secs);
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END: 000032", &secs));   // hhmmss
  TEST_ASSERT_EQUAL_UINT32(32, secs);
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END: 011234", &secs));
  TEST_ASSERT_EQUAL_UINT32(3600 + 12 * 60 + 34, secs);
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END", &secs));
  TEST_ASSERT_EQUAL_UINT32(0, secs);
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END: 0:75", &secs));   // seconds above 59: no guess
  TEST_ASSERT_EQUAL_UINT32(0, secs);
  TEST_ASSERT_TRUE(parseVoiceCallEnd("VOICE CALL: END: soon", &secs));
  TEST_ASSERT_EQUAL_UINT32(0, secs);
  TEST_ASSERT_FALSE(parseVoiceCallEnd("VOICE CALL: BEGIN", &secs));
}

void test_csq_and_bars() {
  int rssi = 0;
  TEST_ASSERT_TRUE(parseCsq("+CSQ: 23,99", &rssi));
  TEST_ASSERT_EQUAL(23, rssi);
  TEST_ASSERT_FALSE(parseCsq("+CSQ: ,99", &rssi));
  TEST_ASSERT_EQUAL(5, csqToBars(23));
  TEST_ASSERT_EQUAL(4, csqToBars(20));
  TEST_ASSERT_EQUAL(3, csqToBars(15));
  TEST_ASSERT_EQUAL(2, csqToBars(10));
  TEST_ASSERT_EQUAL(1, csqToBars(3));
  TEST_ASSERT_EQUAL(0, csqToBars(0));
  TEST_ASSERT_EQUAL(0, csqToBars(99));
}

void test_creg_tells_registered_and_sms_only() {
  Registration r;
  TEST_ASSERT_TRUE(parseCreg("+CREG: 0,1", &r));
  TEST_ASSERT_TRUE(r.registered());
  TEST_ASSERT_FALSE(r.smsOnly());
  TEST_ASSERT_TRUE(parseCreg("+CREG: 0,5", &r));    // roaming
  TEST_ASSERT_TRUE(r.registered());
  TEST_ASSERT_FALSE(r.smsOnly());
  TEST_ASSERT_TRUE(parseCreg("+CREG: 0,7", &r));    // the device's SIM on 2026-09-30: SMS went out all the same
  TEST_ASSERT_TRUE(r.registered());
  TEST_ASSERT_TRUE(r.smsOnly());
  TEST_ASSERT_TRUE(parseCreg("+CREG: 0,6", &r));
  TEST_ASSERT_TRUE(r.smsOnly());
  TEST_ASSERT_TRUE(parseCreg("+CREG: 2,2,\"1A2B\",\"00C3\"", &r));   // searching, with extra fields
  TEST_ASSERT_FALSE(r.registered());
  TEST_ASSERT_TRUE(parseCreg("+CREG: 0,3", &r));    // denied
  TEST_ASSERT_FALSE(r.registered());
  TEST_ASSERT_FALSE(parseCreg("+CREG: 1", &r));     // no status field
  TEST_ASSERT_FALSE(parseCreg("+CREG: 0,x", &r));
}

void test_cireg_tells_whether_the_ims_is_registered() {
  ImsRegistration r;
  TEST_ASSERT_TRUE(parseCireg("+CIREG: 1,1,15", &r));   // the device that works
  TEST_ASSERT_TRUE(r.registered());
  TEST_ASSERT_EQUAL(1, r.mode);
  TEST_ASSERT_EQUAL(15, r.ext);
  TEST_ASSERT_TRUE(parseCireg("+CIREG: 1,0,0", &r));    // the broken state of the measurements
  TEST_ASSERT_FALSE(r.registered());
  TEST_ASSERT_TRUE(parseCireg("+CIREG: 1,1", &r));
  TEST_ASSERT_TRUE(r.registered());
  TEST_ASSERT_EQUAL(-1, r.ext);
  TEST_ASSERT_FALSE(parseCireg("+CIREG: 1", &r));
  TEST_ASSERT_FALSE(parseCireg("+CIREG: 1,x", &r));
}

void test_cgdcont_gives_the_context_type_and_apn() {
  PdpContext c;
  TEST_ASSERT_TRUE(parseCgdcont("+CGDCONT: 1,\"IPV4V6\",\"internet.example\",\"0.0.0.0\",0,0,0,0", &c));
  TEST_ASSERT_EQUAL(1, c.cid);
  TEST_ASSERT_EQUAL_STRING("IPV4V6", c.type);
  TEST_ASSERT_EQUAL_STRING("internet.example", c.apn);
  TEST_ASSERT_TRUE(parseCgdcont("+CGDCONT: 8,\"IPV4V6\",\"ims\"", &c));   // the IMS bearer: never touched, but read
  TEST_ASSERT_EQUAL(8, c.cid);
  TEST_ASSERT_EQUAL_STRING("ims", c.apn);
  TEST_ASSERT_TRUE(parseCgdcont("+CGDCONT: 1,\"IP\",\"\"", &c));          // an empty APN is a valid, empty one
  TEST_ASSERT_EQUAL_STRING("", c.apn);
  TEST_ASSERT_FALSE(parseCgdcont("+CGDCONT: 1,\"IP\"", &c));
  TEST_ASSERT_FALSE(parseCgdcont("+CGDCONT: x,\"IP\",\"a\"", &c));
}

// An APN cut short could look equal to the configured one, and then nothing would be written.
void test_cgdcont_refuses_a_field_that_does_not_fit() {
  PdpContext c;
  const std::string longApn(101, 'a');
  const std::string line = "+CGDCONT: 1,\"IP\",\"" + longApn + "\"";
  TEST_ASSERT_FALSE(parseCgdcont(line.c_str(), &c));
  TEST_ASSERT_FALSE(parseCgdcont("+CGDCONT: 1,\"IPV4V6IPV4V6\",\"a\"", &c));
  const std::string fits = "+CGDCONT: 1,\"IP\",\"" + std::string(100, 'a') + "\"";
  TEST_ASSERT_TRUE(parseCgdcont(fits.c_str(), &c));
  TEST_ASSERT_EQUAL(100, strlen(c.apn));
}

void test_clcc_gives_index_direction_state_and_number() {
  CallListEntry e;
  TEST_ASSERT_TRUE(parseClcc("+CLCC: 1,0,2,0,0,\"+491701111111\",145", Charset::Gsm, &e));
  TEST_ASSERT_EQUAL(1, e.index);
  TEST_ASSERT_EQUAL(0, e.direction);
  TEST_ASSERT_EQUAL(2, e.state);
  TEST_ASSERT_EQUAL_STRING("+491701111111", e.phone);
  TEST_ASSERT_TRUE(parseClcc("+CLCC: 1,1,4,0,0,\"\",128", Charset::Gsm, &e));   // number withheld
  TEST_ASSERT_EQUAL(4, e.state);
  TEST_ASSERT_EQUAL_STRING("", e.phone);
}

void test_clcc_without_a_number_field_is_still_a_call() {
  CallListEntry e;
  TEST_ASSERT_TRUE(parseClcc("+CLCC: 1,0,3,0,0", Charset::Gsm, &e));
  TEST_ASSERT_EQUAL(3, e.state);
  TEST_ASSERT_EQUAL_STRING("", e.phone);
  const std::string line = "+CLCC: 1,0,0,0,0,\"" + ucs2("+491702222222") + "\",145";
  TEST_ASSERT_TRUE(parseClcc(line.c_str(), Charset::Ucs2, &e));
  TEST_ASSERT_EQUAL(0, e.state);
  TEST_ASSERT_EQUAL_STRING("+491702222222", e.phone);
  TEST_ASSERT_FALSE(parseClcc("+CLCC: 1,0,2", Charset::Gsm, &e));
  TEST_ASSERT_FALSE(parseClcc("+CLCC: 1,0,x,0,0", Charset::Gsm, &e));
}

void test_cpin_and_the_missing_card() {
  SimStatus s = SimStatus::Unknown;
  TEST_ASSERT_TRUE(parseCpin("+CPIN: READY", &s));
  TEST_ASSERT_TRUE(s == SimStatus::Ready);
  TEST_ASSERT_TRUE(parseCpin("+CPIN: SIM PIN", &s));
  TEST_ASSERT_TRUE(s == SimStatus::PinRequired);
  TEST_ASSERT_TRUE(parseCpin("+CPIN: SIM PUK", &s));
  TEST_ASSERT_TRUE(s == SimStatus::PukRequired);
  s = SimStatus::Unknown;
  TEST_ASSERT_FALSE(parseCpin("+CPIN: SIM PIN2", &s));      // neither usable nor known to want the PIN
  TEST_ASSERT_TRUE(s == SimStatus::Unknown);
  TEST_ASSERT_FALSE(parseCpin("OK", &s));
  TEST_ASSERT_TRUE(isSimNotInserted("+CME ERROR: SIM not inserted"));
  TEST_ASSERT_FALSE(isSimNotInserted("+CME ERROR: SIM failure"));   // the first seconds: not known yet
  TEST_ASSERT_FALSE(isSimNotInserted("SIM not inserted"));          // only an error line counts
}

void test_the_operator_name_comes_out_of_cops() {
  char name[24];
  TEST_ASSERT_TRUE(parseCopsName("+COPS: 0,0,\"Example Mobile\",7", name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("Example Mobile", name);
  TEST_ASSERT_TRUE(parseCopsName("+COPS: 0,0,\"26201\",7", name, sizeof(name)));   // the code instead of the name
  TEST_ASSERT_EQUAL_STRING("Telekom", name);
  TEST_ASSERT_FALSE(parseCopsName("+COPS: 0", name, sizeof(name)));                 // not registered: no name
  TEST_ASSERT_EQUAL_STRING("", name);
  TEST_ASSERT_FALSE(parseCopsName("+COPS: 0,0,\"\",7", name, sizeof(name)));
  // the A7682E with a Telekom SIM, 2026-10-08: a blank is no name either
  TEST_ASSERT_FALSE(parseCopsName("+COPS: 0,0,\" \",7", name, sizeof(name)));
  TEST_ASSERT_FALSE(parseCopsName("+COPS: 0,0,\"   \",7", name, sizeof(name)));
  TEST_ASSERT_TRUE(parseCopsName("+COPS: 0,0,\" Example\",7", name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("Example", name);
  char small[8];
  TEST_ASSERT_TRUE(parseCopsName("+COPS: 0,0,\"A very long operator name\",7", small, sizeof(small)));
  TEST_ASSERT_EQUAL_STRING("A very ", small);                                       // cut to fit, still terminated
}

void test_the_first_operator_code_of_a_scan_list() {
  char code[8];
  TEST_ASSERT_TRUE(parseFirstOperatorCode("+COPS: (2,\"o2 - de\",\"o2 - de\",\"26203\",7),(1,\"TDG\",\"TDG\",\"26201\",7)", code, sizeof(code)));
  TEST_ASSERT_EQUAL_STRING("26203", code);
  TEST_ASSERT_TRUE(parseFirstOperatorCode("+COPS: (1,\"X\",\"X\",\"310260\",7)", code, sizeof(code)));
  TEST_ASSERT_EQUAL_STRING("310260", code);
  TEST_ASSERT_FALSE(parseFirstOperatorCode("+COPS: (2,\"Name\",\"Name\")", code, sizeof(code)));
  TEST_ASSERT_FALSE(parseFirstOperatorCode("+COPS: (2,\"2620", code, sizeof(code)));   // an unterminated quote
  char tiny[6];
  TEST_ASSERT_FALSE(parseFirstOperatorCode("+COPS: (2,\"a\",\"b\",\"26203\",7)", code, 0));
  TEST_ASSERT_FALSE(parseFirstOperatorCode("+COPS: (2,\"a\",\"b\",\"26203\",7)", tiny, sizeof(tiny)));
}

void test_operator_codes_become_names_for_the_usual_countries() {
  TEST_ASSERT_EQUAL_STRING("Telekom", operatorNameForCode("26201"));
  TEST_ASSERT_EQUAL_STRING("Vodafone", operatorNameForCode("26202"));
  TEST_ASSERT_EQUAL_STRING("O2", operatorNameForCode("26203"));
  TEST_ASSERT_EQUAL_STRING("Orange", operatorNameForCode("20801"));
  TEST_ASSERT_EQUAL_STRING("SFR", operatorNameForCode("20810"));
  TEST_ASSERT_EQUAL_STRING("Movistar", operatorNameForCode("21407"));
  TEST_ASSERT_EQUAL_STRING("26299", operatorNameForCode("26299"));          // unknown: stays
  TEST_ASSERT_EQUAL_STRING("Example Mobile", operatorNameForCode("Example Mobile"));
  TEST_ASSERT_EQUAL_STRING("99999", operatorNameForCode("99999"));          // another country: stays
}

void test_scts_applies_the_time_zone() {
  uint32_t t = 0;
  TEST_ASSERT_TRUE(parseScts(kScts, &t));
  TEST_ASSERT_EQUAL_UINT32(kSctsEpoch, t);
  // New year with a negative offset: 31.12.25 23:00 at UTC-2 is 01.01.26 01:00 UTC.
  TEST_ASSERT_TRUE(parseScts("25/12/31,23:00:00-08", &t));
  TEST_ASSERT_EQUAL_UINT32(1767229200, t);
  TEST_ASSERT_FALSE(parseScts("26/13/15,14:32:10+08", &t));
  TEST_ASSERT_FALSE(parseScts("no date", &t));
}

void test_scts_checks_the_day_against_the_month() {
  uint32_t t = 0;
  TEST_ASSERT_TRUE(parseScts("28/02/29,12:00:00+00", &t));    // 2028 is a leap year
  TEST_ASSERT_FALSE(parseScts("27/02/29,12:00:00+00", &t));
  TEST_ASSERT_FALSE(parseScts("26/04/31,12:00:00+00", &t));
  TEST_ASSERT_FALSE(parseScts("26/09/15,24:00:00+00", &t));
  TEST_ASSERT_FALSE(parseScts("26/09/15,12:00:00+99", &t));
  TEST_ASSERT_FALSE(parseScts("-1/09/15,12:00:00+00", &t));
}

void test_cclk_gives_the_time_and_the_zone() {
  NetworkTime n;
  TEST_ASSERT_TRUE(parseCclk("+CCLK: \"26/09/15,14:32:10+08\"", &n));
  TEST_ASSERT_EQUAL_UINT32(kSctsEpoch, n.epochUtc);
  TEST_ASSERT_EQUAL(120, n.tzOffsetMinutes);
  TEST_ASSERT_TRUE(parseCclk("+CCLK: \"26/01/15,10:00:00-20\"", &n));
  TEST_ASSERT_EQUAL(-300, n.tzOffsetMinutes);
  TEST_ASSERT_FALSE(parseCclk("+CCLK: 26/09/15", &n));
}

// A modem that has no network time yet runs its own clock from 1970 (or 2004): no time at all
// is better than the year 2070.
void test_cclk_refuses_the_clock_of_a_modem_without_network_time() {
  NetworkTime n;
  TEST_ASSERT_FALSE(parseCclk("+CCLK: \"70/01/01,00:00:12+00\"", &n));
  TEST_ASSERT_FALSE(parseCclk("+CCLK: \"04/01/01,00:01:30+00\"", &n));
  TEST_ASSERT_FALSE(parseCclk("+CCLK: \"19/12/31,23:59:59+00\"", &n));
  TEST_ASSERT_TRUE(parseCclk("+CCLK: \"20/01/01,00:00:00+00\"", &n));
  TEST_ASSERT_TRUE(parseCclk("+CCLK: \"69/12/31,23:59:59+00\"", &n));
}

void test_a_dial_string_is_digits_star_hash_and_a_leading_plus() {
  for (const char* good : {"+491701111111", "0170111", "*#06#", "112", "+4917011111111111111111"})   // the last has 23 characters
    TEST_ASSERT_TRUE_MESSAGE(isValidDialString(good), good);
  for (const char* bad : {"", "+", "49+170", "0170 1111", "+49-170", "ATD1;", "\"1", "1\r\nAT", "+49170111111111111111111", "\xD9\xA1"})
    TEST_ASSERT_FALSE_MESSAGE(isValidDialString(bad), bad);
}

void test_cmgs_command_valid_and_rejected_numbers() {
  char cmd[128];
  TEST_ASSERT_TRUE(buildCmgsCommand("+491701111111", Charset::Gsm, cmd, sizeof(cmd)));
  TEST_ASSERT_EQUAL_STRING("AT+CMGS=\"+491701111111\"", cmd);
  TEST_ASSERT_TRUE(buildCmgsCommand("+4917", Charset::Ucs2, cmd, sizeof(cmd)));
  TEST_ASSERT_EQUAL_STRING("AT+CMGS=\"002B0034003900310037\"", cmd);

  // 24 characters do not fit: an error and not a silent cut.
  TEST_ASSERT_FALSE(buildCmgsCommand("+49170111111111111111111", Charset::Gsm, cmd, sizeof(cmd)));
  TEST_ASSERT_TRUE(buildCmgsCommand("+4917011111111111111111", Charset::Gsm, cmd, sizeof(cmd)));  // 23
  TEST_ASSERT_FALSE(buildCmgsCommand("0170 1111", Charset::Gsm, cmd, sizeof(cmd)));
  TEST_ASSERT_FALSE(buildCmgsCommand("49+170", Charset::Gsm, cmd, sizeof(cmd)));
  TEST_ASSERT_FALSE(buildCmgsCommand("\"; AT+CPOF", Charset::Gsm, cmd, sizeof(cmd)));
  TEST_ASSERT_FALSE(buildCmgsCommand("", Charset::Gsm, cmd, sizeof(cmd)));
  TEST_ASSERT_FALSE(buildCmgsCommand("+", Charset::Gsm, cmd, sizeof(cmd)));

  char small[16];  // too small: an error, not cut
  TEST_ASSERT_FALSE(buildCmgsCommand("+491701111111", Charset::Gsm, small, sizeof(small)));
  TEST_ASSERT_EQUAL_STRING("", small);
}

// ---- +CMGL / +CMGR -----------------------------------------------------------------------------

void test_cmgl_single_message_gsm() {
  const std::string resp = "\r\n" + cmglHeader(1, "+491701111111", kScts) + "Pick us up\r\n\r\nOK\r\n";
  ParsedSms out[4];
  SmsResponseParser p(Charset::Gsm, out, 4);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(p.finished());
  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_EQUAL(1, out[0].index);
  TEST_ASSERT_TRUE(out[0].senderValid);
  TEST_ASSERT_EQUAL_STRING("+491701111111", out[0].msg.phone.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick us up", out[0].msg.body.c_str());
  TEST_ASSERT_EQUAL_UINT32(kSctsEpoch, out[0].msg.timestamp);
  TEST_ASSERT_FALSE(out[0].bodyTruncated);
  TEST_ASSERT_EQUAL_STRING("", f.unsolicited.c_str());
}

// Meck sent AT+CMGD after the first message and overwrote the buffer: all three must arrive with their index.
void test_cmgl_three_messages_all_parsed_with_indices() {
  const std::string resp = "\r\n" + cmglHeader(3, "+491701111111", kScts) + "one\r\n" +
                           cmglHeader(5, "+491702222222", kScts) + "two\r\n" +
                           cmglHeader(9, "+493012345678", kScts) + "three\r\n" + "\r\nOK\r\n";
  ParsedSms out[8];
  SmsResponseParser p(Charset::Gsm, out, 8);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(3, p.count());
  const int idx[] = {3, 5, 9};
  const char* phones[] = {"+491701111111", "+491702222222", "+493012345678"};
  const char* bodies[] = {"one", "two", "three"};
  for (int i = 0; i < 3; ++i) {
    TEST_ASSERT_EQUAL(idx[i], out[i].index);
    TEST_ASSERT_EQUAL_STRING(phones[i], out[i].msg.phone.c_str());
    TEST_ASSERT_EQUAL_STRING(bodies[i], out[i].msg.body.c_str());
  }
}

// Ten messages of 160 characters are about 2 KB, four times a 512-byte answer buffer: line by line is no trouble.
void test_cmgl_ten_full_length_messages() {
  std::string resp = "\r\n";
  for (int i = 0; i < 10; ++i) {
    resp += cmglHeader(i + 1, "+491701111111", kScts);
    resp += std::string(159, (char)('a' + i)) + (char)('0' + i) + "\r\n";
  }
  resp += "\r\nOK\r\n";
  TEST_ASSERT_TRUE(resp.size() > 2000);

  ParsedSms out[10];
  SmsResponseParser p(Charset::Gsm, out, 10);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(10, p.count());
  TEST_ASSERT_EQUAL(0, p.dropped());
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT_EQUAL(i + 1, out[i].index);
    TEST_ASSERT_EQUAL(160, out[i].msg.body.size());
    TEST_ASSERT_EQUAL((char)('0' + i), out[i].msg.body[159]);
    TEST_ASSERT_FALSE(out[i].bodyTruncated);
  }
}

// Meck cut the text at the first CR.
void test_cmgl_multiline_body_gsm() {
  const std::string resp = "\r\n" + cmglHeader(1, "+491701111111", kScts) +
                           "Hello Dad,\r\n\r\nschool is off.\r\nSee you\r\n" +
                           cmglHeader(2, "+491702222222", kScts) + "second\r\n" + "\r\nOK\r\n";
  ParsedSms out[4];
  SmsResponseParser p(Charset::Gsm, out, 4);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(2, p.count());
  TEST_ASSERT_EQUAL_STRING("Hello Dad,\n\nschool is off.\nSee you", out[0].msg.body.c_str());
  TEST_ASSERT_EQUAL_STRING("second", out[1].msg.body.c_str());
}

// In UCS2 mode number and text are hex; line breaks are inside the hex.
void test_cmgl_ucs2_umlauts_emoji_newline() {
  const std::string resp = "\r\n" + cmglHeader(4, ucs2("+491701111111").c_str(), kScts) +
                           ucs2("Schule f\xC3\xA4llt aus \xF0\x9F\x98\x80\nPick us up") + "\r\n" +
                           cmglHeader(6, ucs2("+493012345678").c_str(), kScts) + ucs2("Gr\xC3\xBC\xC3\x9F" "e") +
                           "\r\n" + "\r\nOK\r\n";
  ParsedSms out[4];
  SmsResponseParser p(Charset::Ucs2, out, 4);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(2, p.count());
  TEST_ASSERT_EQUAL_STRING("+491701111111", out[0].msg.phone.c_str());
  TEST_ASSERT_EQUAL_STRING("Schule f\xC3\xA4llt aus \xF0\x9F\x98\x80\nPick us up", out[0].msg.body.c_str());
  TEST_ASSERT_EQUAL_STRING("+493012345678", out[1].msg.phone.c_str());
  TEST_ASSERT_EQUAL_STRING("Gr\xC3\xBC\xC3\x9F" "e", out[1].msg.body.c_str());
}

// A RING in the middle of an answer is no text but a URC; a text that reads "BUSY" is no busy signal.
void test_cmgl_ucs2_urc_between_messages_is_unsolicited() {
  const std::string resp = "\r\n" + cmglHeader(1, ucs2("+491701111111").c_str(), kScts) + ucs2("BUSY") + "\r\n" +
                           "RING\r\n" + "+CLIP: \"" + ucs2("+491702222222") + "\",145,,,,0\r\n" +
                           cmglHeader(2, ucs2("+493012345678").c_str(), kScts) + ucs2("two") + "\r\n" + "\r\nOK\r\n";
  ParsedSms out[4];
  SmsResponseParser p(Charset::Ucs2, out, 4);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(2, p.count());
  TEST_ASSERT_EQUAL_STRING("BUSY", out[0].msg.body.c_str());
  TEST_ASSERT_EQUAL_STRING("two", out[1].msg.body.c_str());
  const std::string expected = "RING|+CLIP: \"" + ucs2("+491702222222") + "\",145,,,,0|";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), f.unsolicited.c_str());
}

// A timeout cut the answer: only messages whose end is seen are delivered; the last one stays on the SIM.
void test_cmgl_truncated_response_keeps_the_last_message_on_the_sim() {
  const std::string resp = "\r\n" + cmglHeader(1, "+491701111111", kScts) + "one\r\n" +
                           cmglHeader(2, "+491702222222", kScts) + "two\r\n" +
                           cmglHeader(3, "+493012345678", kScts) + "th";  // the stream stops here
  ParsedSms out[4];
  SmsResponseParser p(Charset::Gsm, out, 4);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_FALSE(p.finished());
  TEST_ASSERT_EQUAL(2, p.count());
  TEST_ASSERT_EQUAL(1, out[0].index);
  TEST_ASSERT_EQUAL(2, out[1].index);
}

void test_cmgl_capacity_exceeded_counts_dropped() {
  std::string resp = "\r\n";
  for (int i = 1; i <= 5; ++i) resp += cmglHeader(i, "+491701111111", kScts) + "x\r\n";
  resp += "\r\nOK\r\n";
  ParsedSms out[3];
  SmsResponseParser p(Charset::Gsm, out, 3);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(3, p.count());
  TEST_ASSERT_EQUAL(2, p.dropped());  // not put away: not to be deleted
  TEST_ASSERT_EQUAL(3, out[2].index);
}

void test_cmgl_empty_list_and_error() {
  ParsedSms out[2];
  SmsResponseParser empty(Charset::Gsm, out, 2);
  Feeder f1;
  f1.run(empty, "\r\nOK\r\n");
  TEST_ASSERT_TRUE(empty.ok());
  TEST_ASSERT_EQUAL(0, empty.count());

  SmsResponseParser err(Charset::Gsm, out, 2);
  Feeder f2;
  f2.run(err, "\r\n+CMS ERROR: 321\r\n");
  TEST_ASSERT_TRUE(err.finished());
  TEST_ASSERT_FALSE(err.ok());
}

void test_cmgl_invalid_sender_is_still_delivered_for_deletion() {
  const std::string resp = "\r\n" + cmglHeader(8, "+49170111111111111111111111", kScts) + "Advert\r\n\r\nOK\r\n";
  ParsedSms out[2];
  SmsResponseParser p(Charset::Gsm, out, 2);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_EQUAL(8, out[0].index);
  TEST_ASSERT_FALSE(out[0].senderValid);
  TEST_ASSERT_EQUAL_STRING("", out[0].msg.phone.c_str());
  TEST_ASSERT_EQUAL_STRING("Advert", out[0].msg.body.c_str());
}

void test_cmgl_overlong_body_is_cut_and_flagged() {
  const std::string resp = "\r\n" + cmglHeader(1, "+491701111111", kScts) + std::string(400, 'z') + "\r\n\r\nOK\r\n";
  ParsedSms out[1];
  SmsResponseParser p(Charset::Gsm, out, 1);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_TRUE(out[0].bodyTruncated);
  TEST_ASSERT_EQUAL(kMaxSmsBytes, out[0].msg.body.size());
}

// A cut inside a two-byte character would leave half of it; the text must stay valid UTF-8.
void test_a_gsm_body_is_cut_at_a_character_boundary() {
  const std::string text = std::string(kMaxSmsBytes - 1, 'a') + "\xC3\xA4" "bc";   // the umlaut straddles the limit
  const std::string resp = "\r\n" + cmglHeader(1, "+491701111111", kScts) + text + "\r\n\r\nOK\r\n";
  ParsedSms out[1];
  SmsResponseParser p(Charset::Gsm, out, 1);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(out[0].bodyTruncated);
  TEST_ASSERT_EQUAL(kMaxSmsBytes - 1, out[0].msg.body.size());
  TEST_ASSERT_EQUAL('a', out[0].msg.body.back());
}

void test_a_ucs2_body_that_does_not_fit_is_cut_and_flagged() {
  const std::string body(400, 'x');
  const std::string resp = "\r\n" + cmglHeader(1, ucs2("+491701111111").c_str(), kScts) + ucs2(body.c_str()) +
                           "\r\n\r\nOK\r\n";
  ParsedSms out[1];
  SmsResponseParser p(Charset::Ucs2, out, 1);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_TRUE(out[0].bodyTruncated);
  TEST_ASSERT_EQUAL(kMaxSmsBytes, out[0].msg.body.size());
}

// 160 characters in UCS2 are one line of 640 hex digits. The first version of the line buffer held 512
// and cut such a text to 128 characters, valid hex and no sign of it.
void test_a_long_ucs2_text_arrives_whole() {
  const std::string body(160, 'q');
  const std::string resp = "\r\n" + cmglHeader(1, ucs2("+491701111111").c_str(), kScts) + ucs2(body.c_str()) +
                           "\r\n\r\nOK\r\n";
  ParsedSms out[1];
  SmsResponseParser p(Charset::Ucs2, out, 1);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_FALSE(out[0].bodyTruncated);
  TEST_ASSERT_EQUAL_STRING(body.c_str(), out[0].msg.body.c_str());
  const std::string longest(kMaxSmsBytes, 'q');                // the longest text the interface carries
  SmsResponseParser q(Charset::Ucs2, out, 1);
  Feeder g;
  g.run(q, ("\r\n" + cmglHeader(2, ucs2("+491701111111").c_str(), kScts) + ucs2(longest.c_str()) + "\r\n\r\nOK\r\n").c_str());
  TEST_ASSERT_FALSE(out[0].bodyTruncated);
  TEST_ASSERT_EQUAL(kMaxSmsBytes, out[0].msg.body.size());
}

void test_spaces_after_the_commas_of_a_header_do_no_harm() {
  const std::string resp = "\r\n+CMGL: 4, \"REC UNREAD\", \"+491701111111\", \"\", \"26/09/15,14:32:10+08\"\r\nhi\r\n\r\nOK\r\n";
  ParsedSms out[1];
  SmsResponseParser p(Charset::Gsm, out, 1);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_EQUAL(4, out[0].index);
  TEST_ASSERT_EQUAL_STRING("+491701111111", out[0].msg.phone.c_str());
  TEST_ASSERT_EQUAL_UINT32(kSctsEpoch, out[0].msg.timestamp);
}

void test_cmgr_single_message_ucs2() {
  const std::string resp = "\r\n+CMGR: \"REC UNREAD\",\"" + ucs2("+491701111111") + "\",\"\",\"" + kScts + "\"\r\n" +
                           ucs2("hello 42") + "\r\n\r\nOK\r\n";
  ParsedSms out[1];
  SmsResponseParser p(Charset::Ucs2, out, 1);
  Feeder f;
  f.run(p, resp.c_str());

  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(1, p.count());
  TEST_ASSERT_EQUAL(-1, out[0].index);  // +CMGR does not carry it
  TEST_ASSERT_EQUAL_STRING("+491701111111", out[0].msg.phone.c_str());
  TEST_ASSERT_EQUAL_STRING("hello 42", out[0].msg.body.c_str());
  TEST_ASSERT_EQUAL_UINT32(kSctsEpoch, out[0].msg.timestamp);
}

// The driver reads the inbox again and again with one parser object; nothing of the last answer may stay.
void test_a_parser_is_reusable_for_the_next_answer() {
  ParsedSms out[2];
  SmsResponseParser p(Charset::Gsm, out, 2);
  Feeder f;
  f.run(p, ("\r\n" + cmglHeader(1, "+491701111111", kScts) + "first\r\n\r\nOK\r\n").c_str());
  TEST_ASSERT_EQUAL(1, p.count());
  p.reset(Charset::Gsm, out, 2);
  TEST_ASSERT_FALSE(p.finished());
  TEST_ASSERT_EQUAL(0, p.count());
  f.run(p, "\r\nOK\r\n");
  TEST_ASSERT_TRUE(p.ok());
  TEST_ASSERT_EQUAL(0, p.count());
}

// After the final line, what comes is no part of the answer any more.
void test_lines_after_the_end_of_an_answer_are_unsolicited() {
  ParsedSms out[1];
  SmsResponseParser p(Charset::Gsm, out, 1);
  TEST_ASSERT_TRUE(p.feedLine("OK") == Feed::Done);
  TEST_ASSERT_TRUE(p.feedLine("RING") == Feed::Unsolicited);
  TEST_ASSERT_TRUE(p.feedLine("+CMGL: 1,\"REC UNREAD\",\"+491701111111\",\"\",\"\"") == Feed::Unsolicited);
  TEST_ASSERT_EQUAL(0, p.count());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_line_reader_splits_crlf_lines);
  RUN_TEST(test_line_reader_flags_an_overlong_line_and_recovers);
  RUN_TEST(test_line_reader_prompt_only_in_prompt_mode);
  RUN_TEST(test_a_line_that_arrives_in_pieces_is_one_line);
  RUN_TEST(test_a_nul_byte_is_dropped_and_does_not_end_the_line);
  RUN_TEST(test_reset_forgets_a_partial_line);
  RUN_TEST(test_a_deadline_expires_at_its_time_and_not_before);
  RUN_TEST(test_a_deadline_survives_the_wrap_of_the_clock);
  RUN_TEST(test_classify_matches_whole_lines_only);
  RUN_TEST(test_classify_knows_the_lines_the_driver_reads);
  RUN_TEST(test_parse_clip_gsm_and_ucs2);
  RUN_TEST(test_parse_clip_refuses_an_overlong_or_missing_number);
  RUN_TEST(test_parse_cmti);
  RUN_TEST(test_voice_call_end_gives_the_duration_in_all_its_forms);
  RUN_TEST(test_csq_and_bars);
  RUN_TEST(test_creg_tells_registered_and_sms_only);
  RUN_TEST(test_cireg_tells_whether_the_ims_is_registered);
  RUN_TEST(test_cgdcont_gives_the_context_type_and_apn);
  RUN_TEST(test_cgdcont_refuses_a_field_that_does_not_fit);
  RUN_TEST(test_clcc_gives_index_direction_state_and_number);
  RUN_TEST(test_clcc_without_a_number_field_is_still_a_call);
  RUN_TEST(test_cpin_and_the_missing_card);
  RUN_TEST(test_the_operator_name_comes_out_of_cops);
  RUN_TEST(test_the_first_operator_code_of_a_scan_list);
  RUN_TEST(test_operator_codes_become_names_for_the_usual_countries);
  RUN_TEST(test_scts_applies_the_time_zone);
  RUN_TEST(test_scts_checks_the_day_against_the_month);
  RUN_TEST(test_cclk_gives_the_time_and_the_zone);
  RUN_TEST(test_cclk_refuses_the_clock_of_a_modem_without_network_time);
  RUN_TEST(test_a_dial_string_is_digits_star_hash_and_a_leading_plus);
  RUN_TEST(test_cmgs_command_valid_and_rejected_numbers);
  RUN_TEST(test_cmgl_single_message_gsm);
  RUN_TEST(test_cmgl_three_messages_all_parsed_with_indices);
  RUN_TEST(test_cmgl_ten_full_length_messages);
  RUN_TEST(test_cmgl_multiline_body_gsm);
  RUN_TEST(test_cmgl_ucs2_umlauts_emoji_newline);
  RUN_TEST(test_cmgl_ucs2_urc_between_messages_is_unsolicited);
  RUN_TEST(test_cmgl_truncated_response_keeps_the_last_message_on_the_sim);
  RUN_TEST(test_cmgl_capacity_exceeded_counts_dropped);
  RUN_TEST(test_cmgl_empty_list_and_error);
  RUN_TEST(test_cmgl_invalid_sender_is_still_delivered_for_deletion);
  RUN_TEST(test_cmgl_overlong_body_is_cut_and_flagged);
  RUN_TEST(test_a_gsm_body_is_cut_at_a_character_boundary);
  RUN_TEST(test_a_ucs2_body_that_does_not_fit_is_cut_and_flagged);
  RUN_TEST(test_a_long_ucs2_text_arrives_whole);
  RUN_TEST(test_spaces_after_the_commas_of_a_header_do_no_harm);
  RUN_TEST(test_cmgr_single_message_ucs2);
  RUN_TEST(test_a_parser_is_reusable_for_the_next_answer);
  RUN_TEST(test_lines_after_the_end_of_an_answer_are_unsolicited);
  return UNITY_END();
}
