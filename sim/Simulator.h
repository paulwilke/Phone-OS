#pragma once

// The simulator: the Runtime (src/boot/Runtime.h) on the fakes of testing/fakes/, made interactive.
// The fakes are the ones the tests use, driven by the console commands below instead of by a test;
// nothing here reimplements a capability. The console's own words (help, prompts, errors) are
// English and are not translated: it is a developer's tool, and the device's words come from the
// catalogs.
//
// A Simulator owns the card (a directory: by default a fresh copy of the data directory under
// out/, so nothing in the repository is ever written to; with `inPlace` the data directory itself,
// which the owner asks for by name) and a Rig, the device built on it. A restart the device asks for builds a new Rig
// on the same card, as a real restart would: the clock starts again, the card keeps what was written.
//
// Time is the fake clock's. advance() moves it in steps of one second and runs the Runtime at each,
// so a scenario costs no real time; the real-time loop of main.cpp calls advance() with the time that
// has passed times the lapse factor.

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <config/ConfigLoader.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <poll.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <FrameBuffer.h>
#include <GoldenCompare.h>
#include <DirectoryStore.h>
#include <SimPanel.h>
#include <SocketTcp.h>
#include <boot/Runtime.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakeMotion.h>
#include <fakes/FakePower.h>
#include <fakes/FakeRadioLink.h>
#include <fakes/FakeWifi.h>
#include <Shows.h>
#include <i18n/Pseudo.h>

namespace sim {

namespace detail {

inline bool readFile(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::stringstream text;
  text << in.rdbuf();
  out = text.str();
  return true;
}

// Days from 1970-01-01 to a civil date (Howard Hinnant's algorithm), so a local date and time can be
// turned into an epoch without the C library's time zone.
inline int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

inline std::vector<std::string> words(const std::string& line) {
  std::vector<std::string> out;
  std::istringstream in(line);
  for (std::string w; in >> w;) out.push_back(w);
  return out;
}

// Everything after the first `skip` words of the line, as typed (single spaces kept).
inline std::string rest(const std::string& line, size_t skip) {
  size_t at = 0;
  for (size_t i = 0; i < skip; ++i) {
    at = line.find_first_not_of(" \t", at);
    if (at == std::string::npos) return "";
    at = line.find_first_of(" \t", at);
    if (at == std::string::npos) return "";
  }
  at = line.find_first_not_of(" \t", at);
  return at == std::string::npos ? "" : line.substr(at);
}

inline std::string base64(const std::string& in) {
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

inline bool number(const std::string& text, double& out) {
  char* end = nullptr;
  out = std::strtod(text.c_str(), &end);
  return !text.empty() && *end == '\0';
}

}  // namespace detail

// What the window needs from the shipped web page; the real page is served in the simulator's web
// part, here it only has to exist for the web service.
class FileAssets : public WebAssets {
public:
  explicit FileAssets(const std::string& path) {
    if (!detail::readFile(path, page_)) page_ = "<html><body>no web page at " + path + "</body></html>";
  }
  const char* indexHtml(size_t& length) override {
    length = page_.size();
    return page_.c_str();
  }

private:
  std::string page_;
};

struct Options {
  std::string dataDir = "sim/data";
  std::string langDir = "lang";
  // Words only host tools show (the demo page): merged into the shipped languages here, never on the device.
  std::string hostLangDir = "tools/lang";
  std::string webPage = "web/index.html";
  std::string goldenDir = "sim/golden";
  // Where the card is copied to. Only a directory this program made (it holds a marker file) or an
  // empty or missing one is ever emptied.
  std::string workDir = "out/sim-card";
  // Use the data directory itself as the card: what the web page saves is saved into it.
  bool inPlace = false;
  // The port of the parents' web page on 127.0.0.1; 0 lets the system choose (for tests).
  uint16_t webPort = 8080;
  uint32_t lapse = 1;
};

class Simulator {
public:
  // The epoch the device starts at: a Tuesday afternoon, 14:32 in Germany.
  static constexpr uint32_t kStartEpoch = 1789475520;
  static constexpr uint32_t kTickMs = 1000;
  static constexpr const char* kOperator = "Example Mobile";

  Simulator(const Options& options, std::ostream& out)
      : options_(options), out_(out), assets_(options.webPage), card_(prepareCard(options, out, error_)) {
    if (!error_.empty()) return;
    loadShippedCatalogs(options.langDir);
    boot();
  }

  // False when the card could not be made; error() says why and nothing else works then.
  bool ok() const { return error_.empty(); }
  const std::string& error() const { return error_; }

  // ---- running ---------------------------------------------------------------------------------
  // Runs one console line. False for an error, which has been explained on the output.
  bool execute(const std::string& line);
  // Runs the lines of a file; stops at the first error and says where.
  bool runScript(const std::string& path);
  // Lets `ms` milliseconds of simulated time pass, one second at a time.
  void advance(uint64_t ms);
  // The device reacts to what the console changed, without time passing.
  void settle() { tick(); }

  bool quitRequested() const { return quit_; }
  uint32_t lapse() const { return lapse_; }

  // ---- the device's side: what a window and the keyboard do -------------------------------------
  void press(ui::Key key) {
    rig_->runtime->input(ui::Input::pressed(key), nowMs());
    settle();
  }
  void typeCharacter(uint32_t codePoint) {
    rig_->runtime->input(ui::Input::text(codePoint), nowMs());
    settle();
  }
  void tap(int x, int y) {
    rig_->runtime->input(ui::Input::touch(x, y), nowMs());
    settle();
  }

