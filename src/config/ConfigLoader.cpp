#include "ConfigLoader.h"

#include <cstring>
#include <string>

#include "../app/ContactBook.h"
#include "../app/LogLevel.h"
#include "../app/QuietHours.h"
#include "../app/TimeZone.h"
#include "../app/Toml.h"
#include "../app/WifiSettings.h"

#include "schema_generated.h"

namespace config {

namespace {

// ---------------------------------------------------------------------------
// File reading
// ---------------------------------------------------------------------------

// Returns true when the file was read into `out`; adds a diagnostic on any problem.
// `out` is cleared on failure.
bool readConfigFile(ConfigStore& store, const char* path, const char* fileId,
                    std::string& out, std::vector<Diagnostic>& d) {
  int64_t size = store.fileSize(path);
  if (size < 0) {
    d.push_back({Diagnostic::Code::MissingFile, {fileId}});
    out.clear();
    return false;
  }
  if ((size_t)size > ConfigLoader::kMaxFileBytes) {
    d.push_back({Diagnostic::Code::OversizedFile, {fileId}});
    out.clear();
    return false;
  }
  if (!store.read(path, out, ConfigLoader::kMaxFileBytes)) {
    d.push_back({Diagnostic::Code::MissingFile, {fileId}});
    out.clear();
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Type-checking helpers — add a diagnostic and return false on mismatch
// ---------------------------------------------------------------------------

bool checkStr(const toml::Value& v, const char* file, const char* key,
              int minLen, int maxLen, std::vector<Diagnostic>& d, std::string& out) {
  if (v.type() != toml::Value::Type::String) {
    d.push_back({Diagnostic::Code::WrongType, {file, key, "string"}});
    return false;
  }
  int len = (int)v.str().size();
  if (minLen > 0 && len < minLen) {
    d.push_back({Diagnostic::Code::OutOfRange, {file, key}});
    return false;
  }
  if (maxLen > 0 && len > maxLen) {
    d.push_back({Diagnostic::Code::OutOfRange, {file, key}});
    return false;
  }
  out = v.str();
  return true;
}

bool checkBool(const toml::Value& v, const char* file, const char* key,
               std::vector<Diagnostic>& d, bool& out) {
  if (v.type() != toml::Value::Type::Boolean) {
    d.push_back({Diagnostic::Code::WrongType, {file, key, "bool"}});
    return false;
  }
  out = v.boolean();
  return true;
}

bool checkInt(const toml::Value& v, const char* file, const char* key,
              int64_t minVal, int64_t maxVal, std::vector<Diagnostic>& d, int64_t& out) {
  if (v.type() != toml::Value::Type::Integer) {
    d.push_back({Diagnostic::Code::WrongType, {file, key, "integer"}});
    return false;
  }
  if (v.integer() < minVal || v.integer() > maxVal) {
    d.push_back({Diagnostic::Code::OutOfRange, {file, key}});
    return false;
  }
  out = v.integer();
  return true;
}

bool checkEnum(const toml::Value& v, const char* file, const char* key,
               const char* allowed, std::vector<Diagnostic>& d, std::string& out) {
  if (v.type() != toml::Value::Type::String) {
    d.push_back({Diagnostic::Code::WrongType, {file, key, "enum"}});
    return false;
  }
  const std::string& s = v.str();
  const char* p = allowed;
  while (*p) {
    const char* bar = strchr(p, '|');
    size_t len = bar ? (size_t)(bar - p) : strlen(p);
    if (s.size() == len && memcmp(s.data(), p, len) == 0) {
      out = s;
      return true;
    }
    p = bar ? bar + 1 : p + len;
  }
  d.push_back({Diagnostic::Code::BadEnum, {file, key, s}});
  return false;
}

bool checkStrList(const toml::Value& v, const char* file, const char* key,
                  std::vector<Diagnostic>& d, std::vector<std::string>& out) {
  if (v.type() != toml::Value::Type::Array) {
    d.push_back({Diagnostic::Code::WrongType, {file, key, "string-list"}});
    return false;
  }
  out.clear();
  for (size_t i = 0; i < v.size(); ++i) {
    if (v.at(i).type() != toml::Value::Type::String) {
      d.push_back({Diagnostic::Code::WrongType, {file, key, "string-list"}});
      return false;
    }
    out.push_back(v.at(i).str());
  }
  return true;
}

// ---------------------------------------------------------------------------
// Unknown-key detection
// ---------------------------------------------------------------------------

// Reports keys in `table` that are not registered as leaves in (file, section).
// Sub-tables and table-lists are skipped: they are sections checked separately.
void checkLeafKeys(std::vector<Diagnostic>& d, const toml::Value& table,
                   const char* file, const char* section) {
  for (size_t i = 0; i < table.size(); ++i) {
    const toml::Value& v = table.at(i);
    if (v.type() == toml::Value::Type::Table ||
        v.type() == toml::Value::Type::TableList) continue;
    const std::string& k = table.keyAt(i);
    bool known = false;
    for (const KeyDef& kd : kKeys) {
      if (strcmp(kd.file, file) == 0 && strcmp(kd.section, section) == 0 && k == kd.name) {
        known = true;
        break;
      }
    }
    if (!known) {
      std::string path = *section ? std::string(section) + "." + k : k;
      d.push_back({Diagnostic::Code::UnknownKey, {file, path}});
    }
  }
}

// Reports top-level keys in `root` that are not in the given null-terminated list.
void checkTopLevel(std::vector<Diagnostic>& d, const toml::Value& root,
                   const char* file, const char* const* known) {
  for (size_t i = 0; i < root.size(); ++i) {
    const std::string& k = root.keyAt(i);
    bool found = false;
    for (const char* const* p = known; *p; ++p) {
      if (k == *p) { found = true; break; }
    }
    if (!found) d.push_back({Diagnostic::Code::UnknownKey, {file, k}});
  }
}

// ---------------------------------------------------------------------------
// Section appliers — settings.toml
// ---------------------------------------------------------------------------

void applyChild(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("name")) checkStr(*v, "settings", "child.name", 0, 24, d, p.childName);
}

void applyDisplay(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("map_url")) checkStr(*v, "settings", "display.map_url", 0, 120, d, p.mapTemplate);
}

// An APN is a dotted name of letters, digits, hyphens and underscores. It ends up inside an AT
// command, so anything else (a quote, a space, a line break) is refused rather than escaped.
bool isApnCharacter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
}

void applyModem(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  const toml::Value* v = sec.find("apn");
  if (!v) return;
  std::string apn;
  if (!checkStr(*v, "settings", "modem.apn", 0, 100, d, apn)) return;
  for (char c : apn) {
    if (!isApnCharacter(c)) {
      d.push_back({Diagnostic::Code::OutOfRange, {"settings", "modem.apn"}});
      return;
    }
  }
  p.modemApn = apn;
}

void applyBattery(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("capacity_mah")) {
    int64_t n = 0;
    if (checkInt(*v, "settings", "battery.capacity_mah", 100, 10000, d, n)) p.batteryCapacityMah = (int)n;
  }
}

