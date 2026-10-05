// The parents' web page against the simulator, through a real socket on the loopback address: the
// real WebService answers, the card is a directory, and every security rule is tried the way a
// browser (or an attacker on the same machine) would meet it. The simulator is pumped on this
// thread while a client thread talks to it.
// allow-non-english: the catalogs under test contain German.

#include <unity.h>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <Simulator.h>

void setUp() {}
void tearDown() {}

namespace {

namespace fs = std::filesystem;

constexpr const char* kPassword = "test-password-1";

struct Reply {
  int status = 0;  // 0: nothing came back
  std::string head, body;
  bool has(const std::string& part) const { return body.find(part) != std::string::npos; }
};

std::string base64(const std::string& in) {
  static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < in.size(); i += 3) {
    const uint32_t n = (static_cast<uint8_t>(in[i]) << 16) | (i + 1 < in.size() ? static_cast<uint8_t>(in[i + 1]) << 8 : 0) |
                       (i + 2 < in.size() ? static_cast<uint8_t>(in[i + 2]) : 0);
    out += t[(n >> 18) & 63];
    out += t[(n >> 12) & 63];
    out += i + 1 < in.size() ? t[(n >> 6) & 63] : '=';
    out += i + 2 < in.size() ? t[n & 63] : '=';
  }
  return out;
}

// One request on one connection, read to the end (the server closes after each answer). Does not
// half-close: the server treats a client that has shut down as gone.
Reply talk(uint16_t port, const std::string& request) {
  Reply reply;
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(fd);
    return reply;
  }
  ::send(fd, request.data(), request.size(), 0);
  std::string raw;
  char buf[4096];
  pollfd p{fd, POLLIN, 0};
  while (::poll(&p, 1, 5000) > 0) {
    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    raw.append(buf, static_cast<size_t>(n));
  }
  ::close(fd);
  const size_t split = raw.find("\r\n\r\n");
  if (split == std::string::npos) return reply;
  reply.head = raw.substr(0, split);
  reply.body = raw.substr(split + 4);
  reply.status = std::atoi(reply.head.c_str() + reply.head.find(' ') + 1);
  return reply;
}

struct Web {
  std::ostringstream out;
  sim::Simulator sim;
  uint16_t port = 0;

  Web() : sim(options(), out) {
    sim.execute("wifi ap");
    sim.advance(2000);
    port = sim.webPort();
  }
  static sim::Options options() {
    static int n = 0;
    sim::Options o;
    o.workDir = (fs::temp_directory_path() / ("phoneplus-simweb-" + std::to_string(::getpid()) + "-" + std::to_string(++n))).string();
    o.webPort = 0;
    return o;
  }

  Reply send(const std::string& request) {
    auto answer = std::async(std::launch::async, [this, request] { return talk(port, request); });
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (answer.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
      sim.advance(20);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (std::chrono::steady_clock::now() > limit) TEST_FAIL_MESSAGE("the simulator did not answer in 20 seconds");
    }
    return answer.get();
  }

  Reply get(const std::string& path, const std::string& password = kPassword) {
    std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n";
    if (!password.empty()) request += "Authorization: Basic " + base64("parent:" + password) + "\r\n";
    return send(request + "\r\n");
  }
  Reply change(const std::string& method, const std::string& path, const std::string& body, bool header = true,
               const std::string& password = kPassword) {
    std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Basic " +
                          base64("parent:" + password) + "\r\n";
    if (header) request += "X-Phoneplus: 1\r\n";
    request += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    return send(request);
  }
  std::string file(const std::string& name) {
    std::ifstream in(fs::path(sim.card().root()) / name, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
  }
};

}  // namespace

// ---- the page and the way in -----------------------------------------------------------------------------

void test_the_simulator_says_where_the_page_is_once_wifi_is_on() {
  Web w;
  TEST_ASSERT_TRUE(w.port != 0);
  TEST_ASSERT_TRUE(w.out.str().find("web: http://127.0.0.1:" + std::to_string(w.port)) != std::string::npos);
}