  // ---- looking ---------------------------------------------------------------------------------
  boot::Runtime& runtime() { return *rig_->runtime; }
  SimPanel& panel() { return rig_->panel; }
  FakeClock& clock() { return rig_->clock; }
  FakeModem& modem() { return rig_->modem; }
  FakeHaptic& haptic() { return rig_->haptic; }
  FakeEventLog& log() { return rig_->log; }
  DirectoryStore& card() { return card_; }
  // The port the web page is on, 0 while Wi-Fi is off.
  uint16_t webPort() const { return rig_->tcp.boundPort(); }
  i18n::Translator& translator() { return translator_; }
  uint32_t nowMs() { return static_cast<uint32_t>(rig_->clock.uptimeMillis()); }
  // How many times the device has been started on this card.
  int boots() const { return boots_; }

private:
  struct Rig {
    FakeClock clock;
    FakePower power{&clock};
    FakeModem modem{clock};
    FakeGnss gnss{power, clock};
    FakeHaptic haptic{clock};
    FakeEventLog log{&clock};
    FakeWifi wifi;
    SocketTcpServer tcp;
    FakeRadioLink radio;
    FakeMotion motion;
    SimPanel panel;
    std::unique_ptr<boot::Runtime> runtime;
    size_t hapticShown = 0;
    size_t logShown = 0;
    explicit Rig(uint16_t webPort)
        : tcp(webPort == 0 ? SocketTcpServer::Listen::Any : SocketTcpServer::Listen::Fixed, webPort), panel([this] { return static_cast<uint32_t>(clock.uptimeMillis()); }) {}
  };

  static std::filesystem::path prepareCard(const Options& options, std::ostream& out, std::string& error);
  void loadShippedCatalogs(const std::string& dir);
  void boot();
  void tick();
  bool command(const std::vector<std::string>& w, const std::string& line);
  void printHelp();
  void printState();
  bool screenshotTo(const std::string& path);
  bool setDate(const std::string& date, const std::string& time);
  bool pressKey(const std::string& name);
  // The picture as the panel shows it after the device has caught up with what the console changed.
  const FrameBuffer& pictureNow() {
    settle();
    rig_->runtime->render();
    return rig_->panel.shown();
  }
  // Runs a line with its output kept apart (see `fails`); true when it worked.
  bool executeQuietly(const std::string& line, std::ostream& into);
  bool screenSays(const std::string& words);
  // One request to the parents' page through the real socket, as a browser would send it; the
  // simulated time runs on, a few milliseconds at a time, until the answer is there. `password` null:
  // no Authorization header.
  void webRequest(const std::string& method, const std::string& path, const std::string& body, const std::string* password);
  bool webCommand(const std::vector<std::string>& w, const std::string& line);
  bool see(const std::vector<std::string>& w, bool expected);
  bool check(const std::vector<std::string>& w, const std::string& line);
  void setSignal(int bars) {
    bars_ = bars;
    rig_->modem.setNetwork(operator_, bars_);
  }

  Options options_;
  std::ostream& out_;
  FileAssets assets_;
  std::string error_;
  DirectoryStore card_;
  i18n::Translator translator_;
  std::unique_ptr<Rig> rig_;
  std::string operator_ = kOperator;
  int bars_ = 4;
  uint32_t lapse_ = 1;
  bool quit_ = false;
  int boots_ = 0;
  int scriptDepth_ = 0;
  std::string webError_;
  std::string shotDir_;
  int webStatus_ = 0;  // of the last `web` request: 0 when nothing answered
  std::string webBody_;
  uint16_t webShown_ = 0;
};

// ---- the card and the catalogs ---------------------------------------------------------------------

inline std::filesystem::path Simulator::prepareCard(const Options& options, std::ostream& out, std::string& error) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path data = options.dataDir;
  if (options.inPlace) {
    if (!fs::is_directory(data, ec)) error = "no data directory " + options.dataDir + " to use in place";
    return data;
  }
  // A work directory is emptied only when this program made it, so a wrong --work cannot cost files.
  const fs::path work = options.workDir;
  const fs::path marker = work / ".phoneplus-sim";
  const fs::path card = work / "card";
  if (fs::exists(work, ec) && !fs::is_empty(work, ec) && !fs::exists(marker, ec)) {
    error = "refusing to empty " + options.workDir + ": it is not a directory of the simulator (no .phoneplus-sim in it)";
    return card;
  }
  const fs::path inside = fs::weakly_canonical(data, ec);
  const fs::path outside = fs::weakly_canonical(work, ec);
  // Neither may be in the other: copying a directory into itself never ends, and emptying one that
  // holds the data would empty the data.
  auto within = [&](const fs::path& inner, const fs::path& outer) {
    const std::string relative = fs::relative(inner, outer, ec).generic_string();
    return !relative.empty() && relative.rfind("..", 0) != 0;
  };
  if (inside == outside || within(inside, outside) || within(outside, inside)) {
    error = "the work directory " + options.workDir + " is the data directory, holds it or lies in it";
    return card;
  }
  fs::remove_all(card, ec);
  fs::create_directories(card, ec);
  { std::ofstream(marker) << "made by the phoneplus simulator; its card/ is emptied at every start\n"; }
  if (!fs::is_directory(data, ec)) {
    out << "warning: no data directory " << options.dataDir << ": the card is empty\n";
    return card;
  }
  fs::copy(data, card, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec) error = "cannot copy " + options.dataDir + " to " + options.workDir + ": " + ec.message();
  return card;
}

// The shipped catalogs are read from lang/, as the device would have them embedded; the
// pseudo-language is added so that a layout can be looked at in the longest words there are.
inline void Simulator::loadShippedCatalogs(const std::string& dir) {
  namespace fs = std::filesystem;
  i18n::Catalog english;
  if (fs::is_directory(dir)) {
    for (const auto& entry : fs::directory_iterator(dir)) {
      if (entry.path().extension() != ".toml") continue;
      std::string text;
      if (!detail::readFile(entry.path().string(), text)) continue;
      i18n::Catalog catalog;
      if (!catalog.parse(text.data(), text.size())) {
        out_ << "warning: cannot load catalog " << entry.path().string() << "\n";
        continue;
      }
      translator_.add(catalog);
      if (entry.path().stem() == i18n::Translator::kReference) english = catalog;
    }
  }
  // The host-only sets (tools/lang/<set>.<code>.toml): their words are added to the language of the same
  // code, as a user catalog with the code of a shipped language would be.
  if (fs::is_directory(options_.hostLangDir)) {
    std::vector<fs::path> sets;
    for (const auto& entry : fs::directory_iterator(options_.hostLangDir))
      if (entry.path().extension() == ".toml") sets.push_back(entry.path());
    std::sort(sets.begin(), sets.end());
    for (const fs::path& path : sets) {
      std::string text;
      i18n::Catalog catalog;
      if (!detail::readFile(path.string(), text) || !catalog.parse(text.data(), text.size())) {
        out_ << "warning: cannot load host catalog " << path.string() << "\n";
        continue;
      }
      translator_.add(catalog);
      if (catalog.code() == i18n::Translator::kReference) english.merge(catalog);
    }
  }
  if (english.size() == 0) out_ << "warning: no English catalog in " << dir << "\n";
  else translator_.add(i18n::pseudoCatalog(english));
}

// ---- starting the device ---------------------------------------------------------------------------

inline void Simulator::boot() {
  rig_ = std::make_unique<Rig>(options_.webPort);
  Rig& r = *rig_;
  ++boots_;
  r.clock.advanceSeconds(100);
  r.clock.setEpochUtc(kStartEpoch);
  r.power.setEnabled(PowerDomain::Cellular, true);
  r.power.setBattery(80);
  r.modem.begin();
  r.modem.setNetwork(operator_, bars_);
  r.gnss.fixAfter(2, 48.13745, 11.57550, 1.5);
  const uint32_t now = nowMs();
  r.runtime = std::make_unique<boot::Runtime>(
      boot::Capabilities{r.clock, r.power, r.modem, r.gnss, r.haptic, r.log, card_, r.wifi, r.tcp, assets_, r.panel,
                         [] { return 12345678u; }, "sim", nullptr, &r.motion},
      translator_, now);
  tick();
}

// One pass of the device, and the duties of a host that only hardware can do (ui/ docs: runtime.md).
inline void Simulator::tick() {
  Rig& r = *rig_;
  r.runtime->step(nowMs());
  if (r.runtime->core().takeRadioStartRequest()) {
    r.power.setEnabled(PowerDomain::Radio, true);
    r.radio.setUp(true);
    r.runtime->core().setRadio(&r.radio);   // the radio comes later, once the host has started it
    out_ << "radio: started\n";
  }
  for (; r.hapticShown < r.haptic.played().size(); ++r.hapticShown) {
    static const char* const names[] = {"click", "ring", "message", "sent", "failed"};
    out_ << "haptic: " << names[static_cast<int>(r.haptic.played()[r.hapticShown])] << "\n";
  }
  if (r.tcp.boundPort() != webShown_) {
    webShown_ = r.tcp.boundPort();
    if (webShown_) out_ << "web: http://127.0.0.1:" << webShown_ << "/\n";
  }
  if (r.tcp.lastError() != webError_) {
    webError_ = r.tcp.lastError();
    if (!webError_.empty()) out_ << "web: cannot listen on 127.0.0.1: " << webError_ << "\n";
  }
  if (r.runtime->core().powerOffRequested()) {
    out_ << "the device switched itself off\n";
    quit_ = true;
  } else if (r.runtime->restartRequested()) {
    out_ << "the device restarts\n";
    boot();
  }
}

inline void Simulator::advance(uint64_t ms) {
  while (ms > 0 && !quit_) {
    const uint64_t step = std::min<uint64_t>(ms, kTickMs);
    rig_->clock.advance(step);
    ms -= step;
    tick();
  }
}

// ---- the console ------------------------------------------------------------------------------------

inline bool Simulator::runScript(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    out_ << "error: cannot read script " << path << "\n";
    return false;
  }
  if (++scriptDepth_ > 8) {
    out_ << "error: scripts run scripts too deeply at " << path << "\n";
    --scriptDepth_;
    return false;
  }
  bool ok = true;
  std::string line;
  for (int n = 1; ok && !quit_ && std::getline(in, line); ++n) {
    if (!execute(line)) {
      out_ << "error: " << path << ":" << n << ": " << line << "\n";
      ok = false;
    }
  }
  --scriptDepth_;
  return ok;
}

