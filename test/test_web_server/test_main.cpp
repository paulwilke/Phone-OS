// Host tests for the web server: accepting connections, reading the request,
// responding and closing — against a fake rather than a real network.
#include <unity.h>

#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

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
  // The send buffer of the stack: room bytes can be written, the rest has to wait until the peer reads
  // (drain()). -1 = unlimited. A write with no room gives 0, as WiFiClient::write does after it has
  // waited ten seconds for room.
  long room = -1;
  bool reset = false;      // the peer has reset the connection: writes give 0, reads give -1
  size_t unreadAtClose = 0;  // what was still waiting in the input when the software closed it: a close like that is a reset

  int read(uint8_t* buf, size_t size) override {
    if (peerGone || reset) return -1;
    if (pos >= inbox.size()) return 0;
    size_t n = inbox.size() - pos;
    if (n > size) n = size;
    memcpy(buf, inbox.data() + pos, n);
    pos += n;
    return (int)n;
  }
  size_t write(const uint8_t* d, size_t len) override {
    if (reset) return 0;
    if (writeLimit && len > writeLimit) len = writeLimit;
    if (room >= 0 && len > (size_t)room) len = (size_t)room;
    if (room >= 0) room -= (long)len;
    outbox.append((const char*)d, len);
    return len;
  }
  void close() override {
    unreadAtClose = inbox.size() - pos;
    closed = true;
  }

  void fromPeer(const std::string& bytes) { inbox += bytes; }
  void drain(size_t bytes) { if (room >= 0) room += (long)bytes; }
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
  int status = 200;  // what handle() answers

  void handle(const http::Request& req, http::Response& res, uint32_t) override {
    lastBody = req.body ? std::string(req.body, req.contentLength) : "";
    res.status = status;
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
  std::vector<std::string> log;
  Fixture() { server.setLog([this](const std::string& line) { log.push_back(line); }); }
  bool logged(const std::string& part) const {
    for (const auto& l : log) if (l.find(part) != std::string::npos) return true;
    return false;
  }

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
  // the refused connection is read to its end for a short while before it is closed, then the next comes
  TEST_ASSERT_FALSE(bad.closed);
  f.run(web::WebServer::kLingerSeconds);
  TEST_ASSERT_TRUE(bad.closed);
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

// A browser that opens a connection and goes silent must not lock everyone out. Browsers keep spare
// connections that never send a byte; with one connection at a time each of them used to hold everybody
// up for the whole idle time.
void test_a_silent_connection_is_dropped_after_the_first_byte_time() {
  Fixture f;
  f.server.begin();
  FakeConn quiet, next;
  // quiet sends nothing
  next.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&quiet);
  f.tcp.waiting.push_back(&next);
  f.server.poll(0);
  f.server.poll(web::WebServer::kFirstByteSeconds - 1);
  TEST_ASSERT_FALSE(quiet.closed);
  f.server.poll(web::WebServer::kFirstByteSeconds);
  TEST_ASSERT_TRUE(quiet.closed);
  f.run(web::WebServer::kFirstByteSeconds);
  TEST_ASSERT_TRUE(next.outbox.find("<hi/>") != std::string::npos);
  TEST_ASSERT_TRUE(f.logged("connection closed without a complete request (silent, 3 s, 0 bytes received)"));
}

// A slow request is not a silent one: the long idle time counts from the last byte once a byte has come.
void test_a_request_that_has_begun_may_pause_for_the_idle_time() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET / HTTP/1.1\r\n");
  f.tcp.waiting.push_back(&c);
  f.server.poll(0);
  f.server.poll(web::WebServer::kFirstByteSeconds + 3);   // later than the first-byte time
  TEST_ASSERT_FALSE(c.closed);
  c.fromPeer("\r\n");
  f.run(web::WebServer::kFirstByteSeconds + 4, 5);
  TEST_ASSERT_TRUE(c.outbox.find("<hi/>") != std::string::npos);
  // and a request that stops in the middle is dropped after the idle time
  FakeConn half;
  half.fromPeer("GET / HTT");
  f.tcp.waiting.push_back(&half);
  f.server.poll(100);
  f.server.poll(100 + web::WebServer::kIdleSeconds - 1);
  TEST_ASSERT_FALSE(half.closed);
  f.server.poll(100 + web::WebServer::kIdleSeconds);
  TEST_ASSERT_TRUE(half.closed);
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

// ---- what the device's stack does, and what the browser does to it --------------------------------------

// The send buffer of the stack is smaller than a page: write() takes what fits and the peer's reading
// makes room. The answer arrives whole and in order, and nothing is logged for a request that went well.
void test_a_send_buffer_that_fills_up_is_waited_for() {
  Fixture f;
  f.handler.page.clear();
  for (int i = 0; i < 700; ++i) f.handler.page += "0123456789abcdef";  // 11 KB, as big as the largest answer
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET /api/file?path=/system.log HTTP/1.1\r\nHost: x\r\n\r\n");
  c.room = 1460;
  f.tcp.waiting.push_back(&c);
  for (uint32_t t = 0; t < 40 && !c.closed; ++t) {
    f.run(t / 10, 3);
    c.drain(1000);  // the browser reads, the stack frees room
  }
  const size_t sep = c.outbox.find("\r\n\r\n");
  TEST_ASSERT_TRUE(sep != std::string::npos);
  TEST_ASSERT_TRUE(c.outbox.substr(sep + 4) == f.handler.page);
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_EQUAL(0, (int)f.log.size());
}

