// The USB console: how a line is understood, which files the cable reaches, the framing of a write and of a web
// request, and that nothing blocks.

#include <deque>

#include <unity.h>

#include <board/Console.h>
#include <fakes/FakeConfigStore.h>
#include <board/Delay.h>
#include <fakes/FakeMillis.h>
#include <fakes/FakeUartPort.h>

using namespace board;
using Kind = ConsoleCommand::Kind;

void setUp() {}
void tearDown() {}

namespace {
struct Host : ConsoleHost {
  int restarts = 0;
  std::string lastPhone, lastText;
  bool accept = true;
  bool web = true;
  std::string webResponse = "HTTP/1.1 200 OK\r\n\r\nhello";
  std::vector<uint8_t> lastRequest;
  void restart() override { ++restarts; }
  bool sendText(const std::string& phone, const std::string& text) override {
    lastPhone = phone;
    lastText = text;
    return accept;
  }
  bool handleWeb(const std::vector<uint8_t>& request, std::string& response) override {
    lastRequest = request;
    response = webResponse;
    return web;
  }
  std::vector<std::string> batteryReport() override { return {"gauge: voltage 4118 mV", "charger: SY6970 at 0x6A"}; }
};

// A pause that lets the clock run, so a wait in the code under test ends (FakeDelay only counts).
struct TickingDelay : Delay {
  explicit TickingDelay(FakeMillis& m) : m_(m) {}
  void delayMs(uint32_t ms) override { m_.advance(ms); }
  void delayUs(uint32_t) override {}
  FakeMillis& m_;
};

// A USB port as the device shows it: takes at most `chunk` bytes per call, and after each accepted chunk answers 0
// for `stalls` calls in a row (the transmit buffer is full; the bus drains it while time passes). `drainMs` is the
// time one zero-length call is worth, so the stalls are not instantaneous. `reading` false: nobody drains any more.
struct UsbLikePort : UartPort {
  UsbLikePort(FakeMillis& m) : m_(m) {}
  size_t write(const uint8_t* data, size_t len) override {
    if (!connected) return 0;
    if (!reading) return 0;
    if (zeros_ < stalls) {
      ++zeros_;
      return 0;
    }
    zeros_ = 0;
    const size_t n = len < chunk ? len : chunk;
    got.append((const char*)data, n);
    return n;
  }
  int read() override {
    if (in_.empty()) return -1;
    const uint8_t b = in_.front();
    in_.pop_front();
    return b;
  }
  bool hostConnected() override { return connected; }
  void feed(const std::string& s) { for (char c : s) in_.push_back((uint8_t)c); }
  FakeMillis& m_;
  size_t chunk = 64;
  int stalls = 20;
  bool connected = true;
  bool reading = true;
  std::string got;

private:
  int zeros_ = 0;
  std::deque<uint8_t> in_;
};

struct Rig {
  FakeUartPort port;
  FakeConfigStore store;
  Host host;
  FakeMillis millis;
  TickingDelay delay{millis};
  Console console{port, store, host, millis, delay};
  void type(const std::string& text) {
    port.feed(text);
    console.poll();
  }
  std::string answer() {
    const std::string s = port.sent();
    return s;
  }
};
}  // namespace

// --- parsing ---------------------------------------------------------------------------------------

void test_lines_that_are_not_for_the_console_are_ignored() {
  TEST_ASSERT_TRUE(parseConsoleLine("").kind == Kind::None);
  TEST_ASSERT_TRUE(parseConsoleLine("[log] Modem: started").kind == Kind::None);
  TEST_ASSERT_TRUE(parseConsoleLine("hello").kind == Kind::None);
  TEST_ASSERT_TRUE(parseConsoleLine("battery").kind == Kind::None);  // case matters: a log line must not be a command
}

void test_the_verbs_with_and_without_the_word_config() {
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG LIST").kind == Kind::List);
  TEST_ASSERT_TRUE(parseConsoleLine("  CONFIG   RESTART  ").kind == Kind::Restart);
  TEST_ASSERT_TRUE(parseConsoleLine("BATTERY").kind == Kind::Battery);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG BATTERY").kind == Kind::Battery);
  TEST_ASSERT_TRUE(parseConsoleLine("SMS +491701111111 hi").kind == Kind::Sms);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG SMS +491701111111 hi").kind == Kind::Sms);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG FLY").kind == Kind::Unknown);
}

