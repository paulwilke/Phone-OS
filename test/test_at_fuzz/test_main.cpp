// Fuzz-style test for the AT layer: random bytes, truncated and damaged valid answers go through the
// line reader, the classifier, every parser and the SMS answer parser, and through the UCS-2 conversion.
// Nothing may crash, read outside a buffer or leave a string unterminated. Run it under AddressSanitizer
// and UndefinedBehaviorSanitizer (pio test -e native-asan -f test_at_fuzz) to catch what a plain run
// does not show. Seed and counts are fixed, so a failure is reproducible.

#include <unity.h>

#include <cstring>
#include <string>

#include <drivers/AtParser.h>
#include <drivers/SmsPdu.h>
#include <drivers/Ucs2.h>

using namespace at;

void setUp() {}
void tearDown() {}

namespace {

struct Lcg {
  uint32_t state;
  explicit Lcg(uint32_t seed) : state(seed) {}
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state;
  }
  uint8_t byte() { return (uint8_t)(next() >> 16); }
  uint32_t below(uint32_t n) { return (next() >> 8) % n; }
};

// Every parser on one line, both charsets; the results do not matter, only that nothing breaks.
void throwAtParsers(const char* line) {
  char phone[kPhoneSize], name[24], code[8];
  int i = 0;
  uint32_t u = 0;
  Registration reg;
  ImsRegistration ims;
  PdpContext pdp;
  CallListEntry call;
  SimStatus sim = SimStatus::Unknown;
  NetworkTime nt;
  (void)classify(line);
  for (Charset cs : {Charset::Gsm, Charset::Ucs2}) {
    parseClip(line, cs, phone, sizeof(phone));
    parseClcc(line, cs, &call);
    TEST_ASSERT_TRUE(strlen(phone) < kPhoneSize);
    TEST_ASSERT_TRUE(strlen(call.phone) < kPhoneSize);
  }
  parseCmti(line, &i);
  parseVoiceCallEnd(line, &u);
  parseCsq(line, &i);
  parseCreg(line, &reg);
  parseCireg(line, &ims);
  parseCgdcont(line, &pdp);
  TEST_ASSERT_TRUE(strlen(pdp.type) < sizeof(pdp.type) && strlen(pdp.apn) < sizeof(pdp.apn));
  parseCpin(line, &sim);
  isSimNotInserted(line);
  parseCopsName(line, name, sizeof(name));
  TEST_ASSERT_TRUE(strlen(name) < sizeof(name));
  parseFirstOperatorCode(line, code, sizeof(code));
  parseCclk(line, &nt);
  parseScts(line, &u);
  (void)operatorNameForCode(line);
}

// A whole stream through the reader into the parsers and the SMS parser.
void throwAtStream(const std::string& stream, Charset cs) {
  LineReader reader;
  ParsedSms out[3];
  SmsResponseParser sms(cs, out, 3);
  for (char c : stream) {
    if (!reader.feed(c)) continue;
    TEST_ASSERT_TRUE(strlen(reader.line()) <= LineReader::kMaxLine);
    throwAtParsers(reader.line());
    sms.feedLine(reader.line());
  }
  TEST_ASSERT_TRUE(sms.count() <= 3);
  for (size_t k = 0; k < sms.count(); ++k) {
    TEST_ASSERT_TRUE(out[k].msg.body.size() <= kMaxSmsBytes);
    TEST_ASSERT_TRUE(out[k].msg.phone.size() <= kMaxPhoneChars);
  }
}

const char* kValidStreams[] = {
    "\r\n+CMGL: 1,\"REC UNREAD\",\"+491701111111\",\"\",\"26/09/15,14:32:10+08\"\r\nhello\r\n\r\nOK\r\n",
    ("\r\n+CMGL: 2,\"REC UNREAD\",\"002B003400390031003700300031\",\"\",\"26/09/15,14:32:10+08\"\r\n"
     "00480069D83DDE00\r\n\r\nOK\r\n"),
    "\r\n+CGDCONT: 1,\"IPV4V6\",\"internet.example\",\"0.0.0.0\",0,0\r\n+CGDCONT: 8,\"IPV4V6\",\"ims\"\r\n\r\nOK\r\n",
    "\r\n+CIREG: 1,1,15\r\n\r\nOK\r\n+CREG: 0,1\r\n+COPS: 0,0,\"26201\",7\r\n+CCLK: \"26/09/15,14:32:10+08\"\r\n",
    "\r\n+CLCC: 1,0,2,0,0,\"+491701111111\",145\r\n\r\nOK\r\nVOICE CALL: END: 000032\r\n*ATREADY: 1\r\n",
    "\r\nRING\r\n\r\n+CLIP: \"+491702222222\",145,,,,0\r\n+CMTI: \"SM\",7\r\n+CSQ: 23,99\r\n+CPIN: SIM PIN\r\n",
};

}  // namespace

