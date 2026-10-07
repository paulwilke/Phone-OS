#include "Console.h"

#include <cstdlib>
#include <cstring>

namespace board {

namespace {

bool writable(const std::string& name) { return name == "settings.toml" || name == "contacts.toml" || name == "secrets.toml"; }
// The logs. Built from parts because the catalog check takes a quoted word with a dot for a catalog key.
bool isLog(const std::string& name) {
  const std::string ending = ".log";
  return name == std::string("system") + ending || name == std::string("system") + ".old" + ending || name == std::string("messages") + ending;
}
bool readable(const std::string& name) { return writable(name) || isLog(name); }

// Digits only, within [0, max]; false for anything else ("12abc", "-1", an empty word).
bool number(const std::string& word, uint32_t max, uint32_t& out) {
  if (word.empty() || word.size() > 9) return false;
  uint32_t v = 0;
  for (char c : word) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (uint32_t)(c - '0');
  }
  if (v > max) return false;
  out = v;
  return true;
}

// The next word and the rest after it (leading blanks removed).
std::string take(std::string& rest) {
  size_t i = 0;
  while (i < rest.size() && rest[i] == ' ') ++i;
  size_t j = i;
  while (j < rest.size() && rest[j] != ' ') ++j;
  const std::string word = rest.substr(i, j - i);
  while (j < rest.size() && rest[j] == ' ') ++j;
  rest = rest.substr(j);
  return word;
}

ConsoleCommand fail(const char* error) {
  ConsoleCommand c;
  c.kind = ConsoleCommand::Kind::Unknown;
  c.error = error;
  return c;
}

}  // namespace

uint16_t consoleChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; ++i) sum = (uint16_t)(sum + data[i]);
  return sum;
}

ConsoleCommand parseConsoleLine(const std::string& input) {
  std::string rest = input;
  std::string verb = take(rest);
  if (verb == "CONFIG") verb = take(rest);
  else if (verb != "BATTERY" && verb != "SMS") return ConsoleCommand();  // not for the console
  if (verb.empty()) return fail("unknown command");

  ConsoleCommand c;
  if (verb == "LIST") {
    c.kind = ConsoleCommand::Kind::List;
    return c;
  }
  if (verb == "RESTART") {
    c.kind = ConsoleCommand::Kind::Restart;
    return c;
  }
  if (verb == "BATTERY") {
    c.kind = ConsoleCommand::Kind::Battery;
    return c;
  }
  if (verb == "SMS") {
    c.phone = take(rest);
    if (c.phone.empty() || rest.empty() || c.phone.size() > 23) return fail("usage: SMS <number> <text>");
    if (rest.size() > 199) return fail("the text is too long");
    for (size_t i = 0; i < rest.size(); ++i) {
      if (rest[i] == '\\' && i + 1 < rest.size() && rest[i + 1] == 'n') {
        c.text += '\n';
        ++i;
      } else {
        c.text += rest[i];
      }
    }
    c.kind = ConsoleCommand::Kind::Sms;
    return c;
  }
  if (verb == "WEB") {
    uint32_t n = 0;
    if (!number(take(rest), kConsoleMaxRequest, n) || n == 0) return fail("usage: CONFIG WEB <bytes 1..9300>");
    c.kind = ConsoleCommand::Kind::Web;
    c.length = n;
    return c;
  }
  if (verb != "READ" && verb != "WRITE") return fail("unknown command");

  c.file = take(rest);
  if (verb == "READ" ? !readable(c.file) : !writable(c.file)) return fail("unknown file");
  c.path = (writable(c.file) ? "/config/" : "/") + c.file;
  if (verb == "READ") {
    c.kind = ConsoleCommand::Kind::Read;
    return c;
  }
  const std::string n = take(rest), sum = take(rest);
  if (n.empty() || sum.empty()) return fail("length or checksum missing");
  uint32_t length = 0, checksum = 0;
  if (!number(n, kConsoleMaxFile, length) || length == 0) return fail("bad length");
  if (!number(sum, 0xFFFF, checksum)) return fail("bad checksum");
  c.kind = ConsoleCommand::Kind::Write;
  c.length = length;
  c.sum = (uint16_t)checksum;
  return c;
}

void Console::say(const std::string& text) {
  // The port's transmit buffer is far smaller than a log answer, so a zero-length write only means "full, try again
  // in a moment". Without a computer nothing will ever drain it: drop at once, the firmware never waits for one
  // that is not there.
  size_t done = 0;
  uint32_t lastProgress = millis_.millis32();
  while (done < text.size() && port_.hostConnected()) {
    const size_t n = port_.write(reinterpret_cast<const uint8_t*>(text.data()) + done, text.size() - done);
    if (n > 0) {
      done += n;
      lastProgress = millis_.millis32();
      continue;
    }
    if (millis_.millis32() - lastProgress >= kConsoleWriteStallMs) return;  // the computer stopped reading
    delay_.delayMs(kConsoleWritePauseMs);
  }
}

