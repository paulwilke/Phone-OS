#include "WebApi.h"

#include <cstdio>
#include <cstring>

#include "../app/SafePath.h"
#include "../app/Toml.h"
#include "../app/WifiController.h"
#include "../config/ConfigLoader.h"
#include "../config/Diagnostic.h"
#include "Json.h"

namespace {

bool isMethod(const http::Request& r, const char* m) { return strcmp(r.method, m) == 0; }

const char* simText(SimStatus s) {
  switch (s) {
    case SimStatus::Ready:       return "ready";
    case SimStatus::Missing:     return "missing";
    case SimStatus::PinRequired: return "pin_required";
    case SimStatus::PinWrong:    return "pin_wrong";
    case SimStatus::PukRequired: return "puk_required";
    default:                     return "unknown";
  }
}

const char* wifiPhaseText(WifiPhase p) {
  switch (p) {
    case WifiPhase::Connecting:  return "connecting";
    case WifiPhase::Connected:   return "station";
    case WifiPhase::AccessPoint: return "access_point";
    case WifiPhase::Failed:      return "failed";
    default:                     return "off";
  }
}

bool allDigits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) if (c < '0' || c > '9') return false;
  return true;
}

// URL-decode `name=value` from a query string.
bool queryValue(const char* query, const char* name, char* out, size_t size) {
  const size_t n = strlen(name);
  const char* p = query;
  while (*p) {
    if (strncmp(p, name, n) == 0 && p[n] == '=') {
      p += n + 1;
      size_t o = 0;
      while (*p && *p != '&') {
        char c = *p++;
        if (c == '+') {
          c = ' ';
        } else if (c == '%' && p[0] && p[1]) {
          char hex[3] = {p[0], p[1], '\0'};
          char* e = nullptr;
          const long v = strtol(hex, &e, 16);
          if (*e) return false;
          c = (char)v;
          p += 2;
        }
        if (o + 1 >= size) return false;
        out[o++] = c;
      }
      out[o] = '\0';
      return true;
    }
    while (*p && *p != '&') ++p;
    if (*p == '&') ++p;
  }
  return false;
}

// A minimal in-memory store that exposes a single file. Used for validation.
class OneFileStore : public ConfigStore {
public:
  std::string path, data;

  bool read(const std::string& p, std::string& out, size_t maxBytes) override {
    if (p != path) { out.clear(); return false; }
    out = data.size() <= maxBytes ? data : data.substr(0, maxBytes);
    return true;
  }
  bool readTail(const std::string& p, std::string& out, size_t maxBytes) override {
    if (p != path) { out.clear(); return false; }
    out = data.size() <= maxBytes ? data : data.substr(data.size() - maxBytes);
    return true;
  }
  int64_t fileSize(const std::string& p) override {
    return p == path ? (int64_t)data.size() : -1;
  }
  bool write(const std::string&, std::string_view) override { return false; }
  bool remove(const std::string&) override { return false; }
  bool list(const std::string&, std::vector<FileEntry>&) override { return false; }
};

std::string formatDiagnostic(const config::Diagnostic& d) {
  std::string msg = config::diagnosticName(d.code);
  for (const auto& arg : d.args) {
    msg += ": ";
    msg += arg;
  }
  return msg;
}

// Append a JSON-escaped string to buf[pos], returning false when it does not fit.
bool appendJsonStr(char* buf, size_t cap, size_t& pos, const std::string& s) {
  auto put = [&](char c) -> bool {
    if (pos + 1 >= cap) return false;
    buf[pos++] = c;
    return true;
  };
  if (!put('"')) return false;
  for (unsigned char c : s) {
    if (c == '"')       { if (!put('\\') || !put('"'))  return false; }
    else if (c == '\\') { if (!put('\\') || !put('\\')) return false; }
    else if (c == '\n') { if (!put('\\') || !put('n'))  return false; }
    else if (c == '\r') { if (!put('\\') || !put('r'))  return false; }
    else if (c == '\t') { if (!put('\\') || !put('t'))  return false; }
    else if (c < 0x20) {
      char esc[8];
      snprintf(esc, sizeof(esc), "\\u%04x", c);
      for (const char* q = esc; *q; ++q) if (!put(*q)) return false;
    } else {
      if (!put((char)c)) return false;
    }
  }
  return put('"');
}

}  // namespace