void test_the_cable_reaches_only_the_known_files() {
  const char* writableFiles[] = {"settings.toml", "contacts.toml", "secrets.toml"};
  for (const char* f : writableFiles) {
    const auto w = parseConsoleLine(std::string("CONFIG WRITE ") + f + " 10 100");
    TEST_ASSERT_TRUE_MESSAGE(w.kind == Kind::Write, f);
    TEST_ASSERT_EQUAL_STRING((std::string("/config/") + f).c_str(), w.path.c_str());
    TEST_ASSERT_TRUE_MESSAGE(parseConsoleLine(std::string("CONFIG READ ") + f).kind == Kind::Read, f);
  }
  const auto log = parseConsoleLine("CONFIG READ system.log");
  TEST_ASSERT_TRUE(log.kind == Kind::Read);
  TEST_ASSERT_EQUAL_STRING("/system.log", log.path.c_str());
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG READ system.old.log").kind == Kind::Read);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG READ messages.log").kind == Kind::Read);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG READ energy.log").kind == Kind::Read);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG READ energy.old.log").kind == Kind::Read);
  // A log is read over the cable and never written.
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WRITE system.log 10 100").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WRITE messages.log 10 100").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WRITE energy.log 10 100").kind == Kind::Unknown);
}

void test_no_path_no_dots_nothing_else_on_the_card() {
  const char* bad[] = {"CONFIG READ ../secrets.toml", "CONFIG READ /config/secrets.toml", "CONFIG READ lang/en.toml",
                       "CONFIG READ other.txt", "CONFIG READ SETTINGS.TOML", "CONFIG WRITE /etc/passwd 10 1",
                       "CONFIG WRITE settings.toml.tmp 10 1", "CONFIG READ", "CONFIG WRITE"};
  for (const char* line : bad) TEST_ASSERT_TRUE_MESSAGE(parseConsoleLine(line).kind == Kind::Unknown, line);
}

void test_the_length_and_the_checksum_of_a_write_are_checked_strictly() {
  const char* bad[] = {"CONFIG WRITE settings.toml", "CONFIG WRITE settings.toml 10", "CONFIG WRITE settings.toml 0 5",
                       "CONFIG WRITE settings.toml 8193 5", "CONFIG WRITE settings.toml -1 5", "CONFIG WRITE settings.toml 12abc 5",
                       "CONFIG WRITE settings.toml 10 65536", "CONFIG WRITE settings.toml 10 x"};
  for (const char* line : bad) TEST_ASSERT_TRUE_MESSAGE(parseConsoleLine(line).kind == Kind::Unknown, line);
  const auto ok = parseConsoleLine("CONFIG WRITE settings.toml 8192 65535");
  TEST_ASSERT_TRUE(ok.kind == Kind::Write);
  TEST_ASSERT_EQUAL(8192, ok.length);
  TEST_ASSERT_EQUAL(65535, ok.sum);
}

void test_the_web_request_length_is_bounded_by_what_the_parser_accepts() {
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WEB 9300").kind == Kind::Web);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WEB 9301").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WEB 0").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("CONFIG WEB").kind == Kind::Unknown);
}

void test_an_sms_keeps_its_text_and_turns_backslash_n_into_a_line_break() {
  const auto c = parseConsoleLine("SMS +491701111111 first line\\nsecond  line");
  TEST_ASSERT_TRUE(c.kind == Kind::Sms);
  TEST_ASSERT_EQUAL_STRING("+491701111111", c.phone.c_str());
  TEST_ASSERT_EQUAL_STRING("first line\nsecond  line", c.text.c_str());
  TEST_ASSERT_TRUE(parseConsoleLine("SMS +491701111111").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("SMS").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("SMS " + std::string(30, '1') + " x").kind == Kind::Unknown);
  TEST_ASSERT_TRUE(parseConsoleLine("SMS +491701111111 " + std::string(200, 'x')).kind == Kind::Unknown);
}

// --- running ---------------------------------------------------------------------------------------

void test_list_shows_the_config_files_and_their_sizes() {
  Rig r;
  r.store.put("/config/settings.toml", "12345");
  r.type("CONFIG LIST\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG LIST BEGIN\nsettings.toml 5\nCONFIG LIST END\n", r.answer().c_str());
}