void test_random_bytes_never_break_the_reader_or_the_parsers() {
  for (uint32_t seed = 1; seed <= 300; ++seed) {
    Lcg rng(seed);
    std::string stream;
    const size_t len = rng.below(4000);
    for (size_t i = 0; i < len; ++i) stream += (char)rng.byte();
    throwAtStream(stream, seed % 2 ? Charset::Gsm : Charset::Ucs2);
  }
}

// Random bytes that look more like the protocol: the characters the answers are made of.
void test_random_protocol_characters_never_break_anything() {
  const char alphabet[] = "+:,\"\r\n 0123456789ABCDEFabcdef/-*#><=?CMGLRSPINOK";
  for (uint32_t seed = 1; seed <= 300; ++seed) {
    Lcg rng(seed * 7919u);
    std::string stream;
    const size_t len = rng.below(3000);
    for (size_t i = 0; i < len; ++i) stream += alphabet[rng.below(sizeof(alphabet) - 1)];
    throwAtStream(stream, seed % 2 ? Charset::Gsm : Charset::Ucs2);
  }
}

void test_every_prefix_of_a_valid_stream_is_harmless() {
  for (const char* valid : kValidStreams) {
    const std::string s(valid);
    for (size_t cut = 0; cut <= s.size(); ++cut) {
      throwAtStream(s.substr(0, cut), Charset::Gsm);
      throwAtStream(s.substr(0, cut), Charset::Ucs2);
    }
  }
}

void test_a_valid_stream_with_damaged_bytes_is_harmless() {
  Lcg rng(4242);
  for (int round = 0; round < 2000; ++round) {
    std::string s(kValidStreams[rng.below(sizeof(kValidStreams) / sizeof(kValidStreams[0]))]);
    const uint32_t damage = 1 + rng.below(6);
    for (uint32_t k = 0; k < damage && !s.empty(); ++k) {
      const size_t at = rng.below((uint32_t)s.size());
      switch (rng.below(3)) {
        case 0: s[at] = (char)rng.byte(); break;
        case 1: s.erase(at, 1 + rng.below(8)); break;
        default: s.insert(at, 1 + rng.below(8), (char)rng.byte()); break;
      }
    }
    throwAtStream(s, round % 2 ? Charset::Gsm : Charset::Ucs2);
  }
}

// One huge line, and one longer than every buffer: cut, flagged, never past the end.
void test_lines_longer_than_every_buffer_are_cut_safely() {
  for (size_t len : {LineReader::kMaxLine - 1, LineReader::kMaxLine, LineReader::kMaxLine + 1, (size_t)20000}) {
    throwAtStream("+CMGL: 1,\"REC UNREAD\",\"" + std::string(len, '1') + "\",\"\",\"" + std::string(len, '2') + "\"\r\n" +
                      std::string(len, 'A') + "\r\n\r\nOK\r\n",
                  Charset::Ucs2);
    throwAtStream("+CGDCONT: 1,\"" + std::string(len, 'T') + "\",\"" + std::string(len, 'A') + "\"\r\n", Charset::Gsm);
    throwAtStream("+COPS: 0,0,\"" + std::string(len, 'N') + "\",7\r\n", Charset::Gsm);
  }
}

