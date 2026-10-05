// Host tests for the web API: authentication, lockout, CSRF protection,
// secrets visibility, config file validation, file browser, backup/restore,
// and the language routes.
//
// Every security rule from the brief has a counter-check (the rule holds) and
// a positive test (valid credentials/headers are accepted). The PR description
// lists the mapping.
#include <unity.h>

#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <app/AppCore.h>
#include <app/WifiController.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakePower.h>
#include <fakes/FakeWifi.h>
#include <hal/ConfigStore.h>
#include <i18n/Translator.h>
#include <web/WebApi.h>

namespace {

// ---------------------------------------------------------------------------
// Fakes
// ---------------------------------------------------------------------------

// An in-memory file store with operator[] for test convenience.
struct MemStore : ConfigStore {
  std::map<std::string, std::string> files;
  int writes = 0;
  bool failWrites = false;

  bool read(const std::string& path, std::string& out, size_t maxBytes) override {
    auto it = files.find(path);
    if (it == files.end()) { out.clear(); return false; }
    out = it->second.size() <= maxBytes ? it->second : it->second.substr(0, maxBytes);
    return true;
  }
  bool readTail(const std::string& path, std::string& out, size_t maxBytes) override {
    auto it = files.find(path);
    if (it == files.end()) { out.clear(); return false; }
    const std::string& v = it->second;
    out = v.size() <= maxBytes ? v : v.substr(v.size() - maxBytes);
    return true;
  }
  int64_t fileSize(const std::string& path) override {
    auto it = files.find(path);
    return it == files.end() ? -1 : (int64_t)it->second.size();
  }
  bool write(const std::string& path, std::string_view data) override {
    if (failWrites) return false;
    ++writes;
    files[path] = std::string(data);
    return true;
  }
  bool remove(const std::string& path) override {
    return files.erase(path) > 0;
  }
  bool list(const std::string& dir, std::vector<FileEntry>& out) override {
    out.clear();
    std::string base = dir;
    if (base.back() != '/') base += '/';
    bool any = base == "/";
    std::map<std::string, FileEntry> seen;
    for (auto& kv : files) {
      if (kv.first.compare(0, base.size(), base) != 0) continue;
      any = true;
      std::string rest = kv.first.substr(base.size());
      const size_t slash = rest.find('/');
      FileEntry e;
      if (slash == std::string::npos) {
        e.name = rest;
        e.size = kv.second.size();
        e.isDir = false;
      } else {
        e.name = rest.substr(0, slash);
        e.isDir = true;
      }
      seen[e.name] = e;
    }
    if (!any) return false;
    for (auto& kv : seen) out.push_back(kv.second);
    return true;
  }
  std::string& operator[](const std::string& k) { return files[k]; }
};

struct StubAssets : WebAssets {
  const char* indexHtml(size_t& n) override {
    static const char page[] = "<html>hi</html>";
    n = sizeof(page) - 1;
    return page;
  }
};

struct StubHost : WebHost {
  int restarts = 0, wifiOffs = 0;
  void requestRestart() override { ++restarts; }
  void requestWifiOff() override { ++wifiOffs; }
};

uint32_t rnd() { return 1; }

// "parent:test-password-1" in Base64.
const char* kAuth    = "Basic cGFyZW50OnRlc3QtcGFzc3dvcmQtMQ==";
// "parent:wrong" in Base64.
const char* kBadAuth = "Basic cGFyZW50Ondyb25n";

struct Fixture {
  FakeClock    clock;
  FakePower    power{&clock};
  FakeModem    modem{clock};
  FakeGnss     gnss{power, clock};
  FakeHaptic   haptic{clock};
  FakeEventLog log{&clock};
  i18n::Translator translator;
  FakeWifi     fw;
  WifiController wifi{fw, rnd};
  MemStore     store;
  StubAssets   assets;
  StubHost     host;
  AppCore      app;
  WebApi       api;
  http::Response res;
  std::string  body;