void test_list_without_a_card_says_so() {
  Rig r;
  r.type("CONFIG LIST\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG ERROR no card\n", r.answer().c_str());
}

void test_read_gives_the_header_the_content_and_the_end_marker() {
  Rig r;
  r.store.put("/config/contacts.toml", "abc");
  r.type("CONFIG READ contacts.toml\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG CONTENT contacts.toml 3 294 from 0 of 3\nabc\nCONFIG END\n", r.answer().c_str());
}

void test_read_of_a_missing_file_is_empty_and_tells_so() {
  Rig r;
  r.type("CONFIG READ system.log\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG CONTENT system.log 0 0 from 0 of 0\n\nCONFIG END\n", r.answer().c_str());
}

void test_read_of_a_long_log_gives_the_end_and_says_where_it_starts() {
  Rig r;
  std::string log(10000, 'a');
  log.replace(9999, 1, "Z");
  r.store.put("/system.log", log);
  r.type("CONFIG READ system.log\n");
  const std::string a = r.answer();
  TEST_ASSERT_TRUE(a.find("CONFIG CONTENT system.log 8192 ") == 0);
  TEST_ASSERT_TRUE(a.find("from 1808 of 10000") != std::string::npos);
  TEST_ASSERT_TRUE(a.find("Z\nCONFIG END\n") != std::string::npos);
}

void test_a_write_is_announced_collected_checked_and_stored() {
  Rig r;
  const std::string data = "language = \"en\"\n";
  r.type("CONFIG WRITE settings.toml " + std::to_string(data.size()) + " " +
         std::to_string(consoleChecksum(reinterpret_cast<const uint8_t*>(data.data()), data.size())) + "\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG READY 16\n", r.answer().c_str());
  r.type(data);
  TEST_ASSERT_TRUE(r.answer().find("CONFIG OK settings.toml 16\n") != std::string::npos);
  std::string stored;
  r.store.read("/config/settings.toml", stored, 100);
  TEST_ASSERT_EQUAL_STRING(data.c_str(), stored.c_str());
}

void test_the_bytes_of_a_write_may_come_in_pieces_and_with_newlines_inside() {
  Rig r;
  const std::string data = "a\nb\r\nc\n";
  r.type("CONFIG WRITE contacts.toml 7 " +
         std::to_string(consoleChecksum(reinterpret_cast<const uint8_t*>(data.data()), data.size())) + "\n");
  for (char c : data) r.type(std::string(1, c));
  std::string stored;
  r.store.read("/config/contacts.toml", stored, 100);
  TEST_ASSERT_EQUAL_STRING(data.c_str(), stored.c_str());
}

void test_a_wrong_checksum_stores_nothing_and_leaves_the_old_file() {
  Rig r;
  r.store.put("/config/settings.toml", "old");
  r.type("CONFIG WRITE settings.toml 3 1\n");
  r.type("new");
  TEST_ASSERT_TRUE(r.answer().find("CONFIG ERROR checksum") != std::string::npos);
  std::string stored;
  r.store.read("/config/settings.toml", stored, 100);
  TEST_ASSERT_EQUAL_STRING("old", stored.c_str());
}

void test_a_store_that_fails_is_reported() {
  Rig r;
  r.store.failAllWrites(true);
  r.type("CONFIG WRITE settings.toml 3 " + std::to_string(consoleChecksum(reinterpret_cast<const uint8_t*>("new"), 3)) + "\n");
  r.type("new");
  TEST_ASSERT_TRUE(r.answer().find("CONFIG ERROR cannot write\n") != std::string::npos);
}

void test_bytes_that_do_not_come_are_given_up_after_ten_seconds_and_the_console_listens_again() {
  Rig r;
  r.type("CONFIG WRITE settings.toml 10 5\n");
  r.type("abc");
  r.millis.advance(9999);
  r.console.poll();
  TEST_ASSERT_TRUE(r.answer().find("only") == std::string::npos);
  r.millis.advance(1);
  r.console.poll();
  TEST_ASSERT_TRUE(r.answer().find("CONFIG ERROR only 3 of 10 bytes\n") != std::string::npos);
  r.type("CONFIG RESTART\n");
  TEST_ASSERT_EQUAL(1, r.host.restarts);
}

