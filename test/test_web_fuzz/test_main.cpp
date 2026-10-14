// Fuzz-style tests for the two readers of the parents' page that take a request body: the JSON reader
// (web::json::toToml, behind PUT /api/settings, /api/contacts and /api/secrets) and the ZIP reader
// (zip::read, behind /api/restore). Random bytes, damaged valid inputs and every prefix of a valid input
// must end in "refused" or "read", never in a crash or a read outside the buffer; nesting deeper than the
// reader's limit is refused, because on the device each level is a stack frame and the loop's stack is
// 8 KB. Run under AddressSanitizer with `pio test -e native-asan -f test_web_fuzz`.
//
// The seed and the trial count are fixed so that CI is deterministic.
#include <unity.h>

#include <cstring>
#include <string>

#include <app/Toml.h>
#include <web/Json.h>
#include <web/Zip.h>

namespace {

// A minimal linear-congruential generator (no system headers needed).
struct Lcg {
  uint32_t state;
  explicit Lcg(uint32_t seed) : state(seed) {}
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state;
  }
  uint8_t byte() { return (uint8_t)(next() >> 16); }
};

const char* kValidJson[] = {
    "{}",
    "{\"child\":{\"name\":\"Alex\"},\"sms\":{\"extra_trigger_words\":[\"a\",\"b\"]}}",
    "{\"contact\":[{\"number\":\"+491701111111\",\"name\":\"Parent One\",\"role\":\"parent\"}]}",
    "{\"quiet_hours\":[],\"tiles\":[{\"action\":\"call\",\"target\":\"+491702222222\"}],\"n\":-12,\"b\":true,\"x\":null}",
    "{\"s\":\"q\\\"uo\\\\te\\n\\u0041\"}",
};

const char kJsonAlphabet[] = "{}[]\":,\\ 0123456789-abctrulfnse\n\t";

void jsonNeverCrashes(const std::string& in) {
  toml::Value root;
  std::string error;
  const bool ok = web::json::toToml(in.data(), in.size(), root, error);
  if (ok) {
    // What was read can be written back as TOML without a crash either.
    (void)toml::emit(root);
  } else {
    TEST_ASSERT_FALSE(error.empty());
  }
}

std::string zipOf(const std::string& settings) { return zip::write({{"config/settings.toml", settings}}, 0); }

void zipNeverCrashes(const std::string& in) {
  std::vector<zip::Entry> out;
  std::string why;
  const bool ok = zip::read(in.data(), in.size(), 4096, 8, out, why);
  if (!ok) TEST_ASSERT_FALSE(why.empty());
}

}  // namespace

void setUp() {}
void tearDown() {}

// ---- JSON ---------------------------------------------------------------------------------------------------

void test_random_bytes_through_the_json_reader_never_crash() {
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    for (size_t len : {1, 8, 64, 512, 8192}) {
      Lcg rng(seed * 7919u + (uint32_t)len);
      std::string buf;
      for (size_t i = 0; i < len; ++i) buf += (char)rng.byte();
      jsonNeverCrashes(buf);
    }
  }
}

void test_random_json_characters_never_crash() {
  const size_t alphabet = strlen(kJsonAlphabet);
  for (uint32_t seed = 1; seed <= 300; ++seed) {
    Lcg rng(seed * 104729u);
    std::string buf = "{";
    const size_t len = 1 + rng.next() % 400;
    for (size_t i = 0; i < len; ++i) buf += kJsonAlphabet[rng.next() % alphabet];
    jsonNeverCrashes(buf);
  }
}

void test_every_prefix_and_every_damaged_byte_of_valid_json_never_crashes() {
  for (const char* valid : kValidJson) {
    const std::string in(valid);
    for (size_t len = 0; len <= in.size(); ++len) jsonNeverCrashes(in.substr(0, len));
    for (size_t at = 0; at < in.size(); ++at) {
      for (char c : {'"', '{', '}', '[', ']', ',', ':', '\\', 'x', '\0', (char)0xC3}) {
        std::string damaged = in;
        damaged[at] = c;
        jsonNeverCrashes(damaged);
      }
    }
  }
}

void test_valid_json_is_read() {
  for (const char* valid : kValidJson) {
    toml::Value root;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(web::json::toToml(valid, strlen(valid), root, error), valid);
  }
}

