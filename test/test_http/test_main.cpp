// Host tests for the HTTP parser and its helpers.
#include <unity.h>

#include <cstring>
#include <string>

#include <web/Http.h>

namespace {
http::Parser::State feedAll(http::Parser& p, const std::string& s) {
  return p.feed((const uint8_t*)s.data(), s.size());
}
}  // namespace

void setUp() {}
void tearDown() {}

void test_get_request() {
  http::Parser p;
  auto st = feedAll(p, "GET /api/state?x=1 HTTP/1.1\r\nHost: a\r\nAuthorization: Basic QUJD\r\n\r\n");
  TEST_ASSERT_TRUE(st == http::Parser::State::Done);
  TEST_ASSERT_EQUAL_STRING("GET", p.request().method);
  TEST_ASSERT_EQUAL_STRING("/api/state", p.request().path);
  TEST_ASSERT_EQUAL_STRING("x=1", p.request().query);
  TEST_ASSERT_EQUAL_STRING("Basic QUJD", p.request().authorization);
}

void test_post_with_body_arriving_in_pieces() {
  http::Parser p;
  TEST_ASSERT_TRUE(feedAll(p, "PUT /api/settings HTTP/1.1\r\nContent-Le") == http::Parser::State::Head);
  TEST_ASSERT_TRUE(feedAll(p, "ngth: 12\r\nX-Phoneplus: 1\r\n\r\n{\"pin\":") == http::Parser::State::Body);
  TEST_ASSERT_TRUE(feedAll(p, "\"12\"}") == http::Parser::State::Done);
  TEST_ASSERT_EQUAL_STRING("{\"pin\":\"12\"}", p.request().body);
  TEST_ASSERT_EQUAL_STRING("1", p.request().custom);
}

void test_header_names_are_case_insensitive() {
  http::Parser p;
  feedAll(p, "POST /x HTTP/1.1\r\ncontent-length: 2\r\nx-phoneplus: 1\r\n\r\nhi");
  TEST_ASSERT_TRUE(p.state() == http::Parser::State::Done);
  TEST_ASSERT_EQUAL_STRING("1", p.request().custom);
}

void test_oversized_body_is_rejected_before_it_is_read() {
  http::Parser p;
  feedAll(p, "POST /x HTTP/1.1\r\nContent-Length: 999999\r\n\r\n");
  TEST_ASSERT_TRUE(p.state() == http::Parser::State::Error);
  TEST_ASSERT_EQUAL(413, p.errorStatus());
}

void test_oversized_head_is_rejected() {
  http::Parser p;
  std::string s = "GET / HTTP/1.1\r\nX: ";
  s += std::string(2000, 'a');
  feedAll(p, s);
  TEST_ASSERT_TRUE(p.state() == http::Parser::State::Error);
  TEST_ASSERT_EQUAL(431, p.errorStatus());
}

void test_garbage_is_a_400() {
  http::Parser p;
  feedAll(p, "\x16\x03\x01 TLS?\r\n\r\n");
  TEST_ASSERT_TRUE(p.state() == http::Parser::State::Error);
  TEST_ASSERT_EQUAL(400, p.errorStatus());
}

void test_post_without_length_is_refused() {
  http::Parser p;
  feedAll(p, "POST /x HTTP/1.1\r\nHost: a\r\n\r\n");
  TEST_ASSERT_EQUAL(411, p.errorStatus());
}

void test_chunked_is_not_accepted() {
  http::Parser p;
  feedAll(p, "POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n");
  TEST_ASSERT_EQUAL(501, p.errorStatus());
}

void test_path_must_start_with_a_slash_and_fit() {
  http::Parser p;
  feedAll(p, "GET http://evil/ HTTP/1.1\r\n\r\n");
  TEST_ASSERT_EQUAL(400, p.errorStatus());
  http::Parser q;
  feedAll(q, "GET /" + std::string(300, 'a') + " HTTP/1.1\r\n\r\n");
  TEST_ASSERT_EQUAL(414, q.errorStatus());
}

void test_parser_can_be_reused() {
  http::Parser p;
  feedAll(p, "GET /a HTTP/1.1\r\n\r\n");
  p.reset();
  feedAll(p, "GET /b HTTP/1.1\r\n\r\n");
  TEST_ASSERT_EQUAL_STRING("/b", p.request().path);
}