inline bool Simulator::execute(const std::string& rawLine) {
  std::string line = rawLine;
  const std::vector<std::string> first = detail::words(line);
  if (first.empty() || first[0][0] == '#') return true;
  // A comment runs to the end of the line, except in what is said or typed, where "#" is a character
  // (a trigger word of the remote query starts with one).
  static const char* const verbatim[] = {"sms", "type", "echo", "radio"};
  if (std::find(std::begin(verbatim), std::end(verbatim), first[0]) == std::end(verbatim)) {
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);
  }
  return command(detail::words(line), line);
}

inline bool Simulator::setDate(const std::string& date, const std::string& time) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  char c1 = 0, c2 = 0, c3 = 0;
  std::istringstream a(date), b(time);
  if (!(a >> y >> c1 >> mo >> c2 >> d) || c1 != '-' || c2 != '-' || !(b >> h >> c3 >> mi) || c3 != ':') return false;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
  const int64_t local = (detail::daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 24 + h) * 3600 + mi * 60;
  // The offset depends on the date (summer time), so it is asked of the zone for the instant itself,
  // not taken from the clock, which still has the offset of the day it was on. Two rounds settle it
  // except in the hour that does not exist at the change of the clocks.
  int offset = rig_->clock.tzOffsetMinutes();
  const tz::Zone* zone = rig_->runtime->core().profile().timeZone;
  int64_t utc = local - static_cast<int64_t>(offset) * 60;
  for (int round = 0; zone && round < 2 && utc > 0; ++round) {
    offset = tz::offsetMinutes(*zone, static_cast<uint32_t>(utc));
    utc = local - static_cast<int64_t>(offset) * 60;
  }
  if (utc < 0 || !rig_->clock.setEpochUtc(static_cast<uint32_t>(utc))) return false;
  return true;
}

inline bool Simulator::pressKey(const std::string& name) {
  static const struct { const char* name; ui::Key key; } keys[] = {
      {"up", ui::Key::Up},     {"down", ui::Key::Down},   {"select", ui::Key::Select}, {"enter", ui::Key::Select},
      {"back", ui::Key::Back}, {"home", ui::Key::Home},   {"inbox", ui::Key::Inbox},   {"compose", ui::Key::Compose}};
  for (const auto& k : keys)
    if (name == k.name) {
      press(k.key);
      return true;
    }
  out_ << "error: no key " << name << " (up down select back home inbox compose)\n";
  return false;
}

