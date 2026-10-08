#pragma once

// The vocabulary of what the core tells the user, as codes. The application core contains
// no sentence: it produces *notices* (the line that shows for a few seconds under the status
// bar) and *system entries* (rows of the message history, see SystemEntry.h) as a kind with
// a few values, and the interface words them from the catalogs when it draws. Switching the
// language therefore changes the words of an entry that was written yesterday.
//
// This file is also the one place that says which catalog key words which code, so that
// a test can check that every code has a text in every shipped language with the placeholders
// the code supplies, and `scripts/check_i18n.py --usage` finds a key nothing uses.

#include <cstdint>
#include <string>

#include "SystemEntry.h"

enum class NoticeKind : uint8_t {
  // Refusals of what the child tried
  SchoolTimeUntil,    // minute: the minute of the day school time ends
  CallNotAllowed,
  CellularIsOff,
  NoNetwork,
  // A call that did not happen; peer: the number
  CallBusy,
  CallNoAnswer,
  CallNoNetwork,
  CallSmsOnly,
  CallNotReady,
  CallModemRestarted,
  CallFailed,
  // An incoming call from a contact rejected in school time (a stranger is rejected without a notice)
  CallerInSchoolTime,
  // Messages
  MessageWaiting,
  MessageSent,
  MessageSentRadio,
  QueueFull,
  SmsRejected,
  RadioUnreachable,
  RadioUnconfirmed,
  // Remote queries, shown when the parents allow it; peer: the number that asked
  LocationQueried,
  QueryRefused,
  // The device
  CellularSwitchedOn,
  CellularSwitchedOff,
  WifiBatteryLow,
  PowerOffRefused,
  LightOn,
  LightOff,
  Count  // not a notice: the number of kinds
};

struct Notice {
  NoticeKind kind = NoticeKind::Count;
  // The other party as a phone number; the interface looks the name up in the contact book
  // when it draws, so a contact renamed later is renamed in what is shown.
  std::string peer;
  int minute = -1;  // minute of the day, for SchoolTimeUntil

  bool none() const { return kind == NoticeKind::Count; }
  bool operator==(const Notice& o) const { return kind == o.kind && peer == o.peer && minute == o.minute; }
};

// How a notice is worded: the catalog key, and the placeholders the code supplies. `name` is
// the display name of `peer`; `time` is `minute` written as the language writes a time.
struct NoticeWording {
  const char* key;
  bool name;
  bool time;
};
const NoticeWording& noticeWording(NoticeKind kind);

// The same for system entries: the catalog key of a kind (nullptr for a kind this version
// does not know), which has the placeholder {name} (the peer) and, for a failed call,
// {reason}; and the key of a reason (nullptr when unknown).
const char* systemKey(const std::string& kind);
const char* callReasonKey(const std::string& reason);