void test_format_response() {
  http::Response r;
  r.status = 401;
  r.contentType = "application/json";
  r.body = "{}";
  r.length = 2;
  r.extraHeader = "WWW-Authenticate: Basic realm=\"x\"";
  char out[512];
  const size_t n = http::format(r, out, sizeof(out));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_NOT_NULL(strstr(out, "HTTP/1.1 401 Unauthorized"));
  TEST_ASSERT_NOT_NULL(strstr(out, "Content-Length: 2"));
  TEST_ASSERT_NOT_NULL(strstr(out, "Cache-Control: no-store"));
  TEST_ASSERT_NOT_NULL(strstr(out, "WWW-Authenticate"));
  TEST_ASSERT_NOT_NULL(strstr(out, "Connection: close"));
  TEST_ASSERT_EQUAL_STRING("{}", out + n - 2);
  // No CORS: foreign pages must not read the response.
  TEST_ASSERT_NULL(strstr(out, "Access-Control"));
}

void test_format_refuses_what_does_not_fit() {
  http::Response r;
  r.body = "0123456789";
  r.length = 10;
  char out[40];
  TEST_ASSERT_EQUAL(0, (int)http::format(r, out, sizeof(out)));
}

void test_basic_password() {
  char pw[64];
  // "parent:test-password-1" -> cGFyZW50OnRlc3QtcGFzc3dvcmQtMQ==
  TEST_ASSERT_TRUE(http::basicPassword("Basic cGFyZW50OnRlc3QtcGFzc3dvcmQtMQ==", pw, sizeof(pw)));
  TEST_ASSERT_EQUAL_STRING("test-password-1", pw);
  // Colons in the password are kept: "u:a:b" -> password is "a:b"
  TEST_ASSERT_TRUE(http::basicPassword("Basic dTphOmI=", pw, sizeof(pw)));
  TEST_ASSERT_EQUAL_STRING("a:b", pw);
  TEST_ASSERT_FALSE(http::basicPassword("Bearer abc", pw, sizeof(pw)));
  TEST_ASSERT_FALSE(http::basicPassword("Basic !!!", pw, sizeof(pw)));
  TEST_ASSERT_FALSE(http::basicPassword("", pw, sizeof(pw)));
}

void test_constant_time_equals() {
  TEST_ASSERT_TRUE(http::constantTimeEquals("abc", "abc"));
  TEST_ASSERT_FALSE(http::constantTimeEquals("abc", "abd"));
  TEST_ASSERT_FALSE(http::constantTimeEquals("abc", "abcd"));
  TEST_ASSERT_FALSE(http::constantTimeEquals("", "x"));
  TEST_ASSERT_TRUE(http::constantTimeEquals("", ""));
}

// Counter-check: a body exactly at the limit is accepted; one byte over is not.
void test_body_exactly_at_limit_is_accepted() {
  http::Parser p;
  const std::string hdr = "POST /x HTTP/1.1\r\nContent-Length: " +
                           std::to_string(http::kMaxBody) + "\r\n\r\n";
  feedAll(p, hdr);
  TEST_ASSERT_TRUE(p.state() != http::Parser::State::Error);
  const std::string big(http::kMaxBody, 'x');
  feedAll(p, big);
  TEST_ASSERT_TRUE(p.state() == http::Parser::State::Done);
}

void test_body_one_byte_over_limit_is_refused() {
  http::Parser p;
  feedAll(p, "POST /x HTTP/1.1\r\nContent-Length: " + std::to_string(http::kMaxBody + 1) + "\r\n\r\n");
  TEST_ASSERT_EQUAL(413, p.errorStatus());
}

// Counter-check: a request line that fills the path buffer exactly is refused
// with 414, not accepted silently with truncation.
void test_path_at_exact_limit_is_refused() {
  http::Parser p;
  // sizeof(req_.path) == 120; path "/" + 119 chars = 120 chars total, which
  // equals sizeof - 1 (the limit check is >=).
  feedAll(p, "GET /" + std::string(119, 'a') + " HTTP/1.1\r\n\r\n");
  TEST_ASSERT_EQUAL(414, p.errorStatus());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_get_request);
  RUN_TEST(test_post_with_body_arriving_in_pieces);
  RUN_TEST(test_header_names_are_case_insensitive);
  RUN_TEST(test_oversized_body_is_rejected_before_it_is_read);
  RUN_TEST(test_oversized_head_is_rejected);
  RUN_TEST(test_garbage_is_a_400);
  RUN_TEST(test_post_without_length_is_refused);
  RUN_TEST(test_chunked_is_not_accepted);
  RUN_TEST(test_path_must_start_with_a_slash_and_fit);
  RUN_TEST(test_parser_can_be_reused);
  RUN_TEST(test_format_response);
  RUN_TEST(test_format_refuses_what_does_not_fit);
  RUN_TEST(test_basic_password);
  RUN_TEST(test_constant_time_equals);
  RUN_TEST(test_body_exactly_at_limit_is_accepted);
  RUN_TEST(test_body_one_byte_over_limit_is_refused);
  RUN_TEST(test_path_at_exact_limit_is_refused);
  return UNITY_END();
}
