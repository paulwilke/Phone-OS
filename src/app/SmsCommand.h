#pragma once

// The remote query by SMS. Pure logic: everything about the device comes in through
// CommandHost.
//
// There is exactly one query: a message that consists of nothing but a trigger word
// ("where are you"), in any case, with blanks around it, from an admin number
// (a parent in the contact book). The trigger words are given by the caller, so that
// every language can have its own: a German, an English and a French word at once.
// There is no secret word; the protection is the admin whitelist.
//
// The location itself is not found here but by the same flow as the SOS ping: receiver
// on, wait for an accurate fix, send the text to whoever asked. A second, separate
// location flow once failed on the device without a known cause; the SOS flow is
// proven and reports its failures.
//
// Accountability: every query is recorded through CommandHost::recordCommand(),
// executed or refused. That cannot be switched off; the setting decides only how
// visible the entry is on the device (CommandNotice).

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum class CommandNotice {
  Silent,   // the log only
  History,  // also an entry in the message history (the default)
  Notify    // also a visible notice
};

enum class CommandOutcome {
  Executed,
  Busy,          // a location reply or SOS ping is running already
  Unauthorized,  // not an admin number: no reply, but recorded
};

struct CommandRecord {
  uint32_t epochUtc = 0;  // 0 = time unknown
  uint32_t uptimeSec = 0;
  std::string phone;
  CommandOutcome outcome = CommandOutcome::Executed;
};

class CommandHost {
public:
  virtual ~CommandHost() = default;

  virtual bool isAdmin(const std::string& phone) = 0;
  // Starts the location search and the reply to `phone`. False when one is running.
  virtual bool startLocationReply(const std::string& phone) = 0;
  virtual uint32_t uptimeSeconds() = 0;  // monotonic
  virtual uint32_t epochUtc() = 0;       // 0 = unknown
  virtual void recordCommand(const CommandRecord& record, CommandNotice notice) = 0;
};

class SmsCommand {
public:
  struct Config {
    CommandNotice notice = CommandNotice::History;
    // The words that trigger the query. Letters A-Z match in either case; other
    // characters (umlauts, accents) must match exactly. Blanks around a word are
    // ignored and empty entries are skipped: an empty word would match every blank message.
    std::vector<std::string> triggers;
  };

  enum class Result {
    NotACommand,  // an ordinary message: store it
    Handled       // a command; do not store it as a message
  };

  SmsCommand(CommandHost& host, const Config& cfg) : host_(host) { setConfig(cfg); }

  Result handle(const std::string& phone, std::string_view body);

  // Changes the settings at run time. The log stays out of reach of this.
  void setConfig(const Config& cfg);
  void setNotice(CommandNotice notice) { notice_ = notice; }

private:
  CommandHost& host_;
  CommandNotice notice_ = CommandNotice::History;
  std::vector<std::string> triggers_;  // trimmed, none empty
};