void test_there_is_no_page_while_wifi_is_off() {
  Web w;
  w.sim.execute("wifi off");
  w.sim.advance(2000);
  TEST_ASSERT_EQUAL(0, w.sim.webPort());
  TEST_ASSERT_EQUAL(0, talk(w.port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n").status);
}

void test_the_real_page_is_served_without_a_password() {
  Web w;
  const Reply page = w.get("/", "");
  TEST_ASSERT_EQUAL(200, page.status);
  TEST_ASSERT_TRUE(page.has("<!doctype html>"));
  TEST_ASSERT_TRUE(page.body.size() > 10000);
}

void test_the_state_needs_the_password() {
  Web w;
  TEST_ASSERT_EQUAL(401, w.get("/api/state", "").status);
  TEST_ASSERT_EQUAL(401, w.get("/api/state", "wrong").status);
  const Reply state = w.get("/api/state");
  TEST_ASSERT_EQUAL(200, state.status);
}

// ---- languages -----------------------------------------------------------------------------------------------

void test_the_page_can_be_had_in_every_shipped_language() {
  Web w;
  const Reply languages = w.get("/api/languages");
  TEST_ASSERT_EQUAL(200, languages.status);
  for (const char* code : {"\"en\"", "\"de\"", "\"fr\""}) TEST_ASSERT_TRUE(languages.has(code));
  const Reply en = w.get("/api/lang/en"), de = w.get("/api/lang/de"), fr = w.get("/api/lang/fr");
  TEST_ASSERT_EQUAL(200, en.status);
  TEST_ASSERT_EQUAL(200, de.status);
  TEST_ASSERT_EQUAL(200, fr.status);
  TEST_ASSERT_TRUE(en.body != de.body && de.body != fr.body && en.body != fr.body);
  TEST_ASSERT_TRUE(de.has("Anmelden"));
  TEST_ASSERT_EQUAL(404, w.get("/api/lang/zz").status);
}

void test_a_user_catalog_on_the_card_is_offered_by_the_page() {
  Web w;
  std::string english;
  {
    std::ifstream in("lang/en.toml", std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    english = text.str();
  }
  const std::string codeLine = "code   = \"en\"";
  english.replace(english.find(codeLine), codeLine.size(), "code   = \"xx\"");
  TEST_ASSERT_TRUE(w.sim.card().write("/lang/xx.toml", english));
  w.sim.execute("run /dev/null");
  w.send("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
  // The catalog is read when the device starts: a restart, as the page asks for one.
  TEST_ASSERT_EQUAL(200, w.change("POST", "/api/restart", "{}").status);
  w.sim.advance(2000);
  TEST_ASSERT_EQUAL(2, w.sim.boots());
  w.sim.execute("wifi ap");
  w.sim.advance(2000);
  w.port = w.sim.webPort();
  TEST_ASSERT_TRUE(w.get("/api/languages").has("\"xx\""));
}

// ---- saving ---------------------------------------------------------------------------------------------------

void test_a_setting_saved_through_the_api_is_in_the_file_of_the_card() {
  Web w;
  const Reply before = w.get("/api/settings");
  TEST_ASSERT_EQUAL(200, before.status);
  std::string json = before.body;
  const size_t at = json.find("\"Alex\"");
  TEST_ASSERT_TRUE(at != std::string::npos);
  json.replace(at, 6, "\"Sam\"");
  const Reply saved = w.change("PUT", "/api/settings", json);
  TEST_ASSERT_EQUAL_MESSAGE(200, saved.status, saved.body.c_str());
  TEST_ASSERT_TRUE(w.file("config/settings.toml").find("name = \"Sam\"") != std::string::npos);
  TEST_ASSERT_TRUE(w.get("/api/settings").has("\"Sam\""));
  // The data directory the card was copied from is untouched.
  std::ifstream original("sim/data/config/settings.toml");
  std::stringstream text;
  text << original.rdbuf();
  TEST_ASSERT_TRUE(text.str().find("Alex") != std::string::npos);
}

void test_a_language_saved_through_the_api_is_the_devices_after_the_restart_the_page_asks_for() {
  Web w;
  std::string json = w.get("/api/settings").body;
  const std::string english = "\"interface\":{\"language\":\"en\"}";
  const size_t at = json.find(english);
  TEST_ASSERT_TRUE(at != std::string::npos);
  json.replace(at, english.size(), "\"interface\":{\"language\":\"de\"}");
  TEST_ASSERT_EQUAL(200, w.change("PUT", "/api/settings", json).status);
  TEST_ASSERT_EQUAL_STRING("en", w.sim.translator().language().c_str());       // restart-time setting
  TEST_ASSERT_EQUAL(200, w.change("POST", "/api/restart", "{}").status);
  w.sim.advance(2000);
  TEST_ASSERT_EQUAL(2, w.sim.boots());
  TEST_ASSERT_EQUAL_STRING("de", w.sim.translator().language().c_str());
}

void test_a_file_the_device_would_reject_is_not_stored_and_a_typo_is_stored_with_a_warning() {
  Web w;
  const std::string settings = w.file("config/settings.toml");
  const Reply broken = w.change("PUT", "/api/settings", "{not json");
  TEST_ASSERT_TRUE(broken.status >= 400);
  TEST_ASSERT_EQUAL_STRING(settings.c_str(), w.file("config/settings.toml").c_str());
  const Reply shortPassword = w.change("PUT", "/api/secrets", "{\"web_password\":\"abc\"}");
  TEST_ASSERT_TRUE(shortPassword.status >= 400);
  TEST_ASSERT_TRUE(w.file("config/secrets.toml").find("test-password-1") != std::string::npos);
  // A key the device does not know in settings.toml is a warning, not a refusal (WebApi::checkConfigText: soft problems become warnings).
  const Reply typo = w.change("PUT", "/api/settings", "{\"child\":{\"name\":\"Sam\"},\"nonsense\":{\"key\":1}}");
  TEST_ASSERT_EQUAL(200, typo.status);
  TEST_ASSERT_TRUE(typo.has("unknown-key"));
}

void test_the_wifi_off_button_ends_the_page() {
  Web w;
  TEST_ASSERT_EQUAL(200, w.change("POST", "/api/wifi-off", "{}").status);
  w.sim.advance(2000);
  TEST_ASSERT_EQUAL(0, talk(w.port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n").status);
}

// ---- the security rules, through the socket ----------------------------------------------------------------------

void test_a_change_without_the_header_is_refused_and_changes_nothing() {
  Web w;
  const std::string before = w.file("config/settings.toml");
  const Reply refused = w.change("PUT", "/api/settings", "{\"child\":{\"name\":\"Sam\"}}", /*header=*/false);
  TEST_ASSERT_EQUAL(403, refused.status);
  TEST_ASSERT_TRUE(refused.has("X-Phoneplus"));
  TEST_ASSERT_EQUAL_STRING(before.c_str(), w.file("config/settings.toml").c_str());
  TEST_ASSERT_EQUAL(403, w.change("DELETE", "/api/file?path=/config/contacts.toml", "", false).status);
  TEST_ASSERT_TRUE(!w.file("config/contacts.toml").empty());
  TEST_ASSERT_EQUAL(403, w.change("POST", "/api/restart", "{}", false).status);
  TEST_ASSERT_EQUAL(1, w.sim.boots());
}

void test_five_wrong_passwords_lock_everybody_out_for_a_minute() {
  Web w;
  for (int i = 0; i < 5; ++i) TEST_ASSERT_EQUAL(401, w.get("/api/state", "wrong").status);
  const Reply locked = w.get("/api/state");                        // the right password does not help now
  TEST_ASSERT_EQUAL(429, locked.status);
  w.sim.advance(30000);
  TEST_ASSERT_EQUAL(429, w.get("/api/state").status);
  w.sim.advance(31000);
  TEST_ASSERT_EQUAL(200, w.get("/api/state").status);
}

void test_four_wrong_passwords_do_not_lock_and_a_right_one_resets_the_count() {
  Web w;
  for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(401, w.get("/api/state", "wrong").status);
  TEST_ASSERT_EQUAL(200, w.get("/api/state").status);
  for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(401, w.get("/api/state", "wrong").status);
  TEST_ASSERT_EQUAL(200, w.get("/api/state").status);
}

void test_a_path_that_leaves_the_card_is_refused_and_nothing_outside_is_touched() {
  Web w;
  const fs::path outside = fs::path(w.sim.card().root()).parent_path() / "outside.txt";
  std::ofstream(outside) << "secret";
  for (const char* path : {"/../outside.txt", "/config/../../outside.txt", "/config/%2e%2e/%2e%2e/outside.txt", "//config/settings.toml",
                           "/config/settings.toml.tmp", "config/settings.toml", "/config/a b.toml"}) {
    const Reply r = w.get(std::string("/api/file?path=") + path);
    TEST_ASSERT_TRUE_MESSAGE(r.status >= 400, path);
    TEST_ASSERT_FALSE_MESSAGE(r.has("secret"), path);
  }
  const Reply write = w.change("PUT", "/api/file?path=/../written-outside.txt", "x");
  TEST_ASSERT_TRUE(write.status >= 400);
  TEST_ASSERT_FALSE(fs::exists(fs::path(w.sim.card().root()).parent_path() / "written-outside.txt"));
  const Reply del = w.change("DELETE", "/api/file?path=/../outside.txt", "");
  TEST_ASSERT_TRUE(del.status >= 400);
  TEST_ASSERT_TRUE(fs::exists(outside));
  fs::remove(outside);
}

void test_the_secrets_cannot_be_deleted_through_the_web() {
  Web w;
  const Reply r = w.change("DELETE", "/api/file?path=/config/secrets.toml", "");
  TEST_ASSERT_TRUE(r.status >= 400);
  TEST_ASSERT_TRUE(w.file("config/secrets.toml").find("test-password-1") != std::string::npos);
}

void test_a_card_without_a_password_locks_the_api() {
  Web w;
  TEST_ASSERT_TRUE(w.sim.card().write("/config/secrets.toml", "sim_pin = \"\"\n"));
  const Reply r = w.get("/api/state", "");
  TEST_ASSERT_EQUAL(403, r.status);
  TEST_ASSERT_EQUAL(403, w.get("/api/state").status);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_simulator_says_where_the_page_is_once_wifi_is_on);
  RUN_TEST(test_there_is_no_page_while_wifi_is_off);
  RUN_TEST(test_the_real_page_is_served_without_a_password);
  RUN_TEST(test_the_state_needs_the_password);
  RUN_TEST(test_the_page_can_be_had_in_every_shipped_language);
  RUN_TEST(test_a_user_catalog_on_the_card_is_offered_by_the_page);
  RUN_TEST(test_a_setting_saved_through_the_api_is_in_the_file_of_the_card);
  RUN_TEST(test_a_language_saved_through_the_api_is_the_devices_after_the_restart_the_page_asks_for);
  RUN_TEST(test_a_file_the_device_would_reject_is_not_stored_and_a_typo_is_stored_with_a_warning);
  RUN_TEST(test_the_wifi_off_button_ends_the_page);
  RUN_TEST(test_a_change_without_the_header_is_refused_and_changes_nothing);
  RUN_TEST(test_five_wrong_passwords_lock_everybody_out_for_a_minute);
  RUN_TEST(test_four_wrong_passwords_do_not_lock_and_a_right_one_resets_the_count);
  RUN_TEST(test_a_path_that_leaves_the_card_is_refused_and_nothing_outside_is_touched);
  RUN_TEST(test_the_secrets_cannot_be_deleted_through_the_web);
  RUN_TEST(test_a_card_without_a_password_locks_the_api);
  return UNITY_END();
}