  explicit Fixture(bool withPassword = true)
      : app{clock, power, modem, gnss, haptic, log, translator, Profile{}} ,
        api{store, assets, host, app, wifi, translator} {
    clock.advanceSeconds(100);
    clock.setEpochUtc(1789475520);  // a known time for epoch test
    clock.setTzOffsetMinutes(120);
    power.setBattery(68);
    modem.begin();
    modem.setNetwork("Example Mobile", 4);
    app.setFirmware("test-1");
    if (withPassword)
      store.files["/config/secrets.toml"] = "web_password = \"test-password-1\"\n";
  }

  // method path [body]; auth: nullptr = no header
  int call(const char* method, const char* path, const char* b = "",
           const char* auth = nullptr, bool csrf = true, uint32_t now = 1000) {
    http::Request r{};
    strncpy(r.method, method, sizeof(r.method) - 1);
    const char* q = strchr(path, '?');
    if (q) {
      strncpy(r.path, path, (size_t)(q - path));
      strncpy(r.query, q + 1, sizeof(r.query) - 1);
    } else {
      strncpy(r.path, path, sizeof(r.path) - 1);
    }
    if (auth) strncpy(r.authorization, auth, sizeof(r.authorization) - 1);
    if (csrf) strcpy(r.custom, "1");
    r.body = b;
    r.contentLength = strlen(b);
    res = http::Response{};
    api.handle(r, res, now);
    body.assign(res.body ? res.body : "", res.length);
    return res.status;
  }
  int get(const char* path, uint32_t now = 1000) {
    return call("GET", path, "", kAuth, true, now);
  }
  int put(const char* path, const char* b) {
    return call("PUT", path, b, kAuth);
  }
  int del(const char* path) {
    return call("DELETE", path, "", kAuth);
  }
  bool has(const char* needle) const {
    return body.find(needle) != std::string::npos;
  }
};

}  // namespace

void setUp()    {}
void tearDown() {}

// Secrets stored on the card for visibility tests (sms_secret is an unknown
// key in the schema and its presence tests that GET shows the raw file).
const char* kSecrets =
    "sms_secret = \"TOPSECRETWORT\"\nsim_pin = \"98765\"\n"
    "web_password = \"test-password-1\"\n\n"
    "[wifi.access_point]\nssid = \"x\"\npassword = \"APGEHEIM12\"\n\n"
    "[[wifi.network]]\nssid = \"Heim\"\npassword = \"WLANGEHEIM1\"\n";

// ─── Authentication ───────────────────────────────────────────────────────────

// Rule 1 counter-check: the page needs no password; the API does.
void test_page_needs_no_password_but_api_does() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.call("GET", "/"));
  TEST_ASSERT_TRUE(f.has("<html>"));
  TEST_ASSERT_EQUAL(401, f.call("GET", "/api/state"));
  TEST_ASSERT_EQUAL(401, f.call("GET", "/api/state", "", kBadAuth));
  TEST_ASSERT_EQUAL(200, f.get("/api/state"));
}

// Rule 1 counter-check: without a valid web_password the API is locked.
void test_without_a_web_password_the_api_is_locked_and_explains() {
  Fixture f(false);
  TEST_ASSERT_EQUAL(403, f.get("/api/state"));
  TEST_ASSERT_TRUE(f.has("secrets.toml"));
  // Too short (< 4) counts as absent.
  f.store.files["/config/secrets.toml"] = "web_password = \"abc\"\n";
  TEST_ASSERT_EQUAL(403, f.get("/api/state"));
}

// Rule 1 counter-check: unknown routes are not revealed without a login.
void test_unknown_route_is_not_revealed_without_login() {
  Fixture f;
  TEST_ASSERT_EQUAL(401, f.call("GET", "/api/nosuchroute"));
  TEST_ASSERT_EQUAL(404, f.get("/api/nosuchroute"));
  TEST_ASSERT_EQUAL(404, f.call("GET", "/etc/passwd"));
}