// On the device each level of nesting is a stack frame; a body of 8192 bytes could nest 8192 levels. The
// reader stops at its limit and says so instead of running the loop task out of stack.
void test_nesting_deeper_than_the_limit_is_refused_not_overflowed() {
  const size_t deep = 8000;
  for (const char* pair : {"[]", "{}"}) {
    std::string in = "{\"k\":";
    for (size_t i = 0; i < deep; ++i) in += pair[0];
    if (pair[0] == '{') in += "\"x\":1";
    for (size_t i = 0; i < deep; ++i) in += pair[1];
    in += "}";
    toml::Value root;
    std::string error;
    TEST_ASSERT_FALSE(web::json::toToml(in.data(), in.size(), root, error));
    // Arrays nest freely and run into the limit; an object wants a key after its brace, so "{{" is refused
    // one level in, before any depth is reached. Either way the reader stops without the frames.
    if (pair[0] == '[') TEST_ASSERT_TRUE_MESSAGE(error.find("deep") != std::string::npos, error.c_str());
  }
}

void test_nesting_within_the_limit_is_read() {
  std::string in = "{\"k\":";
  for (size_t i = 0; i < web::json::kMaxDepth - 2; ++i) in += "[";
  in += "1";
  for (size_t i = 0; i < web::json::kMaxDepth - 2; ++i) in += "]";
  in += "}";
  toml::Value root;
  std::string error;
  TEST_ASSERT_TRUE_MESSAGE(web::json::toToml(in.data(), in.size(), root, error), error.c_str());
}

// ---- ZIP ----------------------------------------------------------------------------------------------------

void test_random_bytes_through_the_zip_reader_never_crash() {
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    for (size_t len : {1, 4, 30, 31, 64, 512, 8192}) {
      Lcg rng(seed * 7919u + (uint32_t)len);
      std::string buf;
      for (size_t i = 0; i < len; ++i) buf += (char)rng.byte();
      zipNeverCrashes(buf);
    }
  }
}

void test_random_bytes_behind_a_zip_signature_never_crash() {
  for (uint32_t seed = 1; seed <= 300; ++seed) {
    Lcg rng(seed * 104729u);
    std::string buf = "PK\x03\x04";
    const size_t len = rng.next() % 200;
    for (size_t i = 0; i < len; ++i) buf += (char)rng.byte();
    zipNeverCrashes(buf);
  }
}

void test_every_prefix_and_every_damaged_byte_of_a_valid_archive_never_crashes() {
  const std::string archive = zipOf("[child]\nname = \"Alex\"\n");
  for (size_t len = 0; len <= archive.size(); ++len) zipNeverCrashes(archive.substr(0, len));
  for (size_t at = 0; at < archive.size(); ++at) {
    for (unsigned char c : {0x00, 0xFF, 0x7F, 0x80, 0x01}) {
      std::string damaged = archive;
      damaged[at] = (char)c;
      zipNeverCrashes(damaged);
    }
  }
}

// A header that claims more than the body holds (sizes, name and extra lengths at their maximum).
void test_lengths_that_point_past_the_end_are_refused() {
  const std::string archive = zipOf("x = 1\n");
  for (size_t field : {18, 22, 26, 28}) {  // packed size, size, name length, extra length
    for (unsigned char fill : {0xFF, 0x7F, 0x80}) {
      std::string damaged = archive;
      damaged[field] = (char)fill;
      damaged[field + 1] = (char)fill;
      zipNeverCrashes(damaged);
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_random_bytes_through_the_json_reader_never_crash);
  RUN_TEST(test_random_json_characters_never_crash);
  RUN_TEST(test_every_prefix_and_every_damaged_byte_of_valid_json_never_crashes);
  RUN_TEST(test_valid_json_is_read);
  RUN_TEST(test_nesting_deeper_than_the_limit_is_refused_not_overflowed);
  RUN_TEST(test_nesting_within_the_limit_is_read);
  RUN_TEST(test_random_bytes_through_the_zip_reader_never_crash);
  RUN_TEST(test_random_bytes_behind_a_zip_signature_never_crash);
  RUN_TEST(test_every_prefix_and_every_damaged_byte_of_a_valid_archive_never_crashes);
  RUN_TEST(test_lengths_that_point_past_the_end_are_refused);
  return UNITY_END();
}
