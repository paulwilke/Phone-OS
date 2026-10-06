#include "ConfigWriter.h"

#include <cstdio>
#include <string>

#include "../app/ContactBook.h"
#include "../app/PhoneNumber.h"

namespace config {

namespace {

// Appends a TOML string value, quoting special characters.
void appendStr(std::string& out, const std::string& s) {
  out += '"';
  for (char c : s) {
    if (c == '"')       out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\t') out += "\\t";
    else if (c == '\r') out += "\\r";
    else                out += c;
  }
  out += '"';
}

void appendBool(std::string& out, bool v) { out += v ? "true" : "false"; }

void appendInt(std::string& out, int64_t v) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)v);
  out += buf;
}

void appendStrList(std::string& out, const std::vector<std::string>& list) {
  out += '[';
  for (size_t i = 0; i < list.size(); ++i) {
    if (i) out += ", ";
    appendStr(out, list[i]);
  }
  out += ']';
}

const char* commandNoticeName(CommandNotice n) {
  switch (n) {
    case CommandNotice::Silent:  return "silent";
    case CommandNotice::History: return "history";
    case CommandNotice::Notify:  return "notify";
  }
  return "history";
}

const char* logLevelName(loglevel::Level l) {
  switch (l) {
    case loglevel::Level::All:    return "all";
    case loglevel::Level::Events: return "events";
    case loglevel::Level::Errors: return "errors";
  }
  return "all";
}

}  // namespace

ConfigWriter::ConfigWriter(ConfigStore& store) : store_(store) {}

bool ConfigWriter::writeSettings(const Profile& p) const {
  std::string out;
  out.reserve(1024);

  out += "[child]\n";
  out += "name = "; appendStr(out, p.childName); out += "\n\n";

  out += "[display]\n";
  out += "map_url = "; appendStr(out, p.mapTemplate); out += "\n\n";

  out += "[modem]\n";
  out += "apn = "; appendStr(out, p.modemApn); out += "\n\n";

  out += "[interface]\n";
  out += "language = "; appendStr(out, p.interfaceLanguage); out += "\n\n";

  out += "[sms]\n";
  out += "language = "; appendStr(out, p.smsLanguage); out += "\n";
  out += "extra_trigger_words = "; appendStrList(out, p.extraTriggers); out += "\n\n";

  out += "[features]\n";
  out += "free_text = "; appendBool(out, p.freeText); out += "\n";
  out += "radio = "; appendBool(out, p.radioAllowed); out += "\n";
  out += "parent_tools = "; appendBool(out, p.parentTools); out += "\n";
  out += "message_vibration = "; appendBool(out, p.messageVibration); out += "\n";
  out += "call_vibration = "; appendBool(out, p.callVibration); out += "\n";
  out += "quiet_call_vibration = "; appendBool(out, p.quietCallVibration); out += "\n";
  out += "modem_sleep = "; appendBool(out, p.modemSleep); out += "\n\n";

  out += "[remote_queries]\n";
  out += "notice = "; appendStr(out, commandNoticeName(p.commandNotice)); out += "\n\n";

  out += "[time]\n";
  out += "timezone = "; appendStr(out, p.timeZone ? p.timeZone->name : ""); out += "\n\n";

  out += "[log]\n";
  out += "level = "; appendStr(out, logLevelName(p.logLevel)); out += "\n\n";

  out += "[wifi]\n";
  out += "auto_off_minutes = "; appendInt(out, (int64_t)p.wifi.autoOffMinutes); out += "\n\n";

  out += "[radio]\n";
  out += "name = "; appendStr(out, p.radioName); out += "\n";
  out += "peer = "; appendStr(out, p.radioPeer); out += "\n\n";

  for (size_t i = 0; i < p.quietHours.count(); ++i) {
    const QuietHours::Window& w = p.quietHours.window(i);
    // Reconstruct days and times from the window data.
    // The QuietHours module stores pre-parsed windows; we write them back with
    // the canonical mon/tue/... names and HH:MM format.
    out += "[[quiet_hours]]\n";
    // Build the days string from the bitmask (bit 0 = Monday)
    static const char* kDayNames[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    std::string days;
    for (int d = 0; d < 7; ++d) {
      if (w.days & (1u << d)) {
        if (!days.empty()) days += ',';
        days += kDayNames[d];
      }
    }
    out += "days = "; appendStr(out, days); out += "\n";
    char time[8];
    snprintf(time, sizeof(time), "%02d:%02d", w.from / 60, w.from % 60);
    out += "from = "; appendStr(out, time); out += "\n";
    snprintf(time, sizeof(time), "%02d:%02d", w.to / 60, w.to % 60);
    out += "to = "; appendStr(out, time); out += "\n\n";
  }

  for (const std::string& target : p.callTargets) {
    out += "[[tiles]]\n";
    out += "action = \"call\"\n";
    out += "target = "; appendStr(out, target); out += "\n\n";
  }

  return store_.write("/config/settings.toml", out);
}

bool ConfigWriter::writeContacts(const Profile& p) const {
  std::string out;
  out.reserve(256);

  for (size_t i = 0; i < p.contacts.count(); ++i) {
    const phone::Contact& c = p.contacts.at(i);
    out += "[[contact]]\n";
    out += "number = "; appendStr(out, c.phone); out += "\n";
    if (c.name[0]) { out += "name = "; appendStr(out, c.name); out += "\n"; }
    out += "role = "; appendStr(out, c.admin ? "parent" : "family"); out += "\n\n";
  }

  return store_.write("/config/contacts.toml", out);
}

bool ConfigWriter::writeSecrets(const Profile& p, const std::string& simPin,
                                 const std::string& webPassword) const {
  std::string out;
  out.reserve(256);

  out += "sim_pin = "; appendStr(out, simPin); out += "\n";
  out += "web_password = "; appendStr(out, webPassword); out += "\n\n";

  out += "[wifi.access_point]\n";
  out += "ssid = "; appendStr(out, p.wifi.apSsid); out += "\n";
  out += "password = "; appendStr(out, p.wifi.apPassword); out += "\n\n";

  for (const WifiSettings::Network& net : p.wifi.networks) {
    out += "[[wifi.network]]\n";
    out += "ssid = "; appendStr(out, net.ssid); out += "\n";
    out += "password = "; appendStr(out, net.password); out += "\n\n";
  }

  return store_.write("/config/secrets.toml", out);
}

}  // namespace config
