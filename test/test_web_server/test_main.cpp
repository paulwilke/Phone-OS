// Host tests for the web server: accepting connections, reading the request,
// responding and closing — against a fake rather than a real network.
#include <unity.h>

#include <cstring>
#include <deque>
#include <string>

#include <web/WebServer.h>
#include <hal/TcpServer.h>

namespace {

// A connection whose input and output are plain strings. The test adds bytes
// with fromPeer() and reads back what the software sent via sentToPeer().
// peerClose() makes read() return -1 (remote end gone). limitWritesTo() caps
// how many bytes write() accepts per call.
struct FakeConn : TcpConnection {
  std::string inbox;
  size_t pos = 0;
  std::string outbox;
  bool peerGone = false;
  size_t writeLimit = 0;  // 0 = unlimited
  bool closed = false;

  int read(uint8_t* buf, size_t size) override {
    if (peerGone) return -1;
    if (pos >= inbox.size()) return 0;
    size_t n = inbox.size() - pos;
    if (n > size) n = size;
    memcpy(buf, inbox.data() + pos, n);
    pos += n;
    return (int)n;
  }
  size_t write(const uint8_t* d, size_t len) override {
    if (writeLimit && len > writeLimit) len = writeLimit;
    outbox.append((const char*)d, len);
    return len;
  }
  void close() override { closed = true; }

  void fromPeer(const std::string& bytes) { inbox += bytes; }
};

struct FakeTcp : TcpServer {
  std::deque<FakeConn*> waiting;
  bool listening = false;
  uint16_t port = 0;
  bool begin(uint16_t p) override { listening = true; port = p; return true; }
  void stop() override { listening = false; }
  TcpConnection* accept() override {
    if (waiting.empty()) return nullptr;
    FakeConn* c = waiting.front();
    waiting.pop_front();
    return c;
  }
};

// A handler stub that returns a page for successful requests and records errors.
struct StubHandler : web::RequestHandler {
  std::string page = "<hi/>";
  std::string lastBody;
  int lastError = 0;

  void handle(const http::Request& req, http::Response& res, uint32_t) override {
    lastBody = req.body ? std::string(req.body, req.contentLength) : "";
    res.status = 200;
    res.body = page.c_str();
    res.length = page.size();
    res.contentType = "text/html; charset=utf-8";
  }

  void error(int status, http::Response& res) override {
    lastError = status;
    res.status = status;
    res.body = "";
    res.length = 0;
  }
};

struct Fixture {
  FakeTcp tcp;
  StubHandler handler;
  web::WebServer server{tcp, handler};