void test_a_web_request_is_collected_handed_to_the_service_and_answered_with_its_length() {
  Rig r;
  const std::string request = "GET /api/state HTTP/1.1\r\n\r\n";
  r.type("CONFIG WEB " + std::to_string(request.size()) + "\n");
  r.type(request);
  const std::string a = r.answer();
  const std::string response = "HTTP/1.1 200 OK\r\n\r\nhello";
  TEST_ASSERT_TRUE(a.find("CONFIG READY " + std::to_string(request.size()) + "\n") == 0);
  TEST_ASSERT_TRUE(a.find("CONFIG WEB " + std::to_string(response.size()) + "\n" + response + "\n") != std::string::npos);
  TEST_ASSERT_EQUAL(request.size(), r.host.lastRequest.size());
}

void test_without_a_web_service_the_bridge_says_so() {
  Rig r;
  r.host.web = false;
  r.type("CONFIG WEB 3\n");
  r.type("abc");
  TEST_ASSERT_TRUE(r.answer().find("CONFIG ERROR no web service\n") != std::string::npos);
}

void test_battery_prints_the_diagnosis_between_markers() {
  Rig r;
  r.type("BATTERY\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG BATTERY BEGIN\ngauge: voltage 4118 mV\ncharger: SY6970 at 0x6A\nCONFIG BATTERY END\n",
                           r.answer().c_str());
}

void test_sms_goes_through_the_hosts_way_of_sending_and_says_whether_it_was_taken() {
  Rig r;
  r.type("SMS +491701111111 hello there\n");
  TEST_ASSERT_EQUAL_STRING("+491701111111", r.host.lastPhone.c_str());
  TEST_ASSERT_EQUAL_STRING("hello there", r.host.lastText.c_str());
  TEST_ASSERT_TRUE(r.answer().find("accepted") != std::string::npos);
  r.host.accept = false;
  r.type("SMS +491701111111 again\n");
  TEST_ASSERT_TRUE(r.answer().find("refused") != std::string::npos);
}

void test_restart_answers_first_and_then_restarts() {
  Rig r;
  r.type("CONFIG RESTART\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG OK restart\n", r.answer().c_str());
  TEST_ASSERT_EQUAL(1, r.host.restarts);
}

void test_log_lines_between_commands_do_not_disturb_it_and_carriage_returns_are_ignored() {
  Rig r;
  r.type("[log] something\r\nCONFIG LIST\r\n[log] more\r\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG ERROR no card\n", r.answer().c_str());
}

// A line of more than 320 bytes is not a command, and neither is what follows it up to the line end.
void test_an_overlong_line_is_refused_once_and_its_tail_is_not_run_as_a_command() {
  Rig r;
  // 320 bytes fill the line; the next byte is what overflows it and is dropped, and the tail that follows is a command
  // by itself if it is taken for a new line.
  r.type("CONFIG SMS +491701111111 " + std::string(295, 'x') + "xCONFIG RESTART\n");
  TEST_ASSERT_EQUAL_STRING("CONFIG ERROR line too long\n", r.answer().c_str());
  TEST_ASSERT_EQUAL(0, r.host.restarts);
  r.type("CONFIG RESTART\n");
  TEST_ASSERT_EQUAL(1, r.host.restarts);
}

void test_a_port_that_takes_nothing_does_not_hang_the_console() {
  Rig r;
  r.port.limitWritesTo(0);
  r.type("CONFIG LIST\n");
  TEST_ASSERT_EQUAL_STRING("", r.answer().c_str());
}

namespace {
struct UsbRig {
  FakeMillis millis;
  UsbLikePort port{millis};
  FakeConfigStore store;
  Host host;
  TickingDelay delay{millis};
  Console console{port, store, host, millis, delay};
  void ask(const std::string& web) {
    host.webResponse = web;
    port.feed("CONFIG WEB 4\n");
    console.poll();
    port.got.clear();  // the "CONFIG READY" line is not what is under test
    port.feed("GET ");
    console.poll();
  }
};
}  // namespace

void test_a_long_answer_arrives_whole_although_the_port_stalls_between_chunks() {
  UsbRig r;
  const std::string body(32 * 1024, 'x');
  r.ask(body);
  const std::string expect = "CONFIG WEB " + std::to_string(body.size()) + "\n" + body + "\n";
  TEST_ASSERT_EQUAL(expect.size(), r.port.got.size());
  TEST_ASSERT_TRUE(r.port.got == expect);
}