// Rule 2 counter-check: five wrong passwords lock everyone out for a minute.
void test_five_wrong_passwords_lock_everyone_for_a_minute() {
  Fixture f;
  for (int i = 0; i < 5; ++i)
    TEST_ASSERT_EQUAL(401, f.call("GET", "/api/state", "", kBadAuth, true, 1000 + i));
  TEST_ASSERT_EQUAL(429, f.get("/api/state", 1010));
  TEST_ASSERT_EQUAL(429, f.get("/api/state", 1000 + 4 + WebApi::kLockSeconds - 1));
  TEST_ASSERT_EQUAL(200, f.get("/api/state", 1000 + 4 + WebApi::kLockSeconds));
}

// Rule 2 counter-check: a correct password resets the failure counter.
void test_a_success_resets_the_failure_count() {
  Fixture f;
  for (int i = 0; i < 4; ++i) f.call("GET", "/api/state", "", kBadAuth);
  TEST_ASSERT_EQUAL(200, f.get("/api/state"));
  for (int i = 0; i < 4; ++i) f.call("GET", "/api/state", "", kBadAuth);
  TEST_ASSERT_EQUAL(200, f.get("/api/state"));
}

// ─── CSRF protection ─────────────────────────────────────────────────────────

// Rule 3 counter-check: mutating requests without X-Phoneplus: 1 are rejected.
void test_changing_requests_need_the_custom_header() {
  Fixture f;
  TEST_ASSERT_EQUAL(403, f.call("PUT",    "/api/file?path=/config/x.txt", "x",  kAuth, false));
  TEST_ASSERT_EQUAL(403, f.call("DELETE", "/api/file?path=/config/x.txt", "",   kAuth, false));
  TEST_ASSERT_EQUAL(403, f.call("POST",   "/api/restart",                  "",   kAuth, false));
  TEST_ASSERT_EQUAL(0, f.host.restarts);
  TEST_ASSERT_FALSE(f.store.files.count("/config/x.txt"));
  // With the header it works.
  TEST_ASSERT_EQUAL(200, f.call("PUT", "/api/file?path=/config/x.txt", "x", kAuth));
}

// ─── Secrets visibility ───────────────────────────────────────────────────────

// Rule 4 counter-check: secrets are visible to the authenticated parent only.
void test_secrets_are_visible_to_the_logged_in_parent_and_to_nobody_else() {
  Fixture f;
  f.store.files["/config/secrets.toml"] = kSecrets;
  TEST_ASSERT_EQUAL(401, f.call("GET", "/api/secrets"));
  TEST_ASSERT_FALSE(f.has("TOPSECRETWORT"));
  TEST_ASSERT_EQUAL(200, f.get("/api/secrets"));
  TEST_ASSERT_TRUE(f.has("TOPSECRETWORT"));
  TEST_ASSERT_TRUE(f.has("WLANGEHEIM1"));
  // The raw-file route also shows them.
  TEST_ASSERT_EQUAL(200, f.get("/api/file?path=/config/secrets.toml"));
  TEST_ASSERT_TRUE(f.has("98765"));
}

// Rule 4 counter-check: the state endpoint shows only presence, not values.
void test_state_does_not_leak_values() {
  Fixture f;
  f.store.files["/config/secrets.toml"] = kSecrets;
  TEST_ASSERT_EQUAL(200, f.get("/api/state"));
  TEST_ASSERT_FALSE(f.has("TOPSECRETWORT"));
  TEST_ASSERT_FALSE(f.has("98765"));
  TEST_ASSERT_FALSE(f.has("test-password-1"));
  TEST_ASSERT_TRUE(f.has("\"pin\":true"));
}

// ─── Config files as JSON ─────────────────────────────────────────────────────