void applyInterface(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("language")) checkStr(*v, "settings", "interface.language", 0, 10, d, p.interfaceLanguage);
}

void applySms(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("language"))
    checkStr(*v, "settings", "sms.language", 0, 10, d, p.smsLanguage);
  if (const toml::Value* v = sec.find("extra_trigger_words"))
    checkStrList(*v, "settings", "sms.extra_trigger_words", d, p.extraTriggers);
}

void applyFeatures(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("free_text"))         checkBool(*v, "settings", "features.free_text", d, p.freeText);
  if (const toml::Value* v = sec.find("radio"))             checkBool(*v, "settings", "features.radio", d, p.radioAllowed);
  if (const toml::Value* v = sec.find("parent_tools"))      checkBool(*v, "settings", "features.parent_tools", d, p.parentTools);
  if (const toml::Value* v = sec.find("message_vibration")) checkBool(*v, "settings", "features.message_vibration", d, p.messageVibration);
  if (const toml::Value* v = sec.find("call_vibration"))    checkBool(*v, "settings", "features.call_vibration", d, p.callVibration);
  if (const toml::Value* v = sec.find("quiet_call_vibration"))
    checkBool(*v, "settings", "features.quiet_call_vibration", d, p.quietCallVibration);
  if (const toml::Value* v = sec.find("modem_sleep"))       checkBool(*v, "settings", "features.modem_sleep", d, p.modemSleep);
}

