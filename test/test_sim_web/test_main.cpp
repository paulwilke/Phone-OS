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
#include <web/WebServer.h>

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

  uint32_t lastSimMs = 0;  // the time of the simulated device that the last request took

  Reply send(const std::string& request) {
    auto answer = std::async(std::launch::async, [this, request] { return talk(port, request); });
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    lastSimMs = 0;
    while (answer.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
      sim.advance(20);
      lastSimMs += 20;
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

// ---- connections ------------------------------------------------------------------------------------------

// A browser keeps spare connections open that never send a byte. The device serves one connection at a time,
// so such a connection must not hold the next request up for the whole idle time.
void test_a_spare_connection_that_says_nothing_does_not_hold_the_page_up() {
  Web w;
  std::vector<int> spare;
  for (int i = 0; i < 3; ++i) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(w.port);
    TEST_ASSERT_EQUAL(0, ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)));
    spare.push_back(fd);
  }
  const Reply state = w.get("/api/state");
  TEST_ASSERT_EQUAL(200, state.status);
  // three spare connections, each let go after the first-byte time; with the whole idle time each it would be 30 s
  TEST_ASSERT_TRUE_MESSAGE(w.lastSimMs <= 3 * (web::WebServer::kFirstByteSeconds + 1) * 1000, std::to_string(w.lastSimMs).c_str());
  for (int fd : spare) ::close(fd);
}