void test_settings_roundtrip_json_to_toml() {
  Fixture f;
  const char* b =
      "{\"child\":{\"name\":\"Alex\"},\"display\":{\"font_scale\":2},"
      "\"tiles\":[{\"label\":\"Parent\",\"action\":\"call\",\"target\":\"+491\"}],"
      "\"quiet_hours\":[{\"days\":\"mon-fri\",\"from\":\"08:00\",\"to\":\"12:30\"}]}";
  TEST_ASSERT_EQUAL(200, f.put("/api/settings", b));
  const std::string& toml = f.store.files["/config/settings.toml"];
  TEST_ASSERT_TRUE(toml.find("[child]")      != std::string::npos);
  TEST_ASSERT_TRUE(toml.find("[[tiles]]")    != std::string::npos);
  TEST_ASSERT_TRUE(toml.find("days = \"mon-fri\"") != std::string::npos);
  TEST_ASSERT_TRUE(f.has("restart_required"));

  TEST_ASSERT_EQUAL(200, f.get("/api/settings"));
  TEST_ASSERT_TRUE(f.has("\"name\":\"Alex\""));
  TEST_ASSERT_TRUE(f.has("\"font_scale\":2"));
  TEST_ASSERT_TRUE(f.has("\"label\":\"Parent\""));
}

void test_settings_with_a_typo_key_are_accepted_but_reported() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.put("/api/settings", "{\"features\":{\"mehs\":true}}"));
  TEST_ASSERT_TRUE_MESSAGE(f.has("mehs"), f.body.c_str());
}

void test_contacts_roundtrip() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.put("/api/contacts",
    "{\"contact\":[{\"number\":\"+491701111111\",\"name\":\"Parent\",\"role\":\"parent\"}]}"));
  TEST_ASSERT_TRUE(f.store.files["/config/contacts.toml"].find("role = \"parent\"") != std::string::npos);
  TEST_ASSERT_EQUAL(200, f.get("/api/contacts"));
  TEST_ASSERT_TRUE(f.has("+491701111111"));
}

void test_a_missing_file_reads_as_an_empty_object() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.get("/api/contacts"));
  TEST_ASSERT_EQUAL_STRING("{}", f.body.c_str());
}

void test_a_broken_file_on_the_card_is_reported_with_its_line() {
  Fixture f;
  f.store.files["/config/settings.toml"] = "[child]\nname = \"ok\"\nkaputt\n";
  TEST_ASSERT_EQUAL(422, f.get("/api/settings"));
  TEST_ASSERT_TRUE_MESSAGE(f.has("line 3"), f.body.c_str());
}

// Rule 5 counter-check: invalid JSON and oversized bodies are refused.
void test_non_objects_and_oversized_bodies_are_refused() {
  Fixture f;
  TEST_ASSERT_EQUAL(400, f.put("/api/settings", "[1,2]"));
  TEST_ASSERT_EQUAL(400, f.put("/api/settings", "{broken"));
  const std::string big = "{\"x\":\"" + std::string(5000, 'a') + "\"}";
  TEST_ASSERT_EQUAL(413, f.put("/api/settings", big.c_str()));
  TEST_ASSERT_EQUAL(0, f.store.writes);
}

void test_card_write_failure_is_reported() {
  Fixture f;
  f.store.failWrites = true;
  TEST_ASSERT_EQUAL(500, f.put("/api/contacts", "{\"contact\":[]}"));
}

// ─── Secrets validation ───────────────────────────────────────────────────────

