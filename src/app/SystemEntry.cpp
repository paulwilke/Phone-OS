#include "SystemEntry.h"

#include <cstring>

namespace sysentry {

namespace {

bool isWord(const std::string& s) {
  for (char c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (!ok) return false;
  }
  return true;
}

}  // namespace

bool make(Message& msg, const std::string& kind, const std::string& argument) {
  if (kind.empty() || !isWord(kind) || !isWord(argument)) return false;
  std::string text = "@" + kind;
  if (!argument.empty()) text += " " + argument;
  if (text.size() >= sizeof(msg.text)) return false;
  msg.source = MsgSource::System;
  memcpy(msg.text, text.c_str(), text.size() + 1);
  return true;
}

bool parse(const Message& msg, Entry& out) {
  if (msg.source != MsgSource::System || msg.text[0] != '@') return false;
  const std::string text = msg.text + 1;
  const size_t space = text.find(' ');
  const std::string kind = text.substr(0, space);
  const std::string argument = space == std::string::npos ? "" : text.substr(space + 1);
  if (kind.empty() || !isWord(kind) || !isWord(argument)) return false;
  out.kind = kind;
  out.argument = argument;
  return true;
}

}  // namespace sysentry