void applyHome(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  const struct { const char* key; const char* path; bool HomeTiles::*field; } tiles[] = {
      {"network", "home.network", &HomeTiles::network}, {"gps", "home.gps", &HomeTiles::gps},
      {"clock", "home.clock", &HomeTiles::clock},       {"settings", "home.settings", &HomeTiles::settings},
      {"wifi", "home.wifi", &HomeTiles::wifi},          {"motion", "home.motion", &HomeTiles::motion},
      {"tests", "home.tests", &HomeTiles::tests},       {"status", "home.status", &HomeTiles::status}};
  for (const auto& tile : tiles)
    if (const toml::Value* v = sec.find(tile.key)) checkBool(*v, "settings", tile.path, d, p.home.*tile.field);
}

void applyRemoteQueries(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("notice")) {
    std::string val;
    if (checkEnum(*v, "settings", "remote_queries.notice", "silent|history|notify", d, val)) {
      if (val == "silent")  p.commandNotice = CommandNotice::Silent;
      else if (val == "history") p.commandNotice = CommandNotice::History;
      else                       p.commandNotice = CommandNotice::Notify;
    }
  }
}

void applyTime(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("timezone")) {
    std::string code;
    if (checkStr(*v, "settings", "time.timezone", 0, 40, d, code) && !code.empty()) {
      const tz::Zone* z = tz::find(code.c_str());
      if (z) p.timeZone = z;
      else   d.push_back({Diagnostic::Code::BadEnum, {"settings", "time.timezone", code}});
    }
  }
}

void applyLog(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("level")) {
    std::string val;
    if (checkEnum(*v, "settings", "log.level", "all|events|errors", d, val))
      p.logLevel = loglevel::parse(val.c_str());
  }
}

void applyWifiSettings(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("auto_off_minutes")) {
    int64_t n = 0;
    if (checkInt(*v, "settings", "wifi.auto_off_minutes", 1, 120, d, n))
      p.wifi.setAutoOffMinutes((uint32_t)n);
  }
}

void applyRadio(const toml::Value& sec, Profile& p, std::vector<Diagnostic>& d) {
  if (const toml::Value* v = sec.find("name")) checkStr(*v, "settings", "radio.name", 0, 32, d, p.radioName);
  if (const toml::Value* v = sec.find("peer")) checkStr(*v, "settings", "radio.peer", 0, 32, d, p.radioPeer);
}

void applyQuietHoursEntry(const toml::Value& entry, QuietHours& qh, std::vector<Diagnostic>& d) {
  const toml::Value* days = entry.find("days");
  const toml::Value* from = entry.find("from");
  const toml::Value* to   = entry.find("to");
  if (!days || !from || !to) {
    d.push_back({Diagnostic::Code::BadQuietHours, {"missing-field"}});
    return;
  }
  if (days->type() != toml::Value::Type::String ||
      from->type() != toml::Value::Type::String ||
      to->type()   != toml::Value::Type::String) {
    d.push_back({Diagnostic::Code::BadQuietHours, {"wrong-type"}});
    return;
  }
  // A wrong type leaves the window switched on, as a window written before the key existed is:
  // a typo must not quietly end school time.
  bool enabled = true;
  if (const toml::Value* e = entry.find("enabled")) checkBool(*e, "settings", "quiet_hours.enabled", d, enabled);
  auto result = qh.add(days->str().c_str(), from->str().c_str(), to->str().c_str(), enabled);
  if (result != QuietHours::AddResult::Added) {
    const char* reason = "invalid";
    switch (result) {
      case QuietHours::AddResult::BadDays:      reason = "bad-days";      break;
      case QuietHours::AddResult::BadTime:      reason = "bad-time";      break;
      case QuietHours::AddResult::EmptyWindow:  reason = "empty-window";  break;
      case QuietHours::AddResult::DayFull:      reason = "day-full";      break;
      default: break;
    }
    d.push_back({Diagnostic::Code::BadQuietHours, {reason}});
  }
}