// A request that ends with an error status leaves a line in the device's log: method, path without query values,
// status, bytes sent of total; and nothing of the password or the body.
void test_a_request_with_an_error_status_is_in_the_log_without_secrets() {
  Web w;
  TEST_ASSERT_EQUAL(404, w.get("/api/file?path=/config/nothing-here.toml").status);
  TEST_ASSERT_EQUAL(401, w.get("/api/state", "wrong").status);
  TEST_ASSERT_EQUAL(200, w.get("/api/state").status);
  std::string log;
  for (const std::string& line : w.sim.log().lines()) log += line + "\n";
  TEST_ASSERT_TRUE(log.find("WEB: GET /api/file?path -> 404, sent ") != std::string::npos);
  TEST_ASSERT_TRUE(log.find("WEB: GET /api/state -> 401, sent ") != std::string::npos);
  TEST_ASSERT_TRUE(log.find("GET /api/state -> 200") == std::string::npos);
  TEST_ASSERT_TRUE(log.find("nothing-here") == std::string::npos);
  TEST_ASSERT_TRUE(log.find("wrong") == std::string::npos);
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

// The form asks for the label and the help text of every field. The help texts come as a second answer
// (/api/lang/<code>/help) because all of a language in one answer is too tight for the response buffer;
// both answers must fit it with room for a longer user catalog, and between them they carry every key
// the form uses, so that the page never shows a raw key.
void test_every_text_the_form_asks_for_arrives_in_every_shipped_language_and_each_answer_has_room_to_spare() {
  std::ifstream in("config/form_generated.json");
  std::stringstream form;
  form << in.rdbuf();
  const std::string text = form.str();
  std::vector<std::string> keys;
  for (size_t at = text.find("\"config."); at != std::string::npos; at = text.find("\"config.", at + 1)) {
    const size_t end = text.find('"', at + 1);
    keys.push_back(text.substr(at, end - at + 1));
  }
  TEST_ASSERT_TRUE(keys.size() > 80);
  Web w;
  for (const char* code : {"en", "de", "fr"}) {
    const Reply main = w.get(std::string("/api/lang/") + code);
    const Reply help = w.get(std::string("/api/lang/") + code + "/help");
    TEST_ASSERT_EQUAL(200, main.status);
    TEST_ASSERT_EQUAL(200, help.status);
    // Each answer uses at most 6000 of the 12288 bytes of the buffer: twice the room for a catalog whose
    // sentences are longer, which a single answer (about 10.2 KB in French) would not have.
    TEST_ASSERT_TRUE_MESSAGE(main.body.size() < 6000, code);
    TEST_ASSERT_TRUE_MESSAGE(help.body.size() < 6000, code);
    for (const std::string& key : keys) {
      const bool isHelp = key.size() > 6 && key.compare(key.size() - 6, 6, ".help\"") == 0;
      TEST_ASSERT_TRUE_MESSAGE((isHelp ? help : main).has(key + ":"), (std::string(code) + " " + key).c_str());
    }
  }
}

// Help texts are behind the password like every other route, and an unknown language has none.
void test_the_help_texts_need_the_password_and_a_known_language() {
  Web w;
  TEST_ASSERT_EQUAL(401, w.get("/api/lang/en/help", "").status);
  TEST_ASSERT_EQUAL(404, w.get("/api/lang/zz/help").status);
}

// The page's words are in memory while the page can be reached and let go when Wi-Fi goes off.
void test_the_page_catalogs_are_loaded_by_the_page_and_let_go_when_wifi_is_off() {
  Web w;
  TEST_ASSERT_FALSE(w.sim.translator().webLoaded());                                 // the device started without them
  TEST_ASSERT_EQUAL(200, w.get("/api/lang/de").status);
  TEST_ASSERT_TRUE(w.sim.translator().webLoaded());
  w.sim.execute("wifi off");
  w.sim.advance(2000);
  TEST_ASSERT_FALSE(w.sim.translator().webLoaded());
  w.sim.execute("wifi ap");
  w.sim.advance(2000);
  w.port = w.sim.webPort();
  TEST_ASSERT_EQUAL(200, w.get("/api/lang/de").has("Anmelden") ? 200 : 0);           // and they come back when asked again
}

// A catalog for the page is /lang/web/<code>.toml: it changes the page's words in that language and
// leaves the device's words alone; a bad one is refused with a line in the log, and the page still works.
void test_a_user_catalog_for_the_page_changes_the_page_and_not_the_device() {
  Web w;
  const Reply before = w.get("/api/lang/de");
  TEST_ASSERT_TRUE(before.has("Anmelden"));
  const std::string head = "[meta]\ncode = \"de\"\nname = \"German\"\nnative = \"Deutsch\"\nplural = \"one-other\"\n\n[strings]\n";
  TEST_ASSERT_TRUE(w.sim.card().write("/lang/readme.txt", "the card has a lang/ folder"));  // a folder is made one level at a time
  TEST_ASSERT_TRUE(w.sim.card().write("/lang/web/de.toml", head + "web.auth.enter = \"Reinkommen\"\n"));
  TEST_ASSERT_TRUE(w.sim.card().write("/lang/web/fr.toml", std::string(head).replace(head.find("\"de\""), 4, "\"fr\"") + "web.auth.enter = \"kaputt {nom}\"\n"));   // a placeholder the page does not have: refused
  w.sim.execute("run /dev/null");
  TEST_ASSERT_EQUAL(200, w.change("POST", "/api/restart", "{}").status);
  w.sim.advance(2000);
  w.sim.execute("wifi ap");
  w.sim.advance(2000);
  w.port = w.sim.webPort();
  const Reply after = w.get("/api/lang/de");
  TEST_ASSERT_TRUE(after.has("Reinkommen"));
  TEST_ASSERT_FALSE(after.has("Anmelden"));                                         // the word it replaced
  TEST_ASSERT_TRUE(after.has("web.title"));                                         // everything else is still there
  const std::string home = w.sim.translator().getIn("de", "home.phone");
  TEST_ASSERT_EQUAL_STRING("Telefon", home.c_str());                                 // the device's words did not move
  TEST_ASSERT_FALSE(w.get("/api/lang/en").has("Reinkommen"));
  const Reply french = w.get("/api/lang/fr");
  TEST_ASSERT_EQUAL(200, french.status);
  TEST_ASSERT_FALSE(french.has("kaputt"));                                          // the catalog with the wrong placeholder was refused
}

void test_a_user_catalog_on_the_card_is_offered_by_the_page() {
  Web w;
  const std::string english =
      "[meta]\ncode   = \"xx\"\nname   = \"Test language\"\nnative = \"Xtest\"\nplural = \"one-other\"\n\n[strings]\nhome.phone = \"Xphone\"\n";
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

void test_the_log_tab_reads_and_clears_the_log_file_of_the_card_through_the_file_route() {
  Web w;
  TEST_ASSERT_EQUAL(404, w.get("/api/file?path=/system.log").status);          // no log yet: the page shows an empty one
  TEST_ASSERT_TRUE(w.sim.card().write("/system.log", "boot ok\nWEB: something happened\n"));
  const Reply log = w.get("/api/file?path=/system.log");
  TEST_ASSERT_EQUAL(200, log.status);
  TEST_ASSERT_TRUE(log.has("WEB: something happened"));
  TEST_ASSERT_EQUAL(403, w.change("DELETE", "/api/file?path=/system.log", "", /*header=*/false).status);
  TEST_ASSERT_TRUE(w.sim.card().fileSize("/system.log") > 0);
  TEST_ASSERT_EQUAL(200, w.change("DELETE", "/api/file?path=/system.log", "{}").status);
  TEST_ASSERT_EQUAL(404, w.get("/api/file?path=/system.log").status);
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
  RUN_TEST(test_a_spare_connection_that_says_nothing_does_not_hold_the_page_up);
  RUN_TEST(test_a_request_with_an_error_status_is_in_the_log_without_secrets);
  RUN_TEST(test_the_page_can_be_had_in_every_shipped_language);
  RUN_TEST(test_every_text_the_form_asks_for_arrives_in_every_shipped_language_and_each_answer_has_room_to_spare);
  RUN_TEST(test_the_help_texts_need_the_password_and_a_known_language);
  RUN_TEST(test_the_page_catalogs_are_loaded_by_the_page_and_let_go_when_wifi_is_off);
  RUN_TEST(test_a_user_catalog_for_the_page_changes_the_page_and_not_the_device);
  RUN_TEST(test_a_user_catalog_on_the_card_is_offered_by_the_page);
  RUN_TEST(test_a_setting_saved_through_the_api_is_in_the_file_of_the_card);
  RUN_TEST(test_a_language_saved_through_the_api_is_the_devices_after_the_restart_the_page_asks_for);
  RUN_TEST(test_a_file_the_device_would_reject_is_not_stored_and_a_typo_is_stored_with_a_warning);
  RUN_TEST(test_the_log_tab_reads_and_clears_the_log_file_of_the_card_through_the_file_route);
  RUN_TEST(test_the_wifi_off_button_ends_the_page);
  RUN_TEST(test_a_change_without_the_header_is_refused_and_changes_nothing);
  RUN_TEST(test_five_wrong_passwords_lock_everybody_out_for_a_minute);
  RUN_TEST(test_four_wrong_passwords_do_not_lock_and_a_right_one_resets_the_count);
  RUN_TEST(test_a_path_that_leaves_the_card_is_refused_and_nothing_outside_is_touched);
  RUN_TEST(test_the_secrets_cannot_be_deleted_through_the_web);
  RUN_TEST(test_a_card_without_a_password_locks_the_api);
  return UNITY_END();
}
