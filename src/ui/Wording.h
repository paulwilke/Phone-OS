#pragma once

// The one place that turns the core's codes into words. The application core produces no
// sentence: a notice, a system entry of the history, the reason a call or Wi-Fi failed, and the
// network label arrive as kinds and values, and a screen asks here for the words when it draws.
// That is why a change of language also changes the entries written yesterday, and why a
// contact renamed since is renamed in the history too.
//
// Every code has a catalog text in every shipped language; a test walks all of them and fails for
// a code whose key is missing, so a missing translation is found by the tests and never as a key
// (`call.reason-busy`) on the child's screen. The one exception is deliberate: a system entry that
// this version does not know (written by a later version) or an entry from before the codes
// existed is shown as it was stored, because that is better than hiding it.

#include <string>

#include "app/AppCore.h"
#include "i18n/Translator.h"

namespace ui {
namespace wording {

// A minute of the day as "14:30". Digits only, so it is the same in every language; the date, which
// has words in it, is written by the status bar from the catalogs.
std::string clockTime(int minuteOfDay);

// When a message was written, as the history shows it: the time of day for today ("14:32"), the
// date and the time for an earlier day ("09/14 14:32", "14.09. 14:32"), and "--:--" for a message
// that arrived before the time was known. `now` is the wall clock of the moment.
std::string messageTime(const AppCore::State::WallClock& at, const AppCore::State::WallClock& now, i18n::Translator& t);

// Who a number is for the child: the contact's name, the number itself for a caller who is not in
// the book, and the word for "unknown" when there is no number at all.
std::string peerName(const std::string& peer, const phone::ContactBook& contacts, i18n::Translator& t);

// The line under the status bar. Empty for no notice. With `maxWidth` (pixels, in the small font of
// the bar) a name that makes the line too wide is shortened with "..." inside itself, so that the
// words after it ("busy", "refused") are never the part that is cut. A phone number is never
// shortened: the digits are the information.
std::string notice(const Notice& notice, const phone::ContactBook& contacts, i18n::Translator& t, int maxWidth = 0);

// A row of the message history that is not a message. Entries of source System only.
std::string systemEntry(const Message& message, const phone::ContactBook& contacts, i18n::Translator& t);

// Why a call did not happen, as a short phrase ("busy", "network allows SMS only"). `reason` is the
// code the core stores (`sysentry::kBusy`, ...); one this version does not know is shown as given.
std::string callReason(const std::string& reason, i18n::Translator& t);

// Why Wi-Fi is off or failed; empty for `None`.
std::string wifiReason(WifiReason reason, i18n::Translator& t);

// The network as the core hands it over: the operator's name, or a word for none, the radio and
// the radio-only save mode; "Network" when a network is there but its name is not.
std::string networkLabel(const Connectivity::NetworkLabel& label, i18n::Translator& t);

}  // namespace wording
}  // namespace ui