inline bool Simulator::screenshotTo(const std::string& path) {
  settle();
  rig_->runtime->render();
  if (!rig_->panel.writePng(path)) {
    out_ << "error: cannot write " << path << "\n";
    return false;
  }
  return true;
}

inline void Simulator::printState() {
  const AppCore::State s = rig_->runtime->core().state();
  static const char* const screens[] = {"home", "lock", "clock", "phone", "number", "call", "messages", "compose",
                                        "recipient", "sos", "settings", "wifi", "net", "status", "language",
                                        "gnss", "radio", "motion", "selftest"};
  static_assert(sizeof(screens) / sizeof(screens[0]) == ui::screen::kCount, "a screen name is missing");
  out_ << "screen:    " << screens[rig_->runtime->stack().visibleId()] << (rig_->runtime->stack().locked() ? " (locked)" : "") << "\n";
  out_ << "language:  " << translator_.language() << "\n";
  if (s.wall.known) {
    out_ << "time:      " << s.wall.year << "-" << std::setfill('0') << std::setw(2) << s.wall.month << "-" << std::setw(2)
         << s.wall.day << " " << std::setw(2) << s.wall.minuteOfDay / 60 << ":" << std::setw(2) << s.wall.minuteOfDay % 60
         << std::setfill(' ') << (s.schoolTime ? "  school time" : "") << "\n";
  } else {
    out_ << "time:      unknown\n";
  }
  out_ << "network:   " << (s.registered ? operator_ : std::string("none")) << ", " << s.signalBars << " bars\n";
  out_ << "battery:   " << s.batteryPercent << "%\n";
  out_ << "unread:    " << s.unread << ", queued " << s.queued << "\n";
  out_ << "panel:     " << rig_->panel.stats().full << " full, " << rig_->panel.stats().partial << " partial refreshes, "
       << rig_->panel.stats().skipped << " frames skipped\n";
  out_ << "lapse:     x" << lapse_ << "\n";
}

inline void Simulator::printHelp() {
  out_ << "Console commands (English only; the device's words come from the catalogs).\n"
          "  The other side\n"
          "    sms <number> <text>          a text message arrives\n"
          "    call <number>                a call comes in\n"
          "    remote answer|busy|noanswer|hangup|fail <why>   what the other end of a call does\n"
          "    radio up|down                the radio chip comes up / goes away\n"
          "    radio peer <name>            a radio contact appears\n"
          "    radio msg <name> <text>      a radio message arrives (the name is one word)\n"
          "    radio confirm yes|no         the radio confirms / does not confirm the oldest message\n"
          "  The world\n"
          "    battery <0-100>              the battery level\n"
          "    plug charging|full | unplug  the charger\n"
          "    signal <0-5>                 the signal bars\n"
          "    network <name>|none          the operator (none: no network)\n"
          "    simcard ready|missing|pin|pinwrong|puk\n"
          "    gps fix [lat lon hdop]       a fix after the receiver's start (default: 48.13745 11.57550 1.5)\n"
          "    gps none | gps bad           never a fix | a fix with poor accuracy\n"
          "    motion tilt <x> <y> <z>|shake|off|on\n"
          "    wifi ap|station|off          switch Wi-Fi: the access point is there at once, the station waits for\n"
          "    wifi connected|failed        the answer of the home network to the station's attempt\n"
          "  Time\n"
          "    date <YYYY-MM-DD> <HH:MM>    set the clock (local time of the device's zone)\n"
          "    advance <n>[ms|s|m|h]        let time pass (default seconds), the device running\n"
          "    lapse <factor>               real-time speed-up of the window and the stdin loop (1 = real time)\n"
          "  The device\n"
          "    key <up|down|select|back|home|inbox|compose>...   keys, one after the other\n"
          "    type <text>                  typed characters\n"
          "    tap <x> <y>                  a touch at a pixel (240 x 320)\n"
          "    language <code>              the interface language (en de fr qps-ploc, or a user catalog)\n"
          "    sos begin [number]|choose emergency|pickup|tap|leave   the SOS button's entry points (there is no SOS page yet)\n"
          "    cellular on|off | savemode on|off | probe              what the network page and its options do\n"
          "    radio start                  the child asks for the radio; the simulator starts it\n"
          "  Looking\n"
          "    state                        a summary of the device\n"
          "    log [n]                      the last n lines of the device's log (default 20)\n"
          "    screenshot <file.png>        the panel as a picture\n"
          "    shotdir <dir> | shot <name>  pictures into a directory: <dir>/<name>.png\n"
          "    ascii [file]                 the panel as # and . (or into a file)\n"
          "    golden <name>                compare the panel with " << options_.goldenDir << "/<name>.txt (UPDATE_GOLDEN=1 writes it)\n"
          "    web [as <password>|anonymous] get|post|put|delete <path> [body]   a request to the parents' page\n"
          "  Asserting (a failed one is an error, so a script fails)\n"
          "    see <catalog key> [name=value...]   (a value may have spaces) the screen says that text, in the language of the moment\n"
          "    see-not <catalog key> [name=value...]\n"
          "    say <text> | say-not <text>         the screen says this literal text (a name, an operator)\n"
          "    check <what> <value>         screen, locked, unread, school, call, sos, registered, cellular, savemode,\n"
          "                                 battery, sms-sent, last-sms-to, last-sms-has, language, boots, log-has, wifi,\n"
          "                                 wifi-seconds-left, web-status, web-has,\n"
          "                                 haptic-click|ring|message|sent|failed (how often played)\n"
          "    fails <command>              the command is refused (an error is what the scenario expects)\n"
          "  Control\n"
          "    run <script>                 run the commands of a file\n"
          "    echo <text>                  print the text (a marker in a script)\n"
          "    help | quit\n";
}