// The value the schema gives the entry a list section uses when the file has none, or nullptr.
const char* defaultEntryValue(const char* file, const char* section, const char* key) {
  for (size_t i = 0; i < kDefaultEntryCount; ++i) {
    const DefaultEntryValue& v = kDefaultEntries[i];
    if (strcmp(v.file, file) == 0 && strcmp(v.section, section) == 0 && strcmp(v.key, key) == 0) return v.value;
  }
  return nullptr;
}

// A card without any window gets the schema's one (switched off): the parents' page and the example
// file show the same window, and no school time runs until a parent switches it on. Only the file's
// own list decides: a card whose windows are all refused has said what it wants and gets no default.
void addDefaultQuietHours(QuietHours& qh) {
  const char* days = defaultEntryValue("settings", "quiet_hours", "days");
  const char* from = defaultEntryValue("settings", "quiet_hours", "from");
  const char* to = defaultEntryValue("settings", "quiet_hours", "to");
  const char* enabled = defaultEntryValue("settings", "quiet_hours", "enabled");
  if (!days || !from || !to) return;
  qh.add(days, from, to, !enabled || strcmp(enabled, "false") != 0);
}

void applyTileEntry(const toml::Value& entry, std::vector<std::string>& callTargets,
                    std::vector<Diagnostic>& d) {
  const toml::Value* actionVal = entry.find("action");
  if (!actionVal || actionVal->type() != toml::Value::Type::String) return;
  const std::string& action = actionVal->str();
  if (action != "call") {
    d.push_back({Diagnostic::Code::NonCallTile, {action}});
    return;
  }
  const toml::Value* targetVal = entry.find("target");
  if (!targetVal || targetVal->type() != toml::Value::Type::String || targetVal->str().empty()) {
    d.push_back({Diagnostic::Code::TileNoTarget, {}});
    return;
  }
  callTargets.push_back(targetVal->str());
}

// ---------------------------------------------------------------------------
// Top-level settings loader
// ---------------------------------------------------------------------------

static const char* kSettingsTopLevel[] = {
  "child", "display", "modem", "battery", "interface", "sms", "features", "remote_queries",
  "time", "log", "wifi", "radio", "mesh", "home", "quiet_hours", "tiles", nullptr
};

