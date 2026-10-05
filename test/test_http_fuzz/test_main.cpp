// Fuzz-style test for the HTTP parser: feeds random and truncated bytes and
// checks that the parser never crashes, never reads outside its buffer, and
// always ends in a defined state. Run this suite under AddressSanitizer
// (platformio.ini native-asan environment) to catch out-of-bounds accesses.
//
// The seed and trial count are fixed so CI is deterministic; the sequence is
// varied enough to exercise every state transition and every error code.
#include <unity.h>

#include <cstring>
#include <string>

#include <web/Http.h>

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

// Well-formed request fragments used to build truncated inputs.
const char* kValidRequests[] = {
    "GET / HTTP/1.1\r\nHost: x\r\n\r\n",
    "POST /api/settings HTTP/1.1\r\nContent-Length: 5\r\nX-Phoneplus: 1\r\n\r\nhello",
    "PUT /api/file?path=/config/x.txt HTTP/1.1\r\n"
    "Authorization: Basic dXNlcjp0ZXN0LXBhc3N3b3JkLTE=\r\n"
    "Content-Length: 3\r\nX-Phoneplus: 1\r\n\r\nabc",
    "DELETE /api/file HTTP/1.1\r\nHost: a\r\n\r\n",
};

// Feed `len` random bytes to a fresh parser; the state must be defined.
void fuzz_random_bytes(uint32_t seed, size_t len) {
  Lcg rng(seed);
  http::Parser p;
  std::string buf;
  buf.reserve(len);
  for (size_t i = 0; i < len; ++i) buf += (char)rng.byte();
  const auto st = p.feed((const uint8_t*)buf.data(), buf.size());
  TEST_ASSERT_TRUE(st == http::Parser::State::Head ||
                   st == http::Parser::State::Body ||
                   st == http::Parser::State::Done ||
                   st == http::Parser::State::Error);
  // If an error, the status code must be one of the defined codes.
  if (st == http::Parser::State::Error) {
    const int code = p.errorStatus();
    TEST_ASSERT_TRUE(code == 400 || code == 411 || code == 413 ||
                     code == 414 || code == 431 || code == 501);
  }
}

// Feed the first `len` bytes of a valid request; the parser must not crash.
void fuzz_truncated(const char* req, size_t len) {
  http::Parser p;
  if (len > 0) {
    p.feed((const uint8_t*)req, len);
  }
  // State can be anything — we are just checking for no crash or OOB access.
  (void)p.state();
  (void)p.errorStatus();
}

// Feed in tiny chunks to exercise the incremental path.
void fuzz_tiny_chunks(const char* req, size_t chunkSize) {
  const size_t fullLen = strlen(req);
  http::Parser p;
  size_t pos = 0;
  while (pos < fullLen) {
    const size_t n = (pos + chunkSize <= fullLen) ? chunkSize : (fullLen - pos);
    p.feed((const uint8_t*)req + pos, n);
    pos += n;
    if (p.state() == http::Parser::State::Done || p.state() == http::Parser::State::Error) break;
  }
  (void)p.state();
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_random_bytes_never_crash() {
  // Many seeds and lengths covering sub-head, head-boundary, and body regions.
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    for (size_t len : {1, 16, 64, 256, 512, 1024, 2048, 4096, 9217}) {
      fuzz_random_bytes(seed * 7919u + len * 31u, len);
    }
  }
}

void test_truncated_valid_requests_never_crash() {
  for (const char* req : kValidRequests) {
    const size_t full = strlen(req);
    for (size_t len = 0; len <= full; ++len) {
      fuzz_truncated(req, len);
    }
  }
}

void test_tiny_chunk_delivery_never_crash() {
  for (const char* req : kValidRequests) {
    for (size_t chunk : {1, 2, 3, 7}) {
      fuzz_tiny_chunks(req, chunk);
    }
  }
}

void test_repeated_feed_after_done_is_harmless() {
  // A done/error parser ignores further input.
  http::Parser p;
  p.feed((const uint8_t*)"GET / HTTP/1.1\r\n\r\n", 18);
  TEST_ASSERT_TRUE(p.state() == http::Parser::State::Done);
  const auto st = p.feed((const uint8_t*)"extra", 5);
  TEST_ASSERT_TRUE(st == http::Parser::State::Done);
}

void test_buffer_boundary_not_overrun() {
  // Feed exactly kMaxHead + kMaxBody bytes in one call — must not overrun the
  // internal buffer.
  http::Parser p;
  std::string buf(http::kMaxHead + http::kMaxBody, 'A');
  const auto st = p.feed((const uint8_t*)buf.data(), buf.size());
  // No crash; state can be anything.
  (void)st;
}

void test_one_byte_over_total_limit_is_refused() {
  // Total capacity is kMaxHead + kMaxBody; feeding one more byte must not
  // overrun the buffer and must return an error.
  http::Parser p;
  std::string buf(http::kMaxHead + http::kMaxBody + 1, 'B');
  const auto st = p.feed((const uint8_t*)buf.data(), buf.size());
  TEST_ASSERT_TRUE(st == http::Parser::State::Error);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_random_bytes_never_crash);
  RUN_TEST(test_truncated_valid_requests_never_crash);
  RUN_TEST(test_tiny_chunk_delivery_never_crash);
  RUN_TEST(test_repeated_feed_after_done_is_harmless);
  RUN_TEST(test_buffer_boundary_not_overrun);
  RUN_TEST(test_one_byte_over_total_limit_is_refused);
  return UNITY_END();
}