// Fields that are just at, and just beyond, the size of the buffer they are copied into.
void test_fields_at_the_edge_of_their_buffers_are_handled() {
  for (size_t len : {(size_t)0, (size_t)1, (size_t)7, (size_t)8, (size_t)11, (size_t)12, (size_t)23, (size_t)24, (size_t)25, (size_t)100,
                     (size_t)101, (size_t)102, (size_t)500}) {
    throwAtStream("+COPS: 0,0,\"" + std::string(len, 'N') + "\",7\r\n", Charset::Gsm);
    throwAtStream("+CGDCONT: 1,\"" + std::string(len, 'T') + "\",\"" + std::string(len, 'A') + "\"\r\n", Charset::Gsm);
    throwAtStream("+CLIP: \"" + std::string(len, '1') + "\",145\r\n", Charset::Gsm);
    throwAtStream("+CLCC: 1,0,2,0,0,\"" + std::string(len, '1') + "\",145\r\n", Charset::Gsm);
    throwAtStream("+COPS: (2,\"a\",\"b\",\"" + std::string(len, '2') + "\",7)\r\n", Charset::Gsm);
  }
}

void test_the_ucs2_conversion_survives_random_input_and_any_buffer_size() {
  Lcg rng(77);
  for (int round = 0; round < 3000; ++round) {
    std::string in;
    const size_t len = rng.below(80);
    const bool hexy = rng.below(2);
    for (size_t i = 0; i < len; ++i) in += hexy ? "0123456789ABCDEFabcdefGZ"[rng.below(24)] : (char)rng.byte();
    char out[96];
    const size_t room = rng.below(sizeof(out)) + 1;
    bool truncated = false;
    text::ucs2HexToUtf8(in.data(), in.size(), out, room, &truncated);
    TEST_ASSERT_TRUE(strlen(out) < room);
    const int units = text::utf8ToUcs2Hex(in.c_str(), out, room);
    TEST_ASSERT_TRUE(strlen(out) < room);
    if (units >= 0) TEST_ASSERT_EQUAL((size_t)units * 4, strlen(out));
  }
}

// A PDU is built from a number and a text that anybody can type: whatever they are, the result is either refused
// or hex digits of an even length whose TPDU length is the number the modem is told.
void test_encoding_random_numbers_and_texts_never_overruns_and_is_always_well_formed() {
  Lcg rng(31337);
  static const char* const pieces[] = {"a", "Z", " ", "\n", "{", "|", "\xC3\xA4", "\xC3\xB6", "\xE2\x82\xAC", "\xD0\xB6", "\xF0\x9F\x98\x80",
                                       "\xC3", "\x80", "\xED\xA0\x80", "\x1A", "\x1B"};
  for (int round = 0; round < 3000; ++round) {
    std::string number;
    const size_t digits = rng.below(30);
    for (size_t i = 0; i < digits; ++i) number += "+0123456789*# \"A"[rng.below(16)];
    std::string text;
    const size_t len = rng.below(500);
    for (size_t i = 0; i < len; ++i) text += pieces[rng.below(sizeof(pieces) / sizeof(pieces[0]))];
    sms::Coding coding;
    std::vector<sms::Part> parts;
    const bool ok = sms::encode(number, text, (uint8_t)rng.below(256), coding, parts);
    if (!ok) {
      TEST_ASSERT_TRUE(parts.empty());
      continue;
    }
    TEST_ASSERT_TRUE(!parts.empty() && parts.size() <= 255);
    for (const sms::Part& part : parts) {
      TEST_ASSERT_TRUE(part.hex.size() % 2 == 0);
      TEST_ASSERT_EQUAL((int)part.hex.size() / 2 - 1, part.tpduLength);
      TEST_ASSERT_TRUE(part.tpduLength <= 175);
      for (char c : part.hex) TEST_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'));
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_random_bytes_never_break_the_reader_or_the_parsers);
  RUN_TEST(test_random_protocol_characters_never_break_anything);
  RUN_TEST(test_every_prefix_of_a_valid_stream_is_harmless);
  RUN_TEST(test_a_valid_stream_with_damaged_bytes_is_harmless);
  RUN_TEST(test_lines_longer_than_every_buffer_are_cut_safely);
  RUN_TEST(test_fields_at_the_edge_of_their_buffers_are_handled);
  RUN_TEST(test_the_ucs2_conversion_survives_random_input_and_any_buffer_size);
  RUN_TEST(test_encoding_random_numbers_and_texts_never_overruns_and_is_always_well_formed);
  return UNITY_END();
}