void test_with_no_computer_connected_nothing_waits() {
  UsbRig r;
  r.port.connected = false;
  const uint32_t before = r.millis.millis32();
  r.ask(std::string(32 * 1024, 'x'));
  TEST_ASSERT_EQUAL_UINT32(before, r.millis.millis32());
  TEST_ASSERT_EQUAL(0, r.port.got.size());
}

void test_a_computer_that_stopped_reading_makes_say_give_up_after_the_bound() {
  UsbRig r;
  r.port.feed("CONFIG WEB 4\n");
  r.console.poll();
  r.port.reading = false;  // the computer stops here; the answer is three writes (header, body, line end)
  const uint32_t before = r.millis.millis32();
  r.port.feed("GET ");
  r.console.poll();
  const uint32_t waited = r.millis.millis32() - before;
  TEST_ASSERT_TRUE(waited >= kConsoleWriteStallMs);
  TEST_ASSERT_TRUE(waited <= 3 * (kConsoleWriteStallMs + kConsoleWritePauseMs));
}

void test_progress_resets_the_budget_so_a_slow_but_reading_computer_gets_everything() {
  UsbRig r;
  r.port.chunk = 16;
  r.port.stalls = 100;  // 200 ms of pauses per chunk: under the bound each time, far over it in total
  const std::string body(2048, 'y');
  r.ask(body);
  TEST_ASSERT_EQUAL(body.size() + std::string("CONFIG WEB 2048\n\n").size(), r.port.got.size());
}

void test_poll_without_input_does_nothing() {
  Rig r;
  r.console.poll();
  TEST_ASSERT_EQUAL_STRING("", r.answer().c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_lines_that_are_not_for_the_console_are_ignored);
  RUN_TEST(test_the_verbs_with_and_without_the_word_config);
  RUN_TEST(test_the_cable_reaches_only_the_known_files);
  RUN_TEST(test_no_path_no_dots_nothing_else_on_the_card);
  RUN_TEST(test_the_length_and_the_checksum_of_a_write_are_checked_strictly);
  RUN_TEST(test_the_web_request_length_is_bounded_by_what_the_parser_accepts);
  RUN_TEST(test_an_sms_keeps_its_text_and_turns_backslash_n_into_a_line_break);
  RUN_TEST(test_list_shows_the_config_files_and_their_sizes);
  RUN_TEST(test_list_without_a_card_says_so);
  RUN_TEST(test_read_gives_the_header_the_content_and_the_end_marker);
  RUN_TEST(test_read_of_a_missing_file_is_empty_and_tells_so);
  RUN_TEST(test_read_of_a_long_log_gives_the_end_and_says_where_it_starts);
  RUN_TEST(test_a_write_is_announced_collected_checked_and_stored);
  RUN_TEST(test_the_bytes_of_a_write_may_come_in_pieces_and_with_newlines_inside);
  RUN_TEST(test_a_wrong_checksum_stores_nothing_and_leaves_the_old_file);
  RUN_TEST(test_a_store_that_fails_is_reported);
  RUN_TEST(test_bytes_that_do_not_come_are_given_up_after_ten_seconds_and_the_console_listens_again);
  RUN_TEST(test_a_web_request_is_collected_handed_to_the_service_and_answered_with_its_length);
  RUN_TEST(test_without_a_web_service_the_bridge_says_so);
  RUN_TEST(test_battery_prints_the_diagnosis_between_markers);
  RUN_TEST(test_sms_goes_through_the_hosts_way_of_sending_and_says_whether_it_was_taken);
  RUN_TEST(test_restart_answers_first_and_then_restarts);
  RUN_TEST(test_log_lines_between_commands_do_not_disturb_it_and_carriage_returns_are_ignored);
  RUN_TEST(test_an_overlong_line_is_refused_once_and_its_tail_is_not_run_as_a_command);
  RUN_TEST(test_a_port_that_takes_nothing_does_not_hang_the_console);
  RUN_TEST(test_a_long_answer_arrives_whole_although_the_port_stalls_between_chunks);
  RUN_TEST(test_with_no_computer_connected_nothing_waits);
  RUN_TEST(test_a_computer_that_stopped_reading_makes_say_give_up_after_the_bound);
  RUN_TEST(test_progress_resets_the_budget_so_a_slow_but_reading_computer_gets_everything);
  RUN_TEST(test_poll_without_input_does_nothing);
  return UNITY_END();
}