WebApi::WebApi(ConfigStore& store, WebAssets& assets, WebHost& host, AppCore& app,
               WifiController& wifi, i18n::Translator& translator)
    : store_(store), assets_(assets), host_(host), app_(app), wifi_(wifi),
      translator_(translator) {}

// ---------------------------------------------------------------------------
// Response helpers
// ---------------------------------------------------------------------------

void WebApi::jsonResp(http::Response& res, int status, const char* body) {
  res.status = status;
  res.contentType = "application/json; charset=utf-8";
  res.body = body;
  res.length = strlen(body);
}

void WebApi::jsonError(http::Response& res, int status, const char* message) {
  // {"error":"..."}
  size_t n = 0;
  resp_[n++] = '{';
  resp_[n++] = '"'; resp_[n++] = 'e'; resp_[n++] = 'r'; resp_[n++] = 'r';
  resp_[n++] = 'o'; resp_[n++] = 'r'; resp_[n++] = '"'; resp_[n++] = ':';
  appendJsonStr(resp_, sizeof(resp_), n, message);
  resp_[n++] = '}';
  resp_[n] = '\0';
  jsonResp(res, status, resp_);
}

void WebApi::error(int status, http::Response& res) {
  jsonError(res, status, http::reasonPhrase(status));
}

void WebApi::ok(http::Response& res, const char* warnings) {
  size_t n = 0;
  const auto put = [&](const char* s) { while (*s && n + 1 < sizeof(resp_)) resp_[n++] = *s++; };
  put("{\"ok\":true,\"restart_required\":true");
  if (warnings && *warnings) {
    put(",\"warnings\":");
    appendJsonStr(resp_, sizeof(resp_), n, warnings);
  }
  put("}");
  resp_[n] = '\0';
  jsonResp(res, 200, resp_);
}

// ---------------------------------------------------------------------------
// Authentication
// ---------------------------------------------------------------------------

bool WebApi::webPassword(char* out, size_t size) {
  out[0] = '\0';
  std::string text;
  if (!store_.read(kSecretsPath, text, kMaxConfigFile)) return false;
  if (text.empty()) return false;

  OneFileStore vs;
  vs.path = kSecretsPath;
  vs.data = text;
  config::ConfigLoader loader(vs);
  const auto result = loader.load();
  // Zero the secrets from memory.
  std::fill(text.begin(), text.end(), '\0');

  snprintf(out, size, "%s", result.webPassword.c_str());
  return strlen(out) >= kMinWebPassword;
}

