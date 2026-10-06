#pragma once

// The log lines for what the device turns away: a text or a call from somebody who is not in the contact
// book, and a radio message from somebody who is not the configured peer. Each is logged, on every log
// level, because the parents must be able to see that something knocked (docs/log-level.md); but a flood
// must not fill the card, so:
//
//   * **at most one line per sender per ten minutes** (`kWindowSeconds`). What is dropped meanwhile is
//     counted, and the count comes as one more line when the ten minutes are over ("... (17 more in the
//     last 10 minutes)"), written by tick() or by the next drop, whichever comes first. The summary is a
//     line too: the next first line of that sender comes ten minutes after it;
//   * a sender is its number in canonical form, so one number written three ways is one sender; a sender
//     that is not a number (a name, a short code) is one sender by its text; a radio name is a sender of
//     its own kind, so a radio node that is called like an SMS sender is not the same;
//   * the first line carries the **text** of a dropped message (the first 120 characters, control
//     characters replaced by a space, nothing else of the message), so that the parents can read a
//     balance, a tariff or a PIN notice the network sent. A call has no text, and a message without text
//     has no text part. The log is readable through the web page with its password.
//
// The table is bounded (`kMaxSenders`): a flood from many different numbers cannot grow it, the oldest
// sender is forgotten first and may then get a line sooner than ten minutes after its last.
//
// Time is the uptime, which does not jump; a time that is earlier than the last line (it cannot happen
// with an uptime, but a host may give anything) is a new window, never a stalled log.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "Logger.h"

class DropLog {
public:
  static constexpr uint32_t kWindowSeconds = 600;
  static constexpr size_t kTextCharacters = 120;
  static constexpr size_t kMaxSenders = 32;

  // `seconds` is a steady clock in seconds.
  DropLog(Logger& log, std::function<uint32_t()> seconds) : log_(log), seconds_(std::move(seconds)) {}

  // A text from somebody who is not in the contact book. `countryCode` canonicalises the number.
  void smsDropped(const std::string& sender, const std::string& text, const char* countryCode);
  // A call from somebody who is not in the contact book (no text); an empty number is a withheld one.
  void callRejected(const std::string& number, const char* countryCode);
  // A radio message from a node that is not the configured peer ("" when none is configured).
  void radioDropped(const std::string& name, const std::string& text, const std::string& configuredPeer);

  // Writes the summaries whose ten minutes are over. Called regularly; cheap.
  void tick();

private:
  struct Entry {
    uint32_t lineAt = 0;    // when the last line for this sender was written
    uint32_t counted = 0;   // dropped since that line
    std::string label;      // the head of a summary line: what was dropped and from whom
  };

  // The line for a drop is `first` when the sender may have one now; otherwise the drop is counted. `label`
  // is the head of the summary that will count it.
  void drop(const std::string& key, const std::string& label, const std::string& first);
  void summarise(Entry& entry, uint32_t now);
  static std::string excerpt(const std::string& text);

  Logger& log_;
  std::function<uint32_t()> seconds_;
  std::map<std::string, Entry> senders_;
};
