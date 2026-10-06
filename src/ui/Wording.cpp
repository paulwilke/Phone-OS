#include "ui/Wording.h"

#include <cstdio>

#include "ui/Fonts.h"
#include "ui/StatusBar.h"
#include "ui/Text.h"

namespace ui {
namespace wording {

std::string clockTime(int minuteOfDay) {
  if (minuteOfDay < 0) minuteOfDay = 0;
  minuteOfDay %= 24 * 60;
  char out[8];
  snprintf(out, sizeof out, "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60);
  return out;
}

std::string weekdayName(const AppCore::State::WallClock& wall, i18n::Translator& t) {
  static const char* const kKeys[7] = {"clock.weekday.sun", "clock.weekday.mon", "clock.weekday.tue", "clock.weekday.wed",
                                       "clock.weekday.thu", "clock.weekday.fri", "clock.weekday.sat"};
  if (!wall.known) return std::string();
  return t.get(kKeys[((wall.weekday % 7) + 7) % 7]);
}

std::string dateLine(const AppCore::State::WallClock& wall, i18n::Translator& t) {
  static const char* const kMonths[12] = {"clock.month.01", "clock.month.02", "clock.month.03", "clock.month.04",
                                          "clock.month.05", "clock.month.06", "clock.month.07", "clock.month.08",
                                          "clock.month.09", "clock.month.10", "clock.month.11", "clock.month.12"};
  if (!wall.known) return std::string();
  const std::string month = t.get(kMonths[((wall.month - 1) % 12 + 12) % 12]);
  return t.get("clock.date", {{"month", month.c_str()}, {"day", wall.day}, {"year", wall.year}});
}

std::string messageTime(const AppCore::State::WallClock& at, const AppCore::State::WallClock& now, i18n::Translator& t) {
  if (!at.known) return "--:--";
  const std::string time = clockTime(at.minuteOfDay);
  if (now.known && at.month == now.month && at.day == now.day) return time;
  char month[8], day[8];
  snprintf(month, sizeof month, "%02d", at.month);
  snprintf(day, sizeof day, "%02d", at.day);
  return t.get("status.date-short", {{"month", month}, {"day", day}}) + " " + time;
}

std::string peerName(const std::string& peer, const phone::ContactBook& contacts, i18n::Translator& t) {
  const std::string name = contacts.displayName(peer.c_str());
  return name.empty() ? t.get("peer.unknown") : name;
}

namespace {

std::string fill(i18n::Translator& t, const NoticeWording& w, const std::string& name, const std::string& time) {
  if (w.name && w.time) return t.get(w.key, {{"name", name.c_str()}, {"time", time.c_str()}});
  if (w.name) return t.get(w.key, {{"name", name.c_str()}});
  if (w.time) return t.get(w.key, {{"time", time.c_str()}});
  return t.get(w.key);
}

}  // namespace

std::string notice(const Notice& n, const phone::ContactBook& contacts, i18n::Translator& t, int maxWidth) {
  if (n.none()) return std::string();
  const NoticeWording& w = noticeWording(n.kind);
  std::string name = w.name ? peerName(n.peer, contacts, t) : std::string();
  const std::string time = w.time ? clockTime(n.minute) : std::string();
  const std::string line = fill(t, w, name, time);
  const Font& font = ui::font(FontSize::Small);
  // A number stands for itself: a caller who is in no book is shown by it, and it is not cut.
  const bool isNumber = w.name && contacts.find(n.peer.c_str()) == nullptr && !n.peer.empty();
  if (maxWidth <= 0 || !w.name || isNumber || text::width(font, line) <= maxWidth) return line;
  const int room = maxWidth - text::width(font, fill(t, w, std::string(), time));
  const std::string shortened = text::ellipsize(font, name, room);
  return shortened.empty() ? line : fill(t, w, shortened, time);
}

std::string callReason(const std::string& reason, i18n::Translator& t) {
  const char* key = callReasonKey(reason);
  return key ? t.get(key) : reason;
}

std::string systemEntry(const Message& message, const phone::ContactBook& contacts, i18n::Translator& t) {
  sysentry::Entry entry;
  if (!sysentry::parse(message, entry)) return message.text;
  const char* key = systemKey(entry.kind);
  if (!key) return message.text;
  const std::string name = peerName(message.peer, contacts, t);
  if (entry.kind == sysentry::kCallFailed) {
    const std::string reason = callReason(entry.argument, t);
    return t.get(key, {{"name", name.c_str()}, {"reason", reason.c_str()}});
  }
  return t.get(key, {{"name", name.c_str()}});
}

std::string failedCallReason(const Notice& n) {
  switch (n.kind) {
    case NoticeKind::CallBusy: return sysentry::kBusy;
    case NoticeKind::CallNoAnswer: return sysentry::kNoAnswer;
    case NoticeKind::CallNoNetwork: return sysentry::kNoNetwork;
    case NoticeKind::CallSmsOnly: return sysentry::kSmsOnly;
    case NoticeKind::CallNotReady: return sysentry::kTelephonyNotReady;
    case NoticeKind::CallModemRestarted: return sysentry::kModemRestarted;
    case NoticeKind::CallFailed: return sysentry::kNotConnected;
    default: return std::string();
  }
}

std::string wifiReason(WifiReason reason, i18n::Translator& t) {
  // No default: a reason added to the enum without a text is a compiler warning here, and a
  // test walks every value besides.
  switch (reason) {
    case WifiReason::None: return std::string();
    case WifiReason::NoHomeNetwork: return t.get("wifi.reason-no-home-network");
    case WifiReason::HomeNetworkUnreachable: return t.get("wifi.reason-home-network-unreachable");
    case WifiReason::AccessPointFailed: return t.get("wifi.reason-access-point-failed");
    case WifiReason::AccessPointLost: return t.get("wifi.reason-access-point-lost");
    case WifiReason::SwitchedOffByItself: return t.get("wifi.reason-switched-off-by-itself");
  }
  return std::string();
}

std::string networkLabel(const Connectivity::NetworkLabel& label, i18n::Translator& t) {
  // The status bar has the same words; asking it with a model that has nothing else to say
  // keeps them in one place.
  StatusModel model;
  switch (label.kind) {
    case Connectivity::NetworkLabel::Kind::None: model.link = StatusModel::Link::None; break;
    case Connectivity::NetworkLabel::Kind::Operator: model.link = StatusModel::Link::Operator; break;
    case Connectivity::NetworkLabel::Kind::Radio: model.link = StatusModel::Link::Radio; break;
    case Connectivity::NetworkLabel::Kind::RadioOnly: model.link = StatusModel::Link::RadioOnly; break;
  }
  model.operatorName = label.name;
  return statusLeft(model, t).text;
}

}  // namespace wording
}  // namespace ui