  void run(uint32_t from = 0, int rounds = 400, uint32_t step = 0) {
    for (int i = 0; i < rounds; ++i) server.poll(from + (step ? (uint32_t)i * step : 0));
  }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_serves_the_page_in_pieces_and_closes() {
  Fixture f;
  f.server.begin();
  TEST_ASSERT_TRUE(f.tcp.listening);
  TEST_ASSERT_EQUAL(80, f.tcp.port);
  FakeConn c;
  c.fromPeer("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
  f.tcp.waiting.push_back(&c);
  f.run();
  TEST_ASSERT_TRUE(c.outbox.rfind("HTTP/1.1 200 OK", 0) == 0);
  TEST_ASSERT_TRUE(c.outbox.find("<hi/>") != std::string::npos);
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_EQUAL(1, f.server.served());
}

void test_a_slow_reader_gets_the_whole_answer() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET / HTTP/1.1\r\n\r\n");
  c.writeLimit = 10;  // network stack only accepts small chunks
  f.tcp.waiting.push_back(&c);
  f.run();
  TEST_ASSERT_TRUE(c.outbox.find("<hi/>") != std::string::npos);
  TEST_ASSERT_TRUE(c.closed);
}

// The web page is larger than any response buffer (on the device the simulator
// returned "response too large"). It is not copied but sent from its own storage
// in chunks.
void test_a_page_larger_than_any_buffer_arrives_intact() {
  Fixture f;
  f.handler.page = "<!doctype html>";
  for (int i = 0; i < 3000; ++i) f.handler.page += "0123456789abcdef";  // 48 KB
  f.handler.page += "</html>";
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET / HTTP/1.1\r\n\r\n");
  c.writeLimit = 1460;  // one TCP segment
  f.tcp.waiting.push_back(&c);
  f.run(0, 3000);
  const size_t sep = c.outbox.find("\r\n\r\n");
  TEST_ASSERT_TRUE(sep != std::string::npos);
  TEST_ASSERT_TRUE(c.outbox.substr(0, 12) == "HTTP/1.1 200");
  TEST_ASSERT_TRUE(c.outbox.substr(sep + 4) == f.handler.page);
  TEST_ASSERT_TRUE(c.closed);
}

// The server passes the request body to the handler intact.
void test_request_body_reaches_the_handler() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  const std::string body = "hello";
  c.fromPeer("PUT /api/settings HTTP/1.1\r\n"
             "X-Phoneplus: 1\r\n"
             "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
  f.tcp.waiting.push_back(&c);
  f.run();
  TEST_ASSERT_TRUE(c.outbox.rfind("HTTP/1.1 200", 0) == 0);
  TEST_ASSERT_EQUAL_STRING("hello", f.handler.lastBody.c_str());
}

void test_connections_are_served_one_after_another() {
  Fixture f;
  f.server.begin();
  FakeConn a, b;
  a.fromPeer("GET / HTTP/1.1\r\n\r\n");
  b.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&a);
  f.tcp.waiting.push_back(&b);
  f.run();
  TEST_ASSERT_TRUE(a.closed && b.closed);
  TEST_ASSERT_EQUAL(2, f.server.served());
}

void test_garbage_gets_a_400_and_the_server_goes_on() {
  Fixture f;
  f.server.begin();
  FakeConn bad, good;
  bad.fromPeer("\x16\x03\x01\x02garbage\r\n\r\n");
  good.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&bad);
  f.tcp.waiting.push_back(&good);
  f.run();
  TEST_ASSERT_TRUE(bad.outbox.rfind("HTTP/1.1 400", 0) == 0);
  TEST_ASSERT_TRUE(good.outbox.rfind("HTTP/1.1 200", 0) == 0);
}

void test_oversized_request_is_refused_without_reading_it_all() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  c.fromPeer("POST /api/settings HTTP/1.1\r\nContent-Length: 100000\r\n\r\n");
  f.tcp.waiting.push_back(&c);
  f.run();
  TEST_ASSERT_TRUE(c.outbox.rfind("HTTP/1.1 413", 0) == 0);
}

// A browser that opens a connection and goes silent must not lock everyone out.
void test_a_silent_connection_is_dropped_after_the_idle_time() {
  Fixture f;
  f.server.begin();
  FakeConn quiet, next;
  // quiet sends nothing
  next.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&quiet);
  f.tcp.waiting.push_back(&next);
  f.server.poll(0);
  f.server.poll(web::WebServer::kIdleSeconds - 1);
  TEST_ASSERT_FALSE(quiet.closed);
  f.server.poll(web::WebServer::kIdleSeconds);
  TEST_ASSERT_TRUE(quiet.closed);
  f.run(web::WebServer::kIdleSeconds);
  TEST_ASSERT_TRUE(next.outbox.find("<hi/>") != std::string::npos);
}

void test_vanished_client_is_cleaned_up() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  c.peerGone = true;  // peer gone before any data
  f.tcp.waiting.push_back(&c);
  f.run(0, 5);
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_EQUAL(0, f.server.served());
}

void test_stop_closes_the_connection_and_the_port() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  // no fromPeer → silent, stays in Reading phase
  f.tcp.waiting.push_back(&c);
  f.server.poll(0);
  f.server.stop();
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_FALSE(f.tcp.listening);
  // after stop, new connections are not accepted
  FakeConn late;
  late.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&late);
  f.run(0, 20);
  TEST_ASSERT_EQUAL_STRING("", late.outbox.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_serves_the_page_in_pieces_and_closes);
  RUN_TEST(test_a_slow_reader_gets_the_whole_answer);
  RUN_TEST(test_a_page_larger_than_any_buffer_arrives_intact);
  RUN_TEST(test_request_body_reaches_the_handler);
  RUN_TEST(test_connections_are_served_one_after_another);
  RUN_TEST(test_garbage_gets_a_400_and_the_server_goes_on);
  RUN_TEST(test_oversized_request_is_refused_without_reading_it_all);
  RUN_TEST(test_a_silent_connection_is_dropped_after_the_idle_time);
  RUN_TEST(test_vanished_client_is_cleaned_up);
  RUN_TEST(test_stop_closes_the_connection_and_the_port);
  return UNITY_END();
}
