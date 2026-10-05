#pragma once

// System entries of the message history ("missed call from Grandma", "call to Parent One
// failed: busy"). The application core produces no sentences, so such an entry is stored
// as a **kind and an argument**; the interface words it from the catalogs when it draws
// the history, which is why switching the language also changes the old entries.
//
// The representation is the `text` field of a message whose source is `System`:
//
//   @<kind>            "@missed-call"
//   @<kind> <argument> "@call-failed busy"
//
// `kind` and `argument` consist of the lower-case letters, digits and hyphens of a catalog
// key, so a stored entry is one word or two and survives the log file unchanged (tabs and
// line breaks never occur in it). The other party, where there is one, is the `peer` field
// of the message as for every other entry; the name is looked up in the contact book when
// the entry is drawn, so a contact renamed later is renamed in the history too.
//
// The file format has not changed: it is still one line per message with a text. Entries
// written by earlier versions hold a sentence, which does not start with `@`; parse()
// refuses them and the interface shows the text as it is.

#include <string>

#include "MessageStore.h"

namespace sysentry {

// The kinds the core writes, and what the argument of each is.
constexpr const char* kMissedCall = "missed-call";            // peer: the caller
constexpr const char* kCallFailed = "call-failed";            // peer: the number called; argument: the reason (below)
constexpr const char* kLocationQueried = "location-queried";  // peer: the parent who asked
constexpr const char* kQueryRefused = "query-refused";        // peer: the number that asked
constexpr const char* kSmsRejected = "sms-rejected";          // peer: the recipient the network refused

// The reasons of a failed call.
constexpr const char* kBusy = "busy";
constexpr const char* kNoAnswer = "no-answer";
constexpr const char* kNoNetwork = "no-network";
constexpr const char* kSmsOnly = "sms-only";
constexpr const char* kNotConnected = "not-connected";

struct Entry {
  std::string kind;
  std::string argument;  // empty when the entry has none
};

// Turns `msg` into a system entry: source System and the encoded text. The peer, time and
// direction are the caller's. Returns false and leaves `msg` unchanged when the kind is
// empty or kind or argument holds a character outside [a-z0-9-].
bool make(Message& msg, const std::string& kind, const std::string& argument = std::string());

// Reads the kind and argument of a system entry. False for any other message, including
// an older system entry that holds a sentence.
bool parse(const Message& msg, Entry& out);

// Writes a system entry into the history: an incoming, unread message of source System with the
// given time, the other party in `peer` and the encoded kind. Returns the number the store gave
// it, 0 when the kind or argument is not a catalog word.
uint32_t record(MessageStore& store, uint32_t time, const std::string& kind, const std::string& peer = std::string(),
                const std::string& argument = std::string());

}  // namespace sysentry