// A reader that takes nothing: the answer is given up after the idle time and the next client is served.
// The line in the log says how far it got.
void test_a_reader_that_takes_nothing_is_given_up_and_logged_and_the_next_one_is_served() {
  Fixture f;
  f.handler.page.assign(5000, 'x');
  f.server.begin();
  FakeConn stuck, next;
  stuck.fromPeer("GET /api/contacts HTTP/1.1\r\n\r\n");
  stuck.room = 1000;
  next.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&stuck);
  f.tcp.waiting.push_back(&next);
  f.run(0, 5);
  TEST_ASSERT_FALSE(stuck.closed);
  TEST_ASSERT_TRUE(next.outbox.empty());  // the second connection waits: one at a time
  f.run(web::WebServer::kIdleSeconds - 1, 5);
  TEST_ASSERT_FALSE(stuck.closed);
  f.run(web::WebServer::kIdleSeconds, 5);
  TEST_ASSERT_TRUE(stuck.closed);
  f.run(web::WebServer::kIdleSeconds, 10);
  TEST_ASSERT_TRUE(next.outbox.find("xxxx") != std::string::npos);
  TEST_ASSERT_EQUAL(1, (int)f.log.size());
  TEST_ASSERT_EQUAL_STRING("WEB: GET /api/contacts -> 200, aborted (stalled), sent 1000 of 5158 bytes", f.log[0].c_str());
}

// A connection that was reset: write() gives 0 for ever. The one slot is freed at once, not after the idle time.
void test_a_reset_connection_frees_the_slot_at_once() {
  Fixture f;
  f.handler.page.assign(5000, 'x');
  f.server.begin();
  FakeConn gone, next;
  gone.fromPeer("GET / HTTP/1.1\r\n\r\n");
  gone.room = 1000;
  next.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&gone);
  f.tcp.waiting.push_back(&next);
  f.run(0, 3);
  TEST_ASSERT_FALSE(gone.closed);
  gone.reset = true;  // the browser gave up on this connection
  f.run(0, 6);        // no time has passed
  TEST_ASSERT_TRUE(gone.closed);
  TEST_ASSERT_TRUE(next.outbox.find("xxxx") != std::string::npos);
  TEST_ASSERT_TRUE(f.logged("WEB: GET / -> 200, aborted (peer gone), sent 1000 of 5158 bytes"));
}

// An early error answer is sent while the browser is still sending: the input is read and thrown away
// before the close, because a close with unread input is a reset in the stack and a reset can destroy the
// answer on its way. The linger ends when the browser closes, or after kLingerSeconds.
void test_an_early_error_answer_is_followed_by_reading_the_rest() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  c.fromPeer("POST /api/settings HTTP/1.1\r\nContent-Length: 100000\r\n\r\n");
  f.tcp.waiting.push_back(&c);
  f.run(0, 5);
  TEST_ASSERT_TRUE(c.outbox.rfind("HTTP/1.1 413", 0) == 0);
  TEST_ASSERT_FALSE(c.closed);
  c.fromPeer(std::string(3000, 'b'));  // the body the browser is already sending
  f.run(1, 5);
  TEST_ASSERT_FALSE(c.closed);
  c.peerGone = true;  // it has read the answer and closes
  f.run(1, 3);
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_EQUAL(0, (int)c.unreadAtClose);

  // a browser that does not close: the linger is bounded
  FakeConn d;
  d.fromPeer("POST /api/settings HTTP/1.1\r\nContent-Length: 100000\r\n\r\n");
  f.tcp.waiting.push_back(&d);
  f.run(10, 5);
  TEST_ASSERT_FALSE(d.closed);
  f.run(10 + web::WebServer::kLingerSeconds, 3);
  TEST_ASSERT_TRUE(d.closed);
}

// Whatever input is left after a complete exchange is read before the close too (a second request that
// the browser sent down the same connection).
void test_input_left_after_a_complete_exchange_is_read_before_the_close() {
  Fixture f;
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET / HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&c);
  f.run(0, 1);                                     // the head of the answer is out
  c.fromPeer("GET /again HTTP/1.1\r\n\r\n");     // and the next request arrives
  f.run(0, 5);
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_EQUAL(0, (int)c.unreadAtClose);
}

// ---- the log lines -------------------------------------------------------------------------------------