// Rule 5 counter-check: secrets rules are enforced.
void test_secrets_rules() {
  Fixture f;
  // Valid: sim_pin digits, password long enough.
  const char* good = "{\"sim_pin\":\"1234\",\"web_password\":\"test-password-1\"}";
  TEST_ASSERT_EQUAL(200, f.put("/api/secrets", good));
  // sim_pin contains non-digits.
  TEST_ASSERT_EQUAL(422, f.put("/api/secrets",
    "{\"sim_pin\":\"12\\\";AT\",\"web_password\":\"test-password-1\"}"));
  // sim_pin too short (< 4).
  TEST_ASSERT_EQUAL(422, f.put("/api/secrets",
    "{\"sim_pin\":\"123\",\"web_password\":\"test-password-1\"}"));
  // Unknown key — typo means the key silently does nothing.
  TEST_ASSERT_EQUAL(422, f.put("/api/secrets",
    "{\"sim_pni\":\"1234\",\"web_password\":\"test-password-1\"}"));
  TEST_ASSERT_TRUE(f.has("sim_pni"));
}

// Rule 5 counter-check: a short or absent web_password locks the page out.
void test_a_short_or_missing_web_password_is_refused() {
  Fixture f;
  TEST_ASSERT_EQUAL(422, f.put("/api/secrets", "{\"web_password\":\"abc\"}"));
  TEST_ASSERT_EQUAL(422, f.put("/api/secrets", "{\"sim_pin\":\"1234\"}"));
  // Original password unchanged.
  TEST_ASSERT_EQUAL_STRING("web_password = \"test-password-1\"\n",
                            f.store.files["/config/secrets.toml"].c_str());
}

// Rule 5 counter-check: a four-character password is the minimum; it is accepted.
void test_a_four_character_web_password_is_accepted() {
  Fixture f;
  // "parent:abcd" = "cGFyZW50OmFiY2Q="
  TEST_ASSERT_EQUAL(200, f.put("/api/secrets", "{\"web_password\":\"abcd\"}"));
  TEST_ASSERT_EQUAL(401, f.get("/api/state"));  // old password no longer valid
  TEST_ASSERT_EQUAL(200, f.call("GET", "/api/state", "", "Basic cGFyZW50OmFiY2Q="));
}

void test_a_changed_web_password_applies_at_once() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.put("/api/secrets", "{\"web_password\":\"new-password\"}"));
  TEST_ASSERT_EQUAL(401, f.get("/api/state"));  // old password no longer valid
}

void test_wifi_passwords_must_be_valid_and_nothing_is_half_saved() {
  Fixture f;
  const std::string before = f.store.files["/config/secrets.toml"];
  TEST_ASSERT_EQUAL(422, f.put("/api/secrets",
    "{\"web_password\":\"test-password-1\","
    "\"wifi\":{\"network\":[{\"ssid\":\"Home\",\"password\":\"short\"}]}}"));
  TEST_ASSERT_TRUE(f.store.files["/config/secrets.toml"] == before);
}

// ─── File browser ─────────────────────────────────────────────────────────────

void test_list_files() {
  Fixture f;
  f.store.files["/config/settings.toml"] = "x";
  f.store.files["/system.log"] = "abc";
  TEST_ASSERT_EQUAL(200, f.get("/api/files?path=/"));
  TEST_ASSERT_TRUE_MESSAGE(f.has("\"name\":\"config\""), f.body.c_str());
  TEST_ASSERT_TRUE(f.has("\"dir\":true"));
  TEST_ASSERT_TRUE(f.has("system" ".log"));
  TEST_ASSERT_EQUAL(200, f.get("/api/files?path=%2Fconfig"));
  TEST_ASSERT_TRUE(f.has("settings.toml"));
  TEST_ASSERT_EQUAL(404, f.get("/api/files?path=/nosuchdir"));
}

void test_raw_read_write_upload() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.call("PUT", "/api/file?path=/config/notes.txt",
                                "line1\nline2\n", kAuth));
  TEST_ASSERT_EQUAL_STRING("line1\nline2\n",
                            f.store.files["/config/notes.txt"].c_str());
  TEST_ASSERT_EQUAL(200, f.get("/api/file?path=/config/notes.txt"));
  TEST_ASSERT_EQUAL_STRING("line1\nline2\n", f.body.c_str());
  TEST_ASSERT_EQUAL(404, f.get("/api/file?path=/config/absent.txt"));
}