void Console::poll() {
  for (int c = port_.read(); c >= 0; c = port_.read()) {
    if (collecting_) {
      bytes_.push_back((uint8_t)c);
      if (bytes_.size() >= raw_.length) finishRaw();
      continue;
    }
    if (c == '\r') continue;
    if (c == '\n') {
      if (!discarding_) line(line_);
      line_.clear();
      discarding_ = false;
      continue;
    }
    if (discarding_) continue;
    if (line_.size() >= kConsoleLineMax) {
      say("CONFIG ERROR line too long\n");
      line_.clear();
      discarding_ = true;
      continue;
    }
    line_ += (char)c;
  }
  if (collecting_ && (int32_t)(millis_.millis32() - deadline_) >= 0) {
    say("CONFIG ERROR only " + std::to_string(bytes_.size()) + " of " + std::to_string(raw_.length) + " bytes\n");
    collecting_ = false;
    bytes_.clear();
  }
}

void Console::line(const std::string& text) { execute(parseConsoleLine(text)); }

void Console::execute(const ConsoleCommand& c) {
  using Kind = ConsoleCommand::Kind;
  switch (c.kind) {
    case Kind::None: return;
    case Kind::Unknown: say(std::string("CONFIG ERROR ") + (c.error ? c.error : "unusable") + "\n"); return;
    case Kind::List: {
      std::vector<FileEntry> entries;
      if (!store_.list("/config", entries)) {
        say("CONFIG ERROR no card\n");
        return;
      }
      say("CONFIG LIST BEGIN\n");
      for (const FileEntry& e : entries) say(e.name + " " + std::to_string(e.size) + "\n");
      say("CONFIG LIST END\n");
      return;
    }
    case Kind::Read: {
      const int64_t total = store_.fileSize(c.path);
      std::string content;
      uint32_t from = 0;
      if (total >= 0) {
        // Of a long log the end: what just went wrong is at the back.
        if (total > (int64_t)kConsoleMaxFile) {
          store_.readTail(c.path, content, kConsoleMaxFile);
          from = (uint32_t)(total - (int64_t)content.size());
        } else {
          store_.read(c.path, content, kConsoleMaxFile);
        }
      }
      say("CONFIG CONTENT " + c.file + " " + std::to_string(content.size()) + " " +
          std::to_string(consoleChecksum(reinterpret_cast<const uint8_t*>(content.data()), content.size())) + " from " +
          std::to_string(from) + " of " + std::to_string(total < 0 ? 0 : total) + "\n");
      say(content);
      say("\nCONFIG END\n");
      return;
    }
    case Kind::Write:
    case Kind::Web:
      raw_ = c;
      collecting_ = true;
      bytes_.clear();
      deadline_ = millis_.millis32() + kConsoleRawTimeoutMs;
      say("CONFIG READY " + std::to_string(c.length) + "\n");
      return;
    case Kind::Restart:
      say("CONFIG OK restart\n");
      host_.restart();
      return;
    case Kind::Battery:
      say("CONFIG BATTERY BEGIN\n");
      for (const std::string& l : host_.batteryReport()) say(l + "\n");
      say("CONFIG BATTERY END\n");
      return;
    case Kind::Sms:
      // The same way as the screens send: cellular first, then the queue.
      say("CONFIG OK sms " + c.phone + " " + std::to_string(c.text.size()) + " chars " +
          (host_.sendText(c.phone, c.text) ? "accepted" : "refused") + "\n");
      return;
  }
}

void Console::finishRaw() {
  collecting_ = false;
  if (raw_.kind == ConsoleCommand::Kind::Write) {
    const uint16_t sum = consoleChecksum(bytes_.data(), bytes_.size());
    if (sum != raw_.sum) {
      say("CONFIG ERROR checksum " + std::to_string(sum) + " instead of " + std::to_string(raw_.sum) + "\n");
    } else if (!store_.write(raw_.path, std::string_view(reinterpret_cast<const char*>(bytes_.data()), bytes_.size()))) {
      say("CONFIG ERROR cannot write\n");
    } else {
      say("CONFIG OK " + raw_.file + " " + std::to_string(bytes_.size()) + "\n");
    }
  } else {
    std::string response;
    if (!host_.handleWeb(bytes_, response)) say("CONFIG ERROR no web service\n");
    else {
      say("CONFIG WEB " + std::to_string(response.size()) + "\n");
      say(response);
      say("\n");
    }
  }
  bytes_.clear();
}

}  // namespace board
