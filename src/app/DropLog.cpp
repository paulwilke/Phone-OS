#include "DropLog.h"

#include "PhoneNumber.h"
#include "Utf8.h"

namespace {

std::string countText(uint32_t n) { return std::to_string(n) + " more in the last 10 minutes"; }

}  // namespace

// The first kTextCharacters characters, a control character (a line break, a tab, an escape, a byte of a
// broken sequence that is below a space) replaced by a space, never cut inside a character.
std::string DropLog::excerpt(const std::string& text) {
  const size_t bytes = utf8::prefixBytes(text.data(), text.size(), kTextCharacters);
  std::string out = text.substr(0, bytes);
  for (char& c : out)
    if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) == 0x7f) c = ' ';
  return out;
}

void DropLog::summarise(Entry& entry, uint32_t now) {
  if (entry.counted == 0) return;
  // The words that keep the line on every log level are in the head: it is the same knock.
  log_.log(entry.label + " ... (" + countText(entry.counted) + ")");
  entry.counted = 0;
  entry.lineAt = now;
}

void DropLog::drop(const std::string& key, const std::string& label, const std::string& first) {  // label: the head of a summary line
  const uint32_t now = seconds_();
  auto it = senders_.find(key);
  if (it != senders_.end()) {
    Entry& entry = it->second;
    const bool fresh = now < entry.lineAt || now - entry.lineAt >= kWindowSeconds;
    if (!fresh) {
      ++entry.counted;
      return;
    }
    summarise(entry, now);
    entry.lineAt = now;
    entry.label = label;
    log_.log(first);
    return;
  }
  if (senders_.size() >= kMaxSenders) {
    // Forget the sender that was heard longest ago; its count, if any, is written first.
    auto oldest = senders_.begin();
    for (auto s = senders_.begin(); s != senders_.end(); ++s)
      if (s->second.lineAt < oldest->second.lineAt) oldest = s;
    summarise(oldest->second, now);
    senders_.erase(oldest);
  }
  Entry entry;
  entry.lineAt = now;
  entry.label = label;
  senders_[key] = entry;
  log_.log(first);
}

void DropLog::tick() {
  const uint32_t now = seconds_();
  for (auto& kv : senders_) {
    Entry& entry = kv.second;
    if (entry.counted > 0 && (now < entry.lineAt || now - entry.lineAt >= kWindowSeconds)) summarise(entry, now);
  }
}

void DropLog::smsDropped(const std::string& sender, const std::string& text, const char* countryCode) {
  const std::string number = phone::canonical(sender.c_str(), countryCode);
  const std::string who = number.empty() ? (sender.empty() ? std::string("non-numeric sender") : "non-numeric sender " + sender) : number;
  const std::string head = "SMS dropped: sender not in the contact book (" + who + ")";
  std::string line = head;
  const std::string shown = excerpt(text);
  if (!shown.empty()) line += ": " + shown;
  drop("num:" + (number.empty() ? sender : number), head, line);
}

void DropLog::callRejected(const std::string& number, const char* countryCode) {
  if (number.empty()) {
    drop("num:withheld", "Call rejected: number withheld", "Call rejected: number withheld");
    return;
  }
  const std::string canonical = phone::canonical(number.c_str(), countryCode);
  const std::string who = canonical.empty() ? "non-numeric number " + number : canonical;
  const std::string head = "Call rejected: caller not in the contact book (" + who + ")";
  drop("num:" + (canonical.empty() ? number : canonical), head, head);
}

void DropLog::radioDropped(const std::string& name, const std::string& text, const std::string& configuredPeer) {
  const std::string shown = excerpt(text);
  const std::string head = "Radio message dropped: sender not the configured radio peer (" + name + ")" +
                           (configuredPeer.empty() ? ", no radio peer is configured" : "");
  std::string line = head;
  if (!shown.empty()) line += ": " + shown;
  drop("radio:" + name, head, line);
}
