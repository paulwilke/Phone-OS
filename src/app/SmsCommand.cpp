#include "SmsCommand.h"

namespace {

bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

std::string_view trim(std::string_view s) {
  while (!s.empty() && isBlank(s.front())) s.remove_prefix(1);
  while (!s.empty() && isBlank(s.back())) s.remove_suffix(1);
  return s;
}

bool sameIgnoringCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

}  // namespace

void SmsCommand::setConfig(const Config& cfg) {
  notice_ = cfg.notice;
  enabled_ = cfg.enabled;
  triggers_.clear();
  for (const std::string& word : cfg.triggers) {
    const std::string_view t = trim(word);
    if (!t.empty()) triggers_.emplace_back(t);
  }
}

SmsCommand::Result SmsCommand::handle(const std::string& phone, std::string_view body) {
  // Nothing but the word: "where are you today?" is a message for the child.
  const std::string_view text = trim(body);
  bool match = false;
  for (const std::string& word : triggers_) {
    if (sameIgnoringCase(text, word)) {
      match = true;
      break;
    }
  }
  if (!match) return Result::NotACommand;

  CommandRecord record;
  record.epochUtc = host_.epochUtc();
  record.uptimeSec = host_.uptimeSeconds();
  record.phone = phone;

  if (phone.empty() || !host_.isAdmin(phone)) {
    // No reply: a stranger does not learn that the command exists.
    record.outcome = CommandOutcome::Unauthorized;
  } else if (!enabled_) {
    record.outcome = CommandOutcome::Disabled;
  } else {
    record.outcome = host_.startLocationReply(phone) ? CommandOutcome::Executed : CommandOutcome::Busy;
  }
  host_.recordCommand(record, notice_);
  return Result::Handled;
}