// Returns false when authentication fails; the response is already set then.
bool WebApi::authenticate(const http::Request& req, http::Response& res, uint32_t now) {
  char expected[96];
  if (!webPassword(expected, sizeof(expected))) {
    memset(expected, 0, sizeof(expected));
    jsonError(res, 403,
      "No web password: enter web_password (at least 4 characters) in /config/secrets.toml "
      "(scripts/deviceconfig.py write secrets.toml ...)");
    return false;
  }

  if (locked_) {
    if ((int32_t)(now - lockedUntil_) < 0) {
      memset(expected, 0, sizeof(expected));
      jsonError(res, 429, "Too many failed attempts, wait one minute");
      return false;
    }
    locked_ = false;
    failures_ = 0;
  }

  char given[96];
  const bool parsed = http::basicPassword(req.authorization, given, sizeof(given));
  const bool good = parsed && http::constantTimeEquals(given, expected);
  memset(expected, 0, sizeof(expected));
  memset(given, 0, sizeof(given));

  if (!good) {
    if (++failures_ >= kMaxFailures) {
      locked_ = true;
      lockedUntil_ = now + kLockSeconds;
    }
    jsonError(res, 401, "Wrong password");
    return false;
  }
  failures_ = 0;
  return true;
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void WebApi::handle(const http::Request& req, http::Response& res, uint32_t now) {
  const bool get = isMethod(req, "GET");

  // The page itself needs no password — it is the login form.
  if (strcmp(req.path, "/") == 0 || strcmp(req.path, "/index.html") == 0) {
    if (!get) return jsonError(res, 405, "GET only");
    size_t len = 0;
    const char* page = assets_.indexHtml(len);
    res.status = 200;
    res.contentType = "text/html; charset=utf-8";
    res.body = page;
    res.length = len;
    return;
  }

  if (strncmp(req.path, "/api/", 5) != 0) return jsonError(res, 404, "Not found");

  // Authenticate before revealing whether a route exists.
  if (!authenticate(req, res, now)) return;
  wifi_.noteActivity(now);

  if (!get && strcmp(req.custom, "1") != 0)
    return jsonError(res, 403, "Header X-Phoneplus: 1 missing");

  const char* p = req.path + 4;  // "/..."

  if (get) {
    if (strcmp(p, "/state")    == 0) return state(res, now);
    if (strcmp(p, "/settings") == 0) return getConfigJson(kSettingsPath, res);
    if (strcmp(p, "/contacts") == 0) return getConfigJson(kContactsPath, res);
    if (strcmp(p, "/secrets")  == 0) return getConfigJson(kSecretsPath,  res);
    if (strcmp(p, "/files")    == 0) return listFiles(req, res);
    if (strcmp(p, "/file")     == 0) return getFile(req, res);
    if (strcmp(p, "/backup")   == 0) return backup(res);
    if (strcmp(p, "/languages") == 0) return languages(res);
    if (strncmp(p, "/lang/", 6) == 0) return langCatalog(p + 6, res);
  } else if (isMethod(req, "PUT") || isMethod(req, "POST")) {
    if (strcmp(p, "/settings") == 0) return putConfigJson(kSettingsPath, req, res);
    if (strcmp(p, "/contacts") == 0) return putConfigJson(kContactsPath, req, res);
    if (strcmp(p, "/secrets")  == 0) return putConfigJson(kSecretsPath,  req, res);
    if (strcmp(p, "/file")     == 0) return putFile(req, res);
    if (strcmp(p, "/restore")  == 0) return restore(req, res);
    if (strcmp(p, "/restart")  == 0) {
      app_.log("WEB: restart requested");
      host_.requestRestart();
      jsonResp(res, 200, "{\"ok\":true}");
      return;
    }
    if (strcmp(p, "/wifi-off") == 0) {
      host_.requestWifiOff();
      jsonResp(res, 200, "{\"ok\":true}");
      return;
    }
  } else if (isMethod(req, "DELETE")) {
    if (strcmp(p, "/file") == 0) return deleteFile(req, res);
  }
  jsonError(res, 404, "Not found");
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void WebApi::state(http::Response& res, uint32_t now) {
  // Read the password-and-secrets presence without logging content.
  bool pinSet = false, pwSet = false;
  {
    std::string text;
    if (store_.read(kSecretsPath, text, kMaxConfigFile)) {
      OneFileStore vs;
      vs.path = kSecretsPath; vs.data = text;
      config::ConfigLoader loader(vs);
      const auto r = loader.load();
      pinSet = !r.simPin.empty();
      pwSet  = r.webPassword.size() >= kMinWebPassword;
      std::fill(text.begin(), text.end(), '\0');
    }
  }

  const AppCore::State s = app_.state();
  const WifiPhase wp = wifi_.phase();

  size_t n = 0;
  auto puts = [&](const char* str) { while (*str && n + 1 < sizeof(resp_)) resp_[n++] = *str++; };
  auto putInt = [&](int64_t v) {
    char tmp[24]; snprintf(tmp, sizeof(tmp), "%lld", (long long)v); puts(tmp);
  };
  auto putBool = [&](bool v) { puts(v ? "true" : "false"); };

  puts("{\"firmware\":");    appendJsonStr(resp_, sizeof(resp_), n, app_.firmware());
  puts(",\"child\":");       appendJsonStr(resp_, sizeof(resp_), n, app_.profile().childName);
  puts(",\"battery_percent\":"); putInt(s.batteryPercent);
  puts(",\"sim\":");         appendJsonStr(resp_, sizeof(resp_), n, simText(s.sim));
  puts(",\"voice\":");       putBool(s.voiceAvailable);
  puts(",\"epoch\":");       putInt(app_.epochUtc());
  puts(",\"tz_offset_minutes\":"); putInt(app_.tzOffsetMinutes());
  puts(",\"wifi\":{\"state\":");
  appendJsonStr(resp_, sizeof(resp_), n, wifiPhaseText(wp));
  puts(",\"ssid\":"); appendJsonStr(resp_, sizeof(resp_), n, wifi_.ssid());
  puts(",\"seconds_left\":"); putInt(wifi_.secondsLeft(now));
  puts("}");
  puts(",\"set\":{\"pin\":"); putBool(pinSet);
  puts(",\"web_password\":"); putBool(pwSet);
  puts("}}");
  resp_[n] = '\0';

  jsonResp(res, 200, resp_);
}

// ---------------------------------------------------------------------------
// Config files as JSON
// ---------------------------------------------------------------------------

void WebApi::getConfigJson(const char* path, http::Response& res) {
  std::string text;
  const bool found = store_.read(path, text, kMaxConfigFile + 1);
  if (!found || text.empty()) {
    jsonResp(res, 200, "{}");
    return;
  }

  toml::Value root;
  const toml::Error e = toml::parse(text, root);
  if (path == kSecretsPath) std::fill(text.begin(), text.end(), '\0');

  if (e.failed()) {
    char msg[160];
    snprintf(msg, sizeof(msg), "%s line %d: %s — use the raw editor to fix it",
             path, e.line, toml::codeName(e.code));
    return jsonError(res, 422, msg);
  }

  const size_t n = web::json::fromToml(root, resp_, sizeof(resp_));
  if (n == 0) return jsonError(res, 500, "File too large");

  if (path == kSecretsPath) std::fill(resp_, resp_ + n, '\0');  // zero then rebuild
  // (fromToml already wrote resp_; keep it as-is for the response)
  if (path != kSecretsPath)
    ;  // resp_ already holds the JSON
  else {
    // Re-run after zeroing — just produce the same output again.
    web::json::fromToml(root, resp_, sizeof(resp_));
  }

  jsonResp(res, 200, resp_);
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool WebApi::checkConfigText(const char* path, const char* text, size_t len,
                              std::string& why, std::string& warnings) {
  const bool isSettings = strcmp(path, kSettingsPath) == 0;
  const bool isContacts = strcmp(path, kContactsPath) == 0;
  const bool isSecrets  = strcmp(path, kSecretsPath)  == 0;
  if (!isSettings && !isContacts && !isSecrets) return true;

  OneFileStore vs;
  vs.path = path;
  vs.data = std::string(text, len);
  config::ConfigLoader loader(vs);
  const auto result = loader.load();

  for (const auto& d : result.diagnostics) {
    if (d.code == config::Diagnostic::Code::MissingFile) continue;

    const std::string msg = [&]() -> std::string {
      if (d.code == config::Diagnostic::Code::SyntaxError && d.args.size() >= 3) {
        return std::string(d.args[0]) + " line " + d.args[1] + ": " + d.args[2];
      }
      return formatDiagnostic(d);
    }();

    if (isSecrets) {
      why = msg;
      return false;
    }
    if (d.code == config::Diagnostic::Code::SyntaxError) {
      why = msg;
      return false;
    }
    if (!warnings.empty()) warnings += "; ";
    warnings += msg;
  }

  if (isSecrets) {
    if (result.webPassword.size() < kMinWebPassword) {
      why = "web_password: at least 4 characters required";
      return false;
    }
    if (!result.simPin.empty() && (result.simPin.size() < 4 || !allDigits(result.simPin))) {
      why = "sim_pin: 4 to 8 digits or empty";
      return false;
    }
  }
  return true;
}

bool WebApi::commit(const char* path, const char* data, size_t len,
                    http::Response& res, const std::string& warnings) {
  if (!store_.write(path, std::string_view(data, len))) {
    jsonError(res, 500, "Write to card failed");
    return false;
  }
  char line[128];
  snprintf(line, sizeof(line), "WEB: %s written (%u bytes)", path, (unsigned)len);
  app_.log(line);
  ok(res, warnings.c_str());
  return true;
}

void WebApi::putConfigJson(const char* path, const http::Request& req, http::Response& res) {
  if (req.contentLength == 0 || req.contentLength > kMaxConfigFile)
    return jsonError(res, 413, "Too large or empty");

  toml::Value root;
  std::string parseError;
  if (!web::json::toToml(req.body, req.contentLength, root, parseError) ||
      root.type() != toml::Value::Type::Table)
    return jsonError(res, 400, "Expected a JSON object");

  const std::string text = toml::emit(root);
  if (text.size() > kMaxConfigFile) return jsonError(res, 413, "Result too large");

  std::string why, warnings;
  if (!checkConfigText(path, text.c_str(), text.size(), why, warnings))
    return jsonError(res, 422, why.c_str());
  commit(path, text.c_str(), text.size(), res, warnings);
}

// ---------------------------------------------------------------------------
// File browser
// ---------------------------------------------------------------------------

bool WebApi::queryPath(const http::Request& req, char* out, size_t size, http::Response& res) {
  if (!queryValue(req.query, "path", out, size) || !safepath::valid(out)) {
    jsonError(res, 400, "path= missing or not a valid path");
    return false;
  }
  return true;
}

void WebApi::listFiles(const http::Request& req, http::Response& res) {
  char path[safepath::kMaxLength + 1];
  if (!queryValue(req.query, "path", path, sizeof(path))) snprintf(path, sizeof(path), "/");
  if (!safepath::valid(path)) return jsonError(res, 400, "Not a valid path");

  std::vector<FileEntry> entries;
  if (!store_.list(path, entries)) return jsonError(res, 404, "Directory not found");

  size_t n = 0;
  auto puts = [&](const char* s) { while (*s && n + 1 < sizeof(resp_)) resp_[n++] = *s++; };

  puts("{\"path\":");
  appendJsonStr(resp_, sizeof(resp_), n, path);
  puts(",\"entries\":[");
  bool first = true;
  for (const auto& e : entries) {
    if (!first) puts(",");
    first = false;
    puts("{\"name\":");
    appendJsonStr(resp_, sizeof(resp_), n, e.name);
    char tmp[32];
    snprintf(tmp, sizeof(tmp), ",\"size\":%llu,\"dir\":", (unsigned long long)e.size);
    puts(tmp);
    puts(e.isDir ? "true}" : "false}");
  }
  puts("]}");
  if (n + 1 >= sizeof(resp_)) return jsonError(res, 500, "Too many entries");
  resp_[n] = '\0';

  jsonResp(res, 200, resp_);
}

void WebApi::getFile(const http::Request& req, http::Response& res) {
  char path[safepath::kMaxLength + 1];
  if (!queryPath(req, path, sizeof(path), res)) return;
  const int64_t size = store_.fileSize(path);
  if (size < 0) return jsonError(res, 404, "File not found");

  const bool tail = safepath::endsWith(path, ".log");
  const size_t cap = sizeof(resp_) - 1;
  std::string text;
  if (tail) store_.readTail(path, text, cap);
  else      store_.read(path, text, cap);

  const size_t n = text.size() <= cap ? text.size() : cap;
  memcpy(resp_, text.data(), n);
  resp_[n] = '\0';

  res.status = 200;
  res.contentType = "text/plain; charset=utf-8";
  res.body = resp_;
  res.length = n;
  if ((size_t)size > n) res.extraHeader = "X-Truncated: 1";
}

void WebApi::putFile(const http::Request& req, http::Response& res) {
  char path[safepath::kMaxLength + 1];
  if (!queryPath(req, path, sizeof(path), res)) return;
  if (strlen(path) < 2 || path[strlen(path) - 1] == '/')
    return jsonError(res, 400, "A file name is required");
  if (req.contentLength > http::kMaxBody) return jsonError(res, 413, "Too large");

  std::string why, warnings;
  if (safepath::inConfigDir(path) &&
      !checkConfigText(path, req.body, req.contentLength, why, warnings))
    return jsonError(res, 422, why.c_str());
  commit(path, req.body, req.contentLength, res, warnings);
}

void WebApi::deleteFile(const http::Request& req, http::Response& res) {
  char path[safepath::kMaxLength + 1];
  if (!queryPath(req, path, sizeof(path), res)) return;
  if (strcmp(path, kSecretsPath) == 0)
    return jsonError(res, 403, "secrets.toml holds the web password and cannot be deleted here");
  if (store_.fileSize(path) < 0) return jsonError(res, 404, "File not found");
  if (!store_.remove(path)) return jsonError(res, 500, "Delete failed");

  char line[128];
  snprintf(line, sizeof(line), "WEB: %s %s deleted",
           safepath::endsWith(path, ".log") ? "log" : "file", path);
  app_.log(line);
  ok(res);
}

// ---------------------------------------------------------------------------
// Backup and restore
// ---------------------------------------------------------------------------

void WebApi::backup(http::Response& res) {
  std::string settingsText, contactsText;
  store_.read(kSettingsPath, settingsText, kMaxConfigFile);
  store_.read(kContactsPath, contactsText, kMaxConfigFile);

  size_t n = 0;
  auto puts = [&](const char* s) { while (*s && n + 1 < sizeof(resp_)) resp_[n++] = *s++; };
  puts("{\"format\":\"phoneplus-backup-2\",\"settings.toml\":");
  if (!appendJsonStr(resp_, sizeof(resp_), n, settingsText)) return jsonError(res, 500, "Backup too large");
  puts(",\"contacts.toml\":");
  if (!appendJsonStr(resp_, sizeof(resp_), n, contactsText)) return jsonError(res, 500, "Backup too large");
  puts("}");
  if (n + 1 >= sizeof(resp_)) return jsonError(res, 500, "Backup too large");
  resp_[n] = '\0';

  jsonResp(res, 200, resp_);
  res.extraHeader = "Content-Disposition: attachment; filename=\"phoneplus-backup.json\"";
}

void WebApi::restore(const http::Request& req, http::Response& res) {
  if (req.contentLength == 0) return jsonError(res, 400, "Empty body");

  char format[48];
  if (!web::json::getString(req.body, req.contentLength, "format", format, sizeof(format)) ||
      strcmp(format, "phoneplus-backup-2") != 0)
    return jsonError(res, 422, "Not a phoneplus backup");

  web::json::getString(req.body, req.contentLength, "settings.toml", scratch_,  sizeof(scratch_));
  web::json::getString(req.body, req.contentLength, "contacts.toml", scratch2_, sizeof(scratch2_));

  const size_t slen = strlen(scratch_);
  const size_t clen = strlen(scratch2_);
  if (slen > kMaxConfigFile || clen > kMaxConfigFile) return jsonError(res, 413, "File too large");

  std::string why, warnings, w2;
  if (slen > 0 && !checkConfigText(kSettingsPath, scratch_,  slen, why, warnings))
    return jsonError(res, 422, why.c_str());
  if (clen > 0 && !checkConfigText(kContactsPath, scratch2_, clen, why, w2))
    return jsonError(res, 422, why.c_str());
  if (slen > 0 && !commit(kSettingsPath, scratch_,  slen, res, warnings)) return;
  if (clen > 0) commit(kContactsPath, scratch2_, clen, res, warnings);
  else ok(res, warnings.c_str());
}

// ---------------------------------------------------------------------------
// Language routes
// ---------------------------------------------------------------------------

void WebApi::languages(http::Response& res) {
  const auto langs = translator_.languages();
  size_t n = 0;
  auto puts = [&](const char* s) { while (*s && n + 1 < sizeof(resp_)) resp_[n++] = *s++; };

  puts("[");
  bool first = true;
  for (const auto& lang : langs) {
    if (!first) puts(",");
    first = false;
    puts("{\"code\":");
    appendJsonStr(resp_, sizeof(resp_), n, lang.code);
    puts(",\"name\":");
    appendJsonStr(resp_, sizeof(resp_), n, lang.name);
    puts(",\"native\":");
    appendJsonStr(resp_, sizeof(resp_), n, lang.native);
    puts("}");
  }
  puts("]");
  if (n + 1 >= sizeof(resp_)) return jsonError(res, 500, "Too many languages");
  resp_[n] = '\0';
  jsonResp(res, 200, resp_);
}

void WebApi::langCatalog(const char* code, http::Response& res) {
  if (*code == '\0') return jsonError(res, 400, "Language code missing");

  // Collect all web.* keys from the requested language catalog.
  const auto langs = translator_.languages();
  bool found = false;
  for (const auto& lang : langs) {
    if (lang.code == code) { found = true; break; }
  }
  if (!found) return jsonError(res, 404, "Language not found");

  // Ask the translator for each key by prefix — we emit only what the page
  // uses. For now, emit an empty object; the web.* keys are populated in PR 4.
  jsonResp(res, 200, "{}");
}