inline bool Simulator::command(const std::vector<std::string>& w, const std::string& line) {
  Rig& r = *rig_;
  const std::string& name = w[0];
  auto fail = [&](const std::string& why) {
    out_ << "error: " << why << " (try help)\n";
    return false;
  };
  auto need = [&](size_t n) { return w.size() >= n; };
  auto nums = [&](size_t from, size_t count, double* values) {
    if (w.size() != from + count) return false;
    for (size_t i = 0; i < count; ++i)
      if (!detail::number(w[from + i], values[i])) return false;
    return true;
  };

  if (name == "help") { printHelp(); return true; }
  if (name == "quit" || name == "exit") { quit_ = true; return true; }
  if (name == "echo") { out_ << detail::rest(line, 1) << "\n"; return true; }
  if (name == "state") { settle(); printState(); return true; }
  if (name == "run") {
    if (!need(2)) return fail("run needs a file");
    return runScript(w[1]);
  }

  // The other side.
  if (name == "sms") {
    const std::string text = detail::rest(line, 2);
    if (!need(3) || text.empty()) return fail("sms needs a number and a text");
    r.modem.injectSms(w[1], text, r.clock.epochUtc());
    settle();
    return true;
  }
  if (name == "call") {
    if (!need(2)) return fail("call needs a number");
    if (!r.modem.injectIncomingCall(w[1])) {
      out_ << "error: the modem cannot ring now (no network, no voice, or not ready)\n";
      return false;
    }
    settle();
    return true;
  }
  if (name == "remote") {
    if (!need(2)) return fail("remote needs what the other end does");
    if (w[1] == "answer") r.modem.remoteAnswer();
    else if (w[1] == "busy") r.modem.remoteBusy();
    else if (w[1] == "noanswer") r.modem.remoteNoAnswer();
    else if (w[1] == "hangup") r.modem.remoteHangup();
    else if (w[1] == "fail") r.modem.failDial(w.size() > 2 ? w[2] : "network");
    else return fail("remote: answer, busy, noanswer, hangup or fail");
    settle();
    return true;
  }
  if (name == "radio") {
    if (!need(2)) return fail("radio needs start, up, down, peer, msg or confirm");
    if (w[1] == "start") r.runtime->core().requestRadioStart();
    else if (w[1] == "up") r.radio.setUp(true);
    else if (w[1] == "down") r.radio.setUp(false);
    else if (w[1] == "peer" && need(3)) r.radio.addPeer(detail::rest(line, 2), true, r.clock.epochUtc());
    else if (w[1] == "msg" && need(4)) r.radio.deliver(w[2], detail::rest(line, 3), r.clock.epochUtc());
    else if (w[1] == "confirm" && need(3) && (w[2] == "yes" || w[2] == "no")) {
      if (!r.radio.confirm(w[2] == "yes")) out_ << "note: no radio message is waiting for a confirmation\n";
    } else return fail("radio: up, down, peer <name>, msg <name> <text> or confirm yes|no");
    settle();
    return true;
  }

  // The world.
  if (name == "battery") {
    double v[1];
    if (!nums(1, 1, v) || v[0] < 0 || v[0] > 100) return fail("battery needs 0 to 100");
    r.power.setBattery(static_cast<int>(v[0]));
    settle();
    return true;
  }
  if (name == "plug") {
    if (!need(2) || (w[1] != "charging" && w[1] != "full")) return fail("plug needs charging or full");
    r.power.plugIn(w[1] == "full" ? ChargeState::Full : ChargeState::Charging);
    settle();
    return true;
  }
  if (name == "unplug") { r.power.unplug(); settle(); return true; }
  if (name == "signal") {
    double v[1];
    if (!nums(1, 1, v) || v[0] < 0 || v[0] > 5) return fail("signal needs 0 to 5");
    setSignal(static_cast<int>(v[0]));
    settle();
    return true;
  }
  if (name == "network") {
    if (!need(2)) return fail("network needs a name or none");
    operator_ = w[1] == "none" ? "" : detail::rest(line, 1);
    r.modem.setNetwork(operator_, operator_.empty() ? 0 : bars_);
    settle();
    return true;
  }
  if (name == "simcard") {
    static const struct { const char* name; SimStatus status; } states[] = {
        {"ready", SimStatus::Ready}, {"missing", SimStatus::Missing}, {"pin", SimStatus::PinRequired},
        {"pinwrong", SimStatus::PinWrong}, {"puk", SimStatus::PukRequired}};
    if (need(2))
      for (const auto& s : states)
        if (w[1] == s.name) {
          r.modem.setSimStatus(s.status);
          settle();
          return true;
        }
    return fail("simcard: ready, missing, pin, pinwrong or puk");
  }
  if (name == "gps") {
    if (!need(2)) return fail("gps needs fix, none or bad");
    if (w[1] == "fix") {
      double v[3] = {48.13745, 11.57550, 1.5};
      if (w.size() != 2 && !nums(2, 3, v)) return fail("gps fix needs latitude, longitude and hdop, or nothing");
      r.gnss.fixAfter(2, v[0], v[1], static_cast<float>(v[2]));
    } else if (w[1] == "none") {
      r.gnss.never();
    } else if (w[1] == "bad") {
      r.gnss.fixAfter(2, 48.13745, 11.57550, 40.0f);
    } else {
      return fail("gps: fix, none or bad");
    }
    settle();
    return true;
  }
  if (name == "motion") {
    if (!need(2)) return fail("motion needs tilt, shake, off or on");
    if (w[1] == "tilt") {
      double v[3];
      if (!nums(2, 3, v)) return fail("motion tilt needs x y z");
      r.motion.tilt(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
    } else if (w[1] == "shake") r.motion.shake();
    else if (w[1] == "off") r.motion.setAvailable(false);
    else if (w[1] == "on") r.motion.setAvailable(true);
    else return fail("motion: tilt, shake, off or on");
    settle();
    return true;
  }
  if (name == "wifi") {
    // What the device's Wi-Fi page does: the core's one Wi-Fi controller is switched, with the rules in
    // front of it (save mode, battery), and the parents' page is reachable while it is on.
    AppCore& core = r.runtime->core();
    if (need(2) && w[1] == "ap") core.setWifiMode(WifiMode::AccessPoint);
    else if (need(2) && w[1] == "station") core.setWifiMode(WifiMode::Station);
    else if (need(2) && w[1] == "off") core.setWifiMode(WifiMode::Off);
    else if (need(2) && w[1] == "connected") r.wifi.finishConnecting(true);
    else if (need(2) && w[1] == "failed") r.wifi.finishConnecting(false);
    else return fail("wifi: ap, station, off, connected or failed");
    settle();
    return true;
  }

  // Time.
  if (name == "date") {
    if (!need(3) || !setDate(w[1], w[2])) return fail("date needs YYYY-MM-DD HH:MM, after 2020");
    settle();
    return true;
  }
  if (name == "advance") {
    if (w.size() != 2) return fail("advance needs one duration, like 90s or 2h");
    char* end = nullptr;
    const double n = std::strtod(w[1].c_str(), &end);
    const std::string unit = end ? end : "";
    double factor = 1000;
    if (unit == "ms") factor = 1;
    else if (unit == "m") factor = 60000;
    else if (unit == "h") factor = 3600000;
    else if (!unit.empty() && unit != "s") return fail("advance: unit ms, s, m or h");
    if (end == w[1].c_str() || n < 0) return fail("advance needs a positive duration");
    advance(static_cast<uint64_t>(n * factor));
    return true;
  }
  if (name == "lapse") {
    double v[1];
    if (!nums(1, 1, v) || v[0] < 1 || v[0] > 100000) return fail("lapse needs a factor from 1 to 100000");
    lapse_ = static_cast<uint32_t>(v[0]);
    return true;
  }

  // The device.
  if (name == "key") {
    if (!need(2)) return fail("key needs at least one key");
    for (size_t i = 1; i < w.size(); ++i)
      if (!pressKey(w[i])) return false;
    return true;
  }
  if (name == "type") {
    const std::string text = detail::rest(line, 1);
    if (text.empty()) return fail("type needs a text");
    // The text is UTF-8; each code point is one keystroke.
    for (size_t i = 0; i < text.size();) {
      const unsigned char c = static_cast<unsigned char>(text[i]);
      const size_t len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
      uint32_t cp = len == 1 ? c : c & (0xFF >> (len + 1));
      for (size_t k = 1; k < len && i + k < text.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
      typeCharacter(cp);
      i += len;
    }
    return true;
  }
  if (name == "tap") {
    double v[2];
    if (!nums(1, 2, v)) return fail("tap needs x and y");
    tap(static_cast<int>(v[0]), static_cast<int>(v[1]));
    return true;
  }
  if (name == "language") {
    if (!need(2)) return fail("language needs a code");
    if (!r.runtime->setLanguage(w[1])) {
      out_ << "error: no catalog has the code " << w[1] << "\n";
      return false;
    }
    settle();
    return true;
  }

  if (name == "sos") {
    AppCore& core = r.runtime->core();
    bool done = false;
    if (need(2) && w[1] == "begin") done = need(3) ? core.beginSos(w[2]) : core.beginSos();
    else if (need(3) && w[1] == "choose" && w[2] == "emergency") done = core.chooseSos(SosPing::Kind::Emergency);
    else if (need(3) && w[1] == "choose" && w[2] == "pickup") done = core.chooseSos(SosPing::Kind::Pickup);
    else if (need(2) && w[1] == "tap") done = core.tapSos();
    else if (need(2) && w[1] == "leave") { core.leaveSos(); done = true; }
    else return fail("sos: begin [number], choose emergency|pickup, tap or leave");
    settle();
    if (!done && w[1] != "tap") out_ << "note: the core did not take that (no one to reach, or not the moment)\n";
    return true;
  }
  if (name == "cellular" || name == "savemode") {
    if (!need(2) || (w[1] != "on" && w[1] != "off")) return fail(name + " needs on or off");
    const bool on = w[1] == "on";
    if (name == "cellular") r.runtime->core().setCellularEnabled(on);
    else r.runtime->core().setSaveMode(on);
    settle();
    return true;
  }
  if (name == "probe") { r.runtime->core().probeCellular(); settle(); return true; }

  if (name == "web") return webCommand(w, line);

  // Asserting.
  if (name == "see" || name == "see-not") return see(w, name == "see");
  if (name == "say" || name == "say-not") {
    const std::string words = detail::rest(line, 1);
    if (words.empty()) return fail(name + " needs a text");
    const bool says = screenSays(words);
    if (says != (name == "say")) {
      out_ << "error: the screen " << (says ? "says \"" : "does not say \"") << words << "\"\n";
      return false;
    }
    return true;
  }
  if (name == "check") return check(w, line);
  if (name == "fails") {
    // For a scenario that says "this is refused": succeeds only when the command after it fails.
    const std::string inner = detail::rest(line, 1);
    if (inner.empty()) return fail("fails needs a command");
    std::ostringstream quiet;
    const bool refused = !executeQuietly(inner, quiet);
    if (!refused) out_ << "error: \"" << inner << "\" worked, and was expected to fail\n";
    return refused;
  }

  // Looking.
  if (name == "log") {
    size_t n = 20;
    if (need(2)) n = static_cast<size_t>(std::strtoul(w[1].c_str(), nullptr, 10));
    const std::vector<std::string> lines = r.log.lines();
    for (size_t i = lines.size() > n ? lines.size() - n : 0; i < lines.size(); ++i) out_ << lines[i] << "\n";
    return true;
  }
  if (name == "shotdir") {
    if (!need(2)) return fail("shotdir needs a directory");
    std::error_code ec;
    std::filesystem::create_directories(w[1], ec);
    if (ec) {
      out_ << "error: cannot make " << w[1] << ": " << ec.message() << "\n";
      return false;
    }
    shotDir_ = w[1];
    return true;
  }
  if (name == "shot") {
    if (!need(2) || shotDir_.empty()) return fail("shot needs a name, and shotdir before it");
    return screenshotTo(shotDir_ + "/" + w[1] + ".png");
  }
  if (name == "screenshot") {
    if (!need(2)) return fail("screenshot needs a file name");
    return screenshotTo(w[1]);
  }
  if (name == "ascii") {
    settle();
    r.runtime->render();
    const std::string art = r.panel.shown().toAscii();
    if (!need(2)) {
      out_ << art;
      return true;
    }
    std::ofstream file(w[1], std::ios::binary);
    file << art;
    if (!file) {
      out_ << "error: cannot write " << w[1] << "\n";
      return false;
    }
    return true;
  }
  if (name == "golden") {
    if (!need(2)) return fail("golden needs a name");
    settle();
    r.runtime->render();
    const golden::Result result = golden::compare(r.panel.shown(), w[1], {0, 0, ui::kScreenWidth, ui::kScreenHeight},
                                                  options_.goldenDir, golden::updateRequested());
    if (!result.ok) out_ << "error: " << result.message << "\n";
    return result.ok;
  }
  return fail("no command " + name);
}

// ---- the parents' page ----------------------------------------------------------------------------------------------

inline void Simulator::webRequest(const std::string& method, const std::string& path, const std::string& body,
                                  const std::string* password) {
  webStatus_ = 0;
  webBody_.clear();
  const uint16_t port = webPort();
  if (port == 0) return;  // Wi-Fi is off: nobody is listening
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (fd < 0 || ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    if (fd >= 0) ::close(fd);
    return;
  }
  std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n";
  if (password) request += "Authorization: Basic " + detail::base64("parent:" + *password) + "\r\n";
  if (method != "GET") request += "X-Phoneplus: 1\r\n";
  request += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
  ::send(fd, request.data(), request.size(), 0);
  // The server answers when the simulator runs: a few milliseconds of its time at a time.
  std::string raw;
  char buffer[4096];
  for (int round = 0; round < 600; ++round) {
    advance(20);
    pollfd p{fd, POLLIN, 0};
    bool closed = false;
    while (::poll(&p, 1, 0) > 0) {
      const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
      if (n <= 0) {
        closed = true;
        break;
      }
      raw.append(buffer, static_cast<size_t>(n));
    }
    if (closed) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ::close(fd);
  const size_t split = raw.find("\r\n\r\n");
  if (split == std::string::npos) return;
  webStatus_ = std::atoi(raw.c_str() + raw.find(' ') + 1);
  webBody_ = raw.substr(split + 4);
}

inline bool Simulator::webCommand(const std::vector<std::string>& w, const std::string& line) {
  size_t at = 1;
  std::string password;
  bool useAuth = true;
  if (w.size() > 3 && w[1] == "anonymous") {
    useAuth = false;
    at = 2;
  } else if (w.size() > 4 && w[1] == "as") {
    password = w[2];
    at = 3;
  } else {
    // The parents' password of the card, as the page's visitor would type it.
    config::ConfigLoader::Result loaded = config::ConfigLoader(card_).load();
    password = loaded.webPassword;
  }
  if (w.size() < at + 2) {
    out_ << "error: web needs a method and a path (try help)\n";
    return false;
  }
  std::string method = w[at];
  for (char& c : method) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  if (method != "GET" && method != "POST" && method != "PUT" && method != "DELETE") {
    out_ << "error: web: get, post, put or delete\n";
    return false;
  }
  webRequest(method, w[at + 1], detail::rest(line, at + 2), useAuth ? &password : nullptr);
  out_ << "web: " << (webStatus_ ? std::to_string(webStatus_) : std::string("no answer")) << " " << method << " " << w[at + 1] << "\n";
  return true;
}

// ---- asserting -------------------------------------------------------------------------------------------------

// Whether the words are on the panel in any of the four fonts, on white or on black. The match is
// exact over the whole word and its margin (testing/Shows.h), so a word that is cut off, drawn in
// the wrong font or covered by another is not found.
inline bool Simulator::screenSays(const std::string& words) {
  const FrameBuffer& picture = pictureNow();
  // Looking is a scan of the whole picture, so each (text, font) is looked up once.
  std::map<std::pair<std::string, int>, bool> known;
  auto found = [&](const std::string& text, ui::FontSize size) {
    const auto key = std::make_pair(text, static_cast<int>(size));
    const auto hit = known.find(key);
    if (hit != known.end()) return hit->second;
    bool result = false;
    for (bool onBlack : {false, true}) result = result || shows::text(picture, text, size, ui::Rect{}, onBlack);
    return known[key] = result;
  };
  for (ui::FontSize size : {ui::FontSize::Small, ui::FontSize::Text, ui::FontSize::Tile, ui::FontSize::Large}) {
    if (found(words, size)) return true;
    // A long text is wrapped over lines: it is said when the lines of one wrapping are all there.
    // Which width the screen wrapped at is not known, so the widths are tried, each different
    // wrapping once.
    std::set<std::vector<std::string>> tried;
    for (int width = 60; width <= ui::kScreenWidth; width += 4) {
      std::vector<std::string> lines;
      for (std::string_view line : ui::text::wrap(ui::font(size), words, width)) lines.emplace_back(line);
      if (lines.size() < 2 || !tried.insert(lines).second) continue;
      bool all = true;
      for (size_t i = 0; all && i < lines.size(); ++i) all = found(lines[i], size);
      if (all) return true;
    }
  }
  return false;
}

// `see key name=value ...`: the catalog's text for the key in the language of the moment, with the
// arguments filled in, so the assertion holds in every language.
inline bool Simulator::see(const std::vector<std::string>& w, bool expected) {
  if (w.size() < 2) {
    out_ << "error: " << w[0] << " needs a catalog key (try help)\n";
    return false;
  }
  std::vector<std::string> values;
  std::vector<std::string> names;
  // A word with "=" starts an argument; the words after it belong to its value ("name=Parent One").
  for (size_t i = 2; i < w.size(); ++i) {
    const size_t eq = w[i].find('=');
    if (eq == std::string::npos || eq == 0) {
      if (values.empty()) {
        out_ << "error: " << w[i] << " is not name=value\n";
        return false;
      }
      values.back() += " " + w[i];
      continue;
    }
    names.push_back(w[i].substr(0, eq));
    values.push_back(w[i].substr(eq + 1));
  }
  std::vector<i18n::Arg> args;
  for (size_t i = 0; i < names.size(); ++i) {
    char* end = nullptr;
    const long long number = std::strtoll(values[i].c_str(), &end, 10);
    if (!values[i].empty() && *end == '\0') args.emplace_back(names[i].c_str(), static_cast<int64_t>(number));
    else args.emplace_back(names[i].c_str(), values[i].c_str());
  }
  if (translator_.text(w[1].c_str()) == nullptr) {
    out_ << "error: no catalog key " << w[1] << "\n";
    return false;
  }
  const std::string words = translator_.get(w[1].c_str(), args.data(), args.size());
  const bool says = screenSays(words);
  if (says != expected) {
    out_ << "error: the screen " << (says ? "says" : "does not say") << " \"" << words << "\" (" << w[1] << ", "
         << translator_.language() << ")\n";
    return false;
  }
  return true;
}

inline bool Simulator::executeQuietly(const std::string& line, std::ostream& into) {
  std::streambuf* const saved = out_.rdbuf(into.rdbuf());
  const bool ok = execute(line);
  out_.rdbuf(saved);
  return ok;
}

inline bool Simulator::check(const std::vector<std::string>& w, const std::string& line) {
  if (w.size() < 3) {
    out_ << "error: check needs what and the value (try help)\n";
    return false;
  }
  settle();
  const AppCore::State s = rig_->runtime->core().state();
  const std::string& what = w[1];
  const std::string value = detail::rest(line, 2);
  std::string actual;
  static const char* const screens[] = {"home", "lock", "clock", "phone", "number", "call", "messages", "compose",
                                        "recipient", "sos", "settings", "wifi", "net", "status", "language",
                                        "gnss", "radio", "motion", "selftest"};
  auto yesNo = [](bool b) { return std::string(b ? "yes" : "no"); };
  const std::vector<FakeModem::SentSms>& sent = rig_->modem.sent();
  if (what == "screen") actual = screens[rig_->runtime->stack().visibleId()];
  else if (what == "locked") actual = yesNo(rig_->runtime->stack().locked());
  else if (what == "unread") actual = std::to_string(s.unread);
  else if (what == "school") actual = yesNo(s.schoolTime);
  else if (what == "registered") actual = yesNo(s.registered);
  else if (what == "cellular") actual = s.cellularEnabled ? "on" : "off";
  else if (what == "savemode") actual = s.saveMode ? "on" : "off";
  else if (what == "battery") actual = std::to_string(s.batteryPercent);
  else if (what == "language") actual = translator_.language();
  else if (what == "boots") actual = std::to_string(boots_);
  else if (what == "sms-sent") actual = std::to_string(sent.size());
  else if (what == "call") {
    switch (s.callState) {
      case ModemState::Dialing: actual = "dialing"; break;
      case ModemState::RingingIn: actual = "ringing"; break;
      case ModemState::InCall: actual = "incall"; break;
      default: actual = "idle"; break;
    }
  } else if (what == "sos") {
    switch (s.sosPhase) {
      case SosFlow::Phase::Choosing: actual = "choosing"; break;
      case SosFlow::Phase::QuietConfirm: actual = "quiet-confirm"; break;
      default: actual = s.emergencyRunning ? "running" : (s.sosSecondsLeft > 0 ? "countdown" : "idle"); break;
    }
  } else if (what.rfind("haptic-", 0) == 0) {
    // How often a pattern was played since the device started: haptic-click, -ring, -message, -sent, -failed.
    static const char* const patterns[] = {"click", "ring", "message", "sent", "failed"};
    size_t index = 5;
    for (size_t i = 0; i < 5; ++i)
      if (what == std::string("haptic-") + patterns[i]) index = i;
    if (index == 5) {
      out_ << "error: no vibration pattern called " << what.substr(7) << "\n";
      return false;
    }
    size_t n = 0;
    for (HapticPattern played : rig_->haptic.played()) n += static_cast<size_t>(played) == index;
    actual = std::to_string(n);
  } else if (what == "wifi") {
    // The state the device's own screens read: the one Wi-Fi controller's phase.
    switch (s.wifiPhase) {
      case WifiPhase::Connecting: actual = "connecting"; break;
      case WifiPhase::Connected: actual = "station"; break;
      case WifiPhase::AccessPoint: actual = "access-point"; break;
      case WifiPhase::Failed: actual = "failed"; break;
      default: actual = "off"; break;
    }
  } else if (what == "wifi-seconds-left") {
    const WifiController* wifi = rig_->runtime->core().wifi();
    actual = std::to_string(wifi ? wifi->secondsLeft(nowMs() / 1000) : 0);
  } else if (what == "web-status") {
    actual = std::to_string(webStatus_);
  } else if (what == "web-has") {
    const bool has = webBody_.find(value) != std::string::npos;
    if (!has) out_ << "error: the last answer of the page is " << (webBody_.size() > 200 ? webBody_.substr(0, 200) + "..." : webBody_)
                   << ", not one with \"" << value << "\"\n";
    return has;
  } else if (what == "log-has") {
    for (const std::string& entry : rig_->log.lines())
      if (entry.find(value) != std::string::npos) return true;
    out_ << "error: no line of the log has \"" << value << "\"\n";
    return false;
  } else if (what == "last-sms-to") {
    actual = sent.empty() ? "" : sent.back().phone;
  } else if (what == "last-sms-has") {
    // The text is searched, not compared: the value is what must be in it.
    const bool has = !sent.empty() && sent.back().body.find(value) != std::string::npos;
    if (!has) out_ << "error: the last text sent is " << (sent.empty() ? "none" : "\"" + sent.back().body + "\"") << ", not one with \"" << value << "\"\n";
    return has;
  } else {
    out_ << "error: nothing to check called " << what << " (try help)\n";
    return false;
  }
  if (actual != value) {
    out_ << "error: " << what << " is " << (actual.empty() ? "empty" : actual) << ", expected " << value << "\n";
    return false;
  }
  return true;
}

// The card a script wants, from a line "# data: <directory>" among its first lines, so a scenario
// that needs a user catalog or school time always on says so itself. Empty when it names none.
inline std::string scriptData(const std::string& path) {
  std::ifstream in(path);
  std::string line;
  for (int n = 0; n < 8 && std::getline(in, line); ++n) {
    static const std::string key = "# data:";
    if (line.compare(0, key.size(), key) == 0) {
      const std::vector<std::string> w = detail::words(line.substr(key.size()));
      return w.empty() ? "" : w[0];
    }
  }
  return "";
}

}  // namespace sim