// Rule 5 counter-check: raw config files through the editor are also validated.
void test_raw_config_files_are_validated() {
  Fixture f;
  TEST_ASSERT_EQUAL(422,
    f.call("PUT", "/api/file?path=/config/settings.toml", "[child\nkaputt", kAuth));
  TEST_ASSERT_FALSE(f.store.files.count("/config/settings.toml"));
  TEST_ASSERT_EQUAL(200,
    f.call("PUT", "/api/file?path=/config/settings.toml",
           "# comment stays\n[child]\nname = \"x\"\n", kAuth));
  TEST_ASSERT_TRUE(f.store.files["/config/settings.toml"].find("# comment stays") !=
                   std::string::npos);
}

void test_paths_cannot_leave_the_card() {
  Fixture f;
  const char* bad[] = {
    "/../etc/passwd", "/config/../x", "config/x", "/a%2F..%2Fb", "/a b"
  };
  for (const char* p : bad) {
    const std::string url = std::string("/api/file?path=") + p;
    TEST_ASSERT_EQUAL_MESSAGE(400, f.get(url.c_str()), p);
    TEST_ASSERT_EQUAL_MESSAGE(400, f.call("PUT",    url.c_str(), "x", kAuth), p);
    TEST_ASSERT_EQUAL_MESSAGE(400, f.call("DELETE", url.c_str(), "",  kAuth), p);
  }
  TEST_ASSERT_EQUAL(400, f.get("/api/file"));
}

void test_a_truncated_file_is_marked_so_the_editor_cannot_save_it_back() {
  Fixture f;
  f.store.files["/big.txt"] = std::string(20000, 'x');
  TEST_ASSERT_EQUAL(200, f.get("/api/file?path=/big.txt"));
  TEST_ASSERT_NOT_NULL(f.res.extraHeader);
  TEST_ASSERT_NOT_NULL(strstr(f.res.extraHeader, "X-Truncated"));
  TEST_ASSERT_TRUE(f.res.length < 20000);
  f.store.files["/small.txt"] = "x";
  f.get("/api/file?path=/small.txt");
  TEST_ASSERT_NULL(f.res.extraHeader);
}

void test_logs_are_read_from_the_end() {
  Fixture f;
  f.store.files["/system.log"] = std::string(20000, 'a') + "TAIL\n";
  TEST_ASSERT_EQUAL(200, f.get("/api/file?path=/system.log"));
  TEST_ASSERT_TRUE(f.has("TAIL"));
}

// ─── Delete and the log ───────────────────────────────────────────────────────

void test_delete_a_file() {
  Fixture f;
  f.store.files["/config/old.txt"] = "x";
  TEST_ASSERT_EQUAL(200, f.del("/api/file?path=/config/old.txt"));
  TEST_ASSERT_FALSE(f.store.files.count("/config/old.txt"));
  TEST_ASSERT_EQUAL(404, f.del("/api/file?path=/config/old.txt"));
}

// Rule 5 counter-check: secrets.toml cannot be deleted.
void test_secrets_toml_cannot_be_deleted_here() {
  Fixture f;
  TEST_ASSERT_EQUAL(403, f.del("/api/file?path=/config/secrets.toml"));
  TEST_ASSERT_TRUE(f.store.files.count("/config/secrets.toml"));
}

void test_logs_may_be_deleted() {
  Fixture f;
  f.store.files["/system.log"] = "old\n";
  TEST_ASSERT_EQUAL(200, f.del("/api/file?path=/system.log"));
  TEST_ASSERT_FALSE(f.store.files.count("/system.log"));
}