void test_a_request_with_an_error_status_leaves_one_line_without_secrets() {
  Fixture f;
  f.handler.status = 404;
  f.handler.page = "{\"error\":\"File not found\"}";
  f.server.begin();
  FakeConn c;
  const std::string body = "TOP-SECRET-BODY";
  c.fromPeer("PUT /api/file?path=/config/secrets.toml&x=hunter2 HTTP/1.1\r\nAuthorization: Basic cGFyZW50Omh1bnRlcjI=\r\n"
             "X-Phoneplus: 1\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
  f.tcp.waiting.push_back(&c);
  f.run(0, 5);
  TEST_ASSERT_EQUAL(1, (int)f.log.size());
  const std::string& line = f.log[0];
  TEST_ASSERT_TRUE(line.rfind("WEB: PUT /api/file?path&x -> 404, sent ", 0) == 0);
  TEST_ASSERT_TRUE(line.find(" of ") != std::string::npos);
  for (const char* secret : {"secrets.toml", "hunter2", "cGFyZW50", "TOP-SECRET", "Authorization"})
    TEST_ASSERT_TRUE_MESSAGE(line.find(secret) == std::string::npos, secret);
  TEST_ASSERT_TRUE(line.find('\n') == std::string::npos);
}

void test_a_request_refused_by_the_parser_is_logged_and_a_good_one_is_not() {
  Fixture f;
  f.server.begin();
  FakeConn good, big;
  good.fromPeer("GET / HTTP/1.1\r\n\r\n");
  big.fromPeer("POST /api/settings HTTP/1.1\r\nContent-Length: 100000\r\n\r\n");
  f.tcp.waiting.push_back(&good);
  f.tcp.waiting.push_back(&big);
  f.run(0, 10);
  f.run(5, 5);
  TEST_ASSERT_EQUAL(1, (int)f.log.size());
  TEST_ASSERT_TRUE(f.logged("WEB: POST /api/settings -> 413, sent "));
}

void test_a_path_cannot_forge_a_log_line() {
  Fixture f;
  f.handler.status = 404;
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET /a\x01" "b\x7f\xc3\xa4 HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&c);
  f.run(0, 5);
  TEST_ASSERT_EQUAL(1, (int)f.log.size());
  TEST_ASSERT_TRUE(f.logged("WEB: GET /a?b??? -> 404"));
}

void test_the_log_is_limited_per_minute_and_says_how_many_more() {
  Fixture f;
  f.handler.status = 401;
  f.server.begin();
  std::vector<std::unique_ptr<FakeConn>> conns;
  for (uint32_t i = 0; i < web::WebServer::kLogLinesPerMinute + 5; ++i) {
    conns.push_back(std::make_unique<FakeConn>());
    conns.back()->fromPeer("GET /api/state HTTP/1.1\r\n\r\n");
    f.tcp.waiting.push_back(conns.back().get());
  }
  f.run(10, 400);
  TEST_ASSERT_EQUAL((int)web::WebServer::kLogLinesPerMinute, (int)f.log.size());
  // the next one after the minute reports what was left out, then goes on
  FakeConn later;
  later.fromPeer("GET /api/state HTTP/1.1\r\n\r\n");
  f.tcp.waiting.push_back(&later);
  f.run(75, 10);
  TEST_ASSERT_EQUAL((int)web::WebServer::kLogLinesPerMinute + 2, (int)f.log.size());
  TEST_ASSERT_EQUAL_STRING("WEB: 5 more requests ended badly in the last minute", f.log[web::WebServer::kLogLinesPerMinute].c_str());
}

void test_stopping_the_server_mid_answer_is_logged() {
  Fixture f;
  f.handler.page.assign(5000, 'x');
  f.server.begin();
  FakeConn c;
  c.fromPeer("GET /api/backup HTTP/1.1\r\n\r\n");
  c.room = 500;
  f.tcp.waiting.push_back(&c);
  f.run(0, 3);
  f.server.stop();
  TEST_ASSERT_TRUE(c.closed);
  TEST_ASSERT_TRUE(f.logged("WEB: GET /api/backup -> 200, aborted (stopped), sent 500 of "));
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
  RUN_TEST(test_a_silent_connection_is_dropped_after_the_first_byte_time);
  RUN_TEST(test_a_request_that_has_begun_may_pause_for_the_idle_time);
  RUN_TEST(test_vanished_client_is_cleaned_up);
  RUN_TEST(test_stop_closes_the_connection_and_the_port);
  RUN_TEST(test_a_send_buffer_that_fills_up_is_waited_for);
  RUN_TEST(test_a_reader_that_takes_nothing_is_given_up_and_logged_and_the_next_one_is_served);
  RUN_TEST(test_a_reset_connection_frees_the_slot_at_once);
  RUN_TEST(test_an_early_error_answer_is_followed_by_reading_the_rest);
  RUN_TEST(test_input_left_after_a_complete_exchange_is_read_before_the_close);
  RUN_TEST(test_a_request_with_an_error_status_leaves_one_line_without_secrets);
  RUN_TEST(test_a_request_refused_by_the_parser_is_logged_and_a_good_one_is_not);
  RUN_TEST(test_a_path_cannot_forge_a_log_line);
  RUN_TEST(test_the_log_is_limited_per_minute_and_says_how_many_more);
  RUN_TEST(test_stopping_the_server_mid_answer_is_logged);
  return UNITY_END();
}
