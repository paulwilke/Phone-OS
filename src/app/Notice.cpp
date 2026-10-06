#include "Notice.h"

#include <cstring>

namespace {

// In the order of NoticeKind; a test checks that the table and the enum agree.
const NoticeWording kWording[] = {
    {"school.until", false, true},             // SchoolTimeUntil
    {"notice.call-not-allowed", false, false},
    {"notice.cellular-is-off", false, false},
    {"notice.no-network", false, false},
    {"call.busy", true, false},                // CallBusy
    {"call.no-answer", true, false},
    {"call.no-network", true, false},
    {"call.sms-only", true, false},
    {"call.not-ready", true, false},
    {"call.modem-restarted", true, false},
    {"call.failed", true, false},
    {"notice.caller-in-school-time", true, false},
    {"notice.message-waiting", false, false},
    {"notice.message-sent", false, false},
    {"notice.message-sent-radio", false, false},
    {"notice.queue-full", false, false},
    {"notice.sms-rejected", false, false},
    {"notice.radio-unreachable", false, false},
    {"notice.radio-unconfirmed", false, false},
    {"system.location-queried", true, false},  // LocationQueried repeats the words of the history entry (a parent asks, who is in the book)
    {"notice.query-refused", true, false},     // short: a stranger asks by number, and a number is never shortened
    {"notice.cellular-switched-on", false, false},
    {"notice.cellular-switched-off", false, false},
    {"notice.save-mode-needs-radio", false, false},
    {"notice.save-mode-on", false, false},
    {"notice.save-mode-no-wifi", false, false},
    {"notice.wifi-battery-low", false, false},
    {"notice.power-off-refused", false, false},
    {"notice.light-on", false, false},
    {"notice.light-off", false, false},
};

static_assert(sizeof(kWording) / sizeof(kWording[0]) == static_cast<size_t>(NoticeKind::Count),
              "one wording for every notice kind");

}  // namespace

const NoticeWording& noticeWording(NoticeKind kind) {
  return kWording[static_cast<size_t>(kind) < static_cast<size_t>(NoticeKind::Count) ? static_cast<size_t>(kind) : 0];
}

const char* systemKey(const std::string& kind) {
  if (kind == sysentry::kMissedCall) return "system.missed-call";
  if (kind == sysentry::kCallFailed) return "system.call-failed";
  if (kind == sysentry::kLocationQueried) return "system.location-queried";
  if (kind == sysentry::kQueryRefused) return "system.query-refused";
  if (kind == sysentry::kLocationQueryFailed) return "system.location-query-failed";
  if (kind == sysentry::kSmsRejected) return "system.sms-rejected";
  return nullptr;
}

const char* callReasonKey(const std::string& reason) {
  if (reason == sysentry::kBusy) return "call.reason-busy";
  if (reason == sysentry::kNoAnswer) return "call.reason-no-answer";
  if (reason == sysentry::kNoNetwork) return "call.reason-no-network";
  if (reason == sysentry::kSmsOnly) return "call.reason-sms-only";
  if (reason == sysentry::kTelephonyNotReady) return "call.reason-not-ready";
  if (reason == sysentry::kModemRestarted) return "call.reason-modem-restarted";
  if (reason == sysentry::kNotConnected) return "call.reason-not-connected";
  return nullptr;
}