void loadSettings(const toml::Value& root, Profile& p, std::vector<Diagnostic>& d) {
  checkTopLevel(d, root, "settings", kSettingsTopLevel);

  if (const toml::Value* s = root.find("child")) {
    checkLeafKeys(d, *s, "settings", "child");
    applyChild(*s, p, d);
  }
  if (const toml::Value* s = root.find("display")) {
    checkLeafKeys(d, *s, "settings", "display");
    applyDisplay(*s, p, d);
  }
  if (const toml::Value* s = root.find("modem")) {
    checkLeafKeys(d, *s, "settings", "modem");
    applyModem(*s, p, d);
  }
  if (const toml::Value* s = root.find("battery")) {
    checkLeafKeys(d, *s, "settings", "battery");
    applyBattery(*s, p, d);
  }
  if (const toml::Value* s = root.find("interface")) {
    checkLeafKeys(d, *s, "settings", "interface");
    applyInterface(*s, p, d);
  }
  if (const toml::Value* s = root.find("sms")) {
    checkLeafKeys(d, *s, "settings", "sms");
    applySms(*s, p, d);
  }
  if (const toml::Value* s = root.find("features")) {
    checkLeafKeys(d, *s, "settings", "features");
    applyFeatures(*s, p, d);
  }
  if (const toml::Value* s = root.find("home")) {
    checkLeafKeys(d, *s, "settings", "home");
    applyHome(*s, p, d);
  }
  if (const toml::Value* s = root.find("remote_queries")) {
    checkLeafKeys(d, *s, "settings", "remote_queries");
    applyRemoteQueries(*s, p, d);
  }
  if (const toml::Value* s = root.find("time")) {
    checkLeafKeys(d, *s, "settings", "time");
    applyTime(*s, p, d);
  }
  if (const toml::Value* s = root.find("log")) {
    checkLeafKeys(d, *s, "settings", "log");
    applyLog(*s, p, d);
  }
  if (const toml::Value* s = root.find("wifi")) {
    checkLeafKeys(d, *s, "settings", "wifi");
    applyWifiSettings(*s, p, d);
  }

  // [radio] with [mesh] as alias for cards from the previous firmware
  const toml::Value* radioSec = root.find("radio");
  if (!radioSec) {
    radioSec = root.find("mesh");
    if (radioSec) d.push_back({Diagnostic::Code::MeshAlias, {}});
  }
  if (radioSec) {
    checkLeafKeys(d, *radioSec, "settings", "radio");
    applyRadio(*radioSec, p, d);
  }

  const toml::Value* windows = root.find("quiet_hours");
  if (windows && windows->type() == toml::Value::Type::TableList && windows->size() > 0) {
    for (size_t i = 0; i < windows->size(); ++i) {
      checkLeafKeys(d, windows->at(i), "settings", "quiet_hours");
      applyQuietHoursEntry(windows->at(i), p.quietHours, d);
    }
  } else {
    addDefaultQuietHours(p.quietHours);  // no key, `quiet_hours = []` and anything that is no list of tables
  }
  if (const toml::Value* list = root.find("tiles")) {
    if (list->type() == toml::Value::Type::TableList) {
      for (size_t i = 0; i < list->size(); ++i) {
        checkLeafKeys(d, list->at(i), "settings", "tiles");
        applyTileEntry(list->at(i), p.callTargets, d);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// contacts.toml loader
// ---------------------------------------------------------------------------

static const char* kContactsTopLevel[] = {"contact", nullptr};

void loadContacts(const toml::Value& root, phone::ContactBook& contacts,
                  std::vector<Diagnostic>& d) {
  checkTopLevel(d, root, "contacts", kContactsTopLevel);

  const toml::Value* list = root.find("contact");
  if (!list || list->type() != toml::Value::Type::TableList) return;

  std::string whitelistText;
  for (size_t i = 0; i < list->size(); ++i) {
    const toml::Value& entry = list->at(i);
    checkLeafKeys(d, entry, "contacts", "contact");

    const toml::Value* numVal = entry.find("number");
    if (!numVal || numVal->type() != toml::Value::Type::String || numVal->str().empty()) {
      d.push_back({Diagnostic::Code::ContactNoNumber, {std::to_string(i + 1)}});
      continue;
    }
    const std::string& number = numVal->str();

    bool admin = false;
    if (const toml::Value* roleVal = entry.find("role")) {
      if (roleVal->type() == toml::Value::Type::String) {
        const std::string& role = roleVal->str();
        if (role == "parent") admin = true;
        else if (role != "family") {
          d.push_back({Diagnostic::Code::UnknownRole, {role}});
          // treated as family
        }
      } else {
        d.push_back({Diagnostic::Code::WrongType, {"contacts", "contact.role", "string"}});
      }
    }

    whitelistText += number;
    if (admin) whitelistText += " admin";
    if (const toml::Value* nameVal = entry.find("name")) {
      if (nameVal->type() == toml::Value::Type::String && !nameVal->str().empty()) {
        whitelistText += " ";
        whitelistText += nameVal->str();
      }
    }
    whitelistText += "\n";
  }

  contacts.load(whitelistText.c_str(), whitelistText.size());
  // ContactBook::problems() are silently handled — the whitelist format handles its own
  // issues (duplicate, too many) and ContactBook::load() is already tested separately.
}

// ---------------------------------------------------------------------------
// secrets.toml loader
// ---------------------------------------------------------------------------

static const char* kSecretsTopLevel[] = {"sim_pin", "web_password", "wifi", nullptr};

void loadSecrets(const toml::Value& root, WifiSettings& wifi, std::string& simPin,
                 std::string& webPassword, std::vector<Diagnostic>& d) {
  checkTopLevel(d, root, "secrets", kSecretsTopLevel);
  checkLeafKeys(d, root, "secrets", "");

  if (const toml::Value* v = root.find("sim_pin"))
    checkStr(*v, "secrets", "sim_pin", 0, 8, d, simPin);

  if (const toml::Value* v = root.find("web_password")) {
    std::string pwd;
    if (checkStr(*v, "secrets", "web_password", 0, 0, d, pwd)) {
      if (!pwd.empty() && (int)pwd.size() < 4)
        d.push_back({Diagnostic::Code::OutOfRange, {"secrets", "web_password"}});
      else
        webPassword = pwd;
    }
  }

  const toml::Value* wifiVal = root.find("wifi");
  if (!wifiVal) return;
  if (wifiVal->type() != toml::Value::Type::Table) return;

  // Check for unexpected sub-keys in the wifi table
  for (size_t i = 0; i < wifiVal->size(); ++i) {
    const std::string& k = wifiVal->keyAt(i);
    if (k != "access_point" && k != "network")
      d.push_back({Diagnostic::Code::UnknownKey, {"secrets", "wifi." + k}});
  }

  if (const toml::Value* ap = wifiVal->find("access_point")) {
    if (ap->type() == toml::Value::Type::Table) {
      checkLeafKeys(d, *ap, "secrets", "wifi.access_point");
      std::string ssid, pwd;
      if (const toml::Value* v = ap->find("ssid")) {
        if (checkStr(*v, "secrets", "wifi.access_point.ssid", 0, 32, d, ssid))
          wifi.setApSsid(ssid);
      }
      if (const toml::Value* v = ap->find("password")) {
        if (checkStr(*v, "secrets", "wifi.access_point.password", 0, 63, d, pwd))
          wifi.setApPassword(pwd);
      }
    }
  }

  if (const toml::Value* netList = wifiVal->find("network")) {
    if (netList->type() == toml::Value::Type::TableList) {
      for (size_t i = 0; i < netList->size(); ++i) {
        const toml::Value& entry = netList->at(i);
        checkLeafKeys(d, entry, "secrets", "wifi.network");
        std::string ssid, pwd;
        if (const toml::Value* v = entry.find("ssid")) checkStr(*v, "secrets", "wifi.network.ssid", 0, 32, d, ssid);
        if (const toml::Value* v = entry.find("password")) checkStr(*v, "secrets", "wifi.network.password", 0, 63, d, pwd);
        if (!ssid.empty()) {
          auto res = wifi.addNetwork(ssid, pwd);
          if (res == WifiSettings::AddResult::BadName)
            d.push_back({Diagnostic::Code::OutOfRange, {"secrets", "wifi.network.ssid"}});
          else if (res == WifiSettings::AddResult::BadPassword)
            d.push_back({Diagnostic::Code::OutOfRange, {"secrets", "wifi.network.password"}});
          else if (res == WifiSettings::AddResult::TooMany)
            d.push_back({Diagnostic::Code::OutOfRange, {"secrets", "wifi.network"}});
        }
      }
    }
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// ConfigLoader
// ---------------------------------------------------------------------------

ConfigLoader::ConfigLoader(ConfigStore& store) : store_(store) {}

ConfigLoader::Result ConfigLoader::load() const {
  Result r;
  std::string text;
  bool settingsLoaded = false;

  if (readConfigFile(store_, "/config/settings.toml", "settings", text, r.diagnostics)) {
    toml::Value root;
    toml::Error err = toml::parse(text, root);
    if (err.failed()) {
      r.diagnostics.push_back({Diagnostic::Code::SyntaxError,
        {"settings", std::to_string(err.line), toml::codeName(err.code)}});
    } else {
      loadSettings(root, r.profile, r.diagnostics);
      settingsLoaded = true;
    }
  }
  // A missing or unreadable settings file is a card without windows as well.
  if (!settingsLoaded) addDefaultQuietHours(r.profile.quietHours);

  if (readConfigFile(store_, "/config/contacts.toml", "contacts", text, r.diagnostics)) {
    toml::Value root;
    toml::Error err = toml::parse(text, root);
    if (err.failed()) {
      r.diagnostics.push_back({Diagnostic::Code::SyntaxError,
        {"contacts", std::to_string(err.line), toml::codeName(err.code)}});
    } else {
      loadContacts(root, r.profile.contacts, r.diagnostics);
    }
  }

  if (readConfigFile(store_, "/config/secrets.toml", "secrets", text, r.diagnostics)) {
    toml::Value root;
    toml::Error err = toml::parse(text, root);
    if (err.failed()) {
      r.diagnostics.push_back({Diagnostic::Code::SyntaxError,
        {"secrets", std::to_string(err.line), toml::codeName(err.code)}});
    } else {
      loadSecrets(root, r.profile.wifi, r.simPin, r.webPassword, r.diagnostics);
    }
  }

  return r;
}

bool ConfigLoader::checkLanguages(const Profile& profile, const i18n::Translator& translator,
                                  std::vector<Diagnostic>& diagnostics) {
  auto installed = [&](const std::string& code) {
    for (const auto& language : translator.languages())
      if (language.code == code) return true;
    return false;
  };
  const bool interfaceOk = installed(profile.interfaceLanguage);
  if (!interfaceOk)
    diagnostics.push_back({Diagnostic::Code::BadCatalogCode, {"settings", "interface.language", profile.interfaceLanguage}});
  if (!installed(profile.smsLanguage))
    diagnostics.push_back({Diagnostic::Code::BadCatalogCode, {"settings", "sms.language", profile.smsLanguage}});
  return interfaceOk;
}

namespace {

// Reads every *.toml of `dir` on the card and hands it to `add`; a file that is too big or that `add`
// refuses is skipped with a diagnostic naming it. Never throws, never stops at the first bad file.
template <class Add>
void readCatalogs(ConfigStore& store, const char* dir, size_t maxBytes, std::vector<Diagnostic>& diagnostics, Add add) {
  std::vector<FileEntry> entries;
  if (!store.list(dir, entries)) return;

  for (const FileEntry& e : entries) {
    if (e.isDir) continue;
    // Only .toml files
    const std::string& name = e.name;
    if (name.size() < 6) continue;
    if (name.substr(name.size() - 5) != ".toml" &&
        name.substr(name.size() - 5) != ".TOML") continue;

    const std::string path = std::string(dir) + "/" + name;
    std::string text;
    int64_t sz = store.fileSize(path);
    if (sz < 0 || (size_t)sz > maxBytes) {
      diagnostics.push_back({Diagnostic::Code::OversizedFile, {path}});
      continue;
    }
    if (!store.read(path, text, maxBytes)) continue;

    std::vector<std::string> errors;
    if (!add(text, &errors)) {
      std::string firstError = errors.empty() ? "invalid catalog" : errors[0];
      diagnostics.push_back({Diagnostic::Code::UserCatalogError, {path, firstError}});
    }
  }
}

}  // namespace

void ConfigLoader::loadUserCatalogs(ConfigStore& store, i18n::Translator& translator,
                                     std::vector<Diagnostic>& diagnostics) {
  readCatalogs(store, "/lang", kMaxCatalogBytes, diagnostics, [&](const std::string& text, std::vector<std::string>* errors) {
    return translator.addUser(text.c_str(), text.size(), errors);
  });
}

void ConfigLoader::loadUserWebCatalogs(ConfigStore& store, i18n::Translator& translator,
                                        std::vector<Diagnostic>& diagnostics) {
  readCatalogs(store, "/lang/web", kMaxCatalogBytes, diagnostics, [&](const std::string& text, std::vector<std::string>* errors) {
    return translator.addUserWeb(text.c_str(), text.size(), errors);
  });
}

}  // namespace config