// Rule 6 counter-check: every change is logged; content is never logged.
void test_every_change_is_logged_but_never_with_content() {
  Fixture f;
  f.put("/api/secrets", "{\"web_password\":\"test-password-1\",\"sim_pin\":\"4711\"}");
  // Deleting a non-existent file: no log entry.
  f.del("/api/file?path=/system.log");
  f.store.files["/system.log"] = "x";
  f.del("/api/file?path=/system.log");
  f.call("PUT", "/api/file?path=/config/n.txt", "secret-content", kAuth);
  TEST_ASSERT_TRUE(f.log.contains("WEB: /config/secrets.toml written"));
  TEST_ASSERT_TRUE(f.log.contains("log /system.log deleted"));
  TEST_ASSERT_TRUE(f.log.contains("WEB: /config/n.txt written"));
  TEST_ASSERT_FALSE(f.log.contains("4711"));
  TEST_ASSERT_FALSE(f.log.contains("test-password-1"));
  TEST_ASSERT_FALSE(f.log.contains("secret-content"));
}

// ─── Backup and restore ───────────────────────────────────────────────────────

// Rule 5 counter-check: backup does not include secrets.
void test_backup_has_settings_and_contacts_but_no_secrets() {
  Fixture f;
  f.store.files["/config/settings.toml"] = "[child]\nname = \"Alex\"\n";
  f.store.files["/config/contacts.toml"] = "[[contact]]\nnumber = \"+491\"\n";
  f.store.files["/config/secrets.toml"]  = kSecrets;
  TEST_ASSERT_EQUAL(200, f.get("/api/backup"));
  TEST_ASSERT_NOT_NULL(f.res.extraHeader);
  TEST_ASSERT_TRUE(f.has("Alex"));
  TEST_ASSERT_FALSE(f.has("TOPSECRETWORT"));
  TEST_ASSERT_FALSE(f.has("WLANGEHEIM1"));
}

void test_backup_roundtrip() {
  Fixture f;
  f.store.files["/config/settings.toml"] = "[child]\nname = \"Alex\"\n";
  f.store.files["/config/contacts.toml"] = "[[contact]]\nnumber = \"+491\"\n";
  f.get("/api/backup");
  const std::string backup = f.body;
  f.store.files.erase("/config/settings.toml");
  f.store.files.erase("/config/contacts.toml");
  TEST_ASSERT_EQUAL(200, f.call("POST", "/api/restore", backup.c_str(), kAuth));
  TEST_ASSERT_EQUAL_STRING("[child]\nname = \"Alex\"\n",
                            f.store.files["/config/settings.toml"].c_str());
  TEST_ASSERT_EQUAL_STRING("[[contact]]\nnumber = \"+491\"\n",
                            f.store.files["/config/contacts.toml"].c_str());
}

// Rule 5 counter-check: restore rejects foreign files without writing anything.
void test_restore_rejects_foreign_files_and_writes_nothing() {
  Fixture f;
  TEST_ASSERT_EQUAL(422,
    f.call("POST", "/api/restore", "{\"format\":\"other\"}", kAuth));
  TEST_ASSERT_EQUAL(422,
    f.call("POST", "/api/restore",
           "{\"format\":\"phoneplus-backup-2\",\"settings.toml\":\"[kaputt\"}", kAuth));
  TEST_ASSERT_EQUAL(0, f.store.writes);
}

// ─── Actions ─────────────────────────────────────────────────────────────────

void test_restart_and_wifi_off_reach_the_host() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.call("POST", "/api/restart",  "", kAuth));
  TEST_ASSERT_EQUAL(1, f.host.restarts);
  TEST_ASSERT_EQUAL(200, f.call("POST", "/api/wifi-off", "", kAuth));
  TEST_ASSERT_EQUAL(1, f.host.wifiOffs);
}

// Rule 1 counter-check: a request postpones the Wi-Fi auto-off timer.
void test_a_request_postpones_the_wifi_shutdown() {
  Fixture f;
  f.wifi.configure(WifiSettings{});
  f.wifi.request(WifiMode::AccessPoint, 0);
  f.get("/api/state", 500);
  // After the API call at t=500, the idle timer was reset; seconds left = 10 * 60.
  TEST_ASSERT_EQUAL_UINT32(10 * 60, f.wifi.secondsLeft(500));
}

void test_state_reports_the_device() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.get("/api/state"));
  TEST_ASSERT_TRUE(f.has("\"firmware\":\"test-1\""));
  TEST_ASSERT_TRUE(f.has("\"sim\":\"ready\""));
  TEST_ASSERT_TRUE(f.has("\"battery_percent\":68"));
}

void test_error_helper() {
  Fixture f;
  f.api.error(413, f.res);
  TEST_ASSERT_EQUAL(413, f.res.status);
}

// ─── Language routes ──────────────────────────────────────────────────────────

void test_languages_returns_a_json_array() {
  Fixture f;
  TEST_ASSERT_EQUAL(200, f.get("/api/languages"));
  // Even with an empty translator the response is a valid JSON array.
  TEST_ASSERT_TRUE(f.has("["));
}

void test_lang_catalog_for_unknown_code_is_404() {
  Fixture f;
  TEST_ASSERT_EQUAL(404, f.get("/api/lang/zzz"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_page_needs_no_password_but_api_does);
  RUN_TEST(test_without_a_web_password_the_api_is_locked_and_explains);
  RUN_TEST(test_unknown_route_is_not_revealed_without_login);
  RUN_TEST(test_five_wrong_passwords_lock_everyone_for_a_minute);
  RUN_TEST(test_a_success_resets_the_failure_count);
  RUN_TEST(test_changing_requests_need_the_custom_header);
  RUN_TEST(test_secrets_are_visible_to_the_logged_in_parent_and_to_nobody_else);
  RUN_TEST(test_state_does_not_leak_values);
  RUN_TEST(test_settings_roundtrip_json_to_toml);
  RUN_TEST(test_settings_with_a_typo_key_are_accepted_but_reported);
  RUN_TEST(test_contacts_roundtrip);
  RUN_TEST(test_a_missing_file_reads_as_an_empty_object);
  RUN_TEST(test_a_broken_file_on_the_card_is_reported_with_its_line);
  RUN_TEST(test_non_objects_and_oversized_bodies_are_refused);
  RUN_TEST(test_card_write_failure_is_reported);
  RUN_TEST(test_secrets_rules);
  RUN_TEST(test_a_short_or_missing_web_password_is_refused);
  RUN_TEST(test_a_four_character_web_password_is_accepted);
  RUN_TEST(test_a_changed_web_password_applies_at_once);
  RUN_TEST(test_wifi_passwords_must_be_valid_and_nothing_is_half_saved);
  RUN_TEST(test_list_files);
  RUN_TEST(test_raw_read_write_upload);
  RUN_TEST(test_raw_config_files_are_validated);
  RUN_TEST(test_paths_cannot_leave_the_card);
  RUN_TEST(test_a_truncated_file_is_marked_so_the_editor_cannot_save_it_back);
  RUN_TEST(test_logs_are_read_from_the_end);
  RUN_TEST(test_delete_a_file);
  RUN_TEST(test_secrets_toml_cannot_be_deleted_here);
  RUN_TEST(test_logs_may_be_deleted);
  RUN_TEST(test_every_change_is_logged_but_never_with_content);
  RUN_TEST(test_backup_has_settings_and_contacts_but_no_secrets);
  RUN_TEST(test_backup_roundtrip);
  RUN_TEST(test_restore_rejects_foreign_files_and_writes_nothing);
  RUN_TEST(test_restart_and_wifi_off_reach_the_host);
  RUN_TEST(test_a_request_postpones_the_wifi_shutdown);
  RUN_TEST(test_state_reports_the_device);
  RUN_TEST(test_error_helper);
  RUN_TEST(test_languages_returns_a_json_array);
  RUN_TEST(test_lang_catalog_for_unknown_code_is_404);
  return UNITY_END();
}
