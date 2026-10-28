#pragma once

// The settings the behaviour of the device depends on, as one plain struct: who is in the
// contact book, when school time is, which vibrations are on, what the child may do. The
// application core reads it and never parses anything: src/config/ConfigLoader reads the
// configuration files into this struct. Tests fill it through testing/ProfileBuilder.h.
//
// The defaults are the ones the device had before anybody wrote a settings file: the
// behaviour of a freshly flashed device must not depend on a file being there.

#include <cstdint>
#include <string>
#include <vector>

#include "ContactBook.h"
#include "LogLevel.h"
#include "MapLink.h"
#include "QuietHours.h"
#include "SmsCommand.h"
#include "TimeZone.h"
#include "WifiSettings.h"

// Which tiles of the home screen's second page the child sees ([home] in settings.toml). All are shown unless the
// parents switch one off; Wi-Fi and Status still need the parent tools, Motion a motion sensor.
struct HomeTiles {
  bool network = true;
  bool gps = true;
  bool clock = true;
  bool settings = true;
  bool wifi = true;
  bool motion = true;
  bool tests = true;
  bool status = true;
};

struct Profile {
  // --- people and time ---
  phone::ContactBook contacts;
  QuietHours quietHours;
  // The numbers of the home-screen call actions, in the order the parents arranged them. They
  // may be called even when they are not in the contact book, and the first one is who the SOS
  // button reaches when it has no explicit target.
  std::vector<std::string> callTargets;
  // The contacts with `sos = true`, in the order of the file: the SOS goes to every one of them. Empty: the
  // SOS goes to the first call target, as in 4.0.
  std::vector<std::string> sosContacts;
  // The zone school time and the clock follow; nullptr = the clock keeps the offset it has.
  const tz::Zone* timeZone = nullptr;

  // --- what the child may do ---
  bool freeText = false;       // may write free text and choose any recipient
  bool radioAllowed = false;   // the long-range radio may be used (it still starts only on request)
  bool parentTools = false;    // settings and Wi-Fi are shown on the device
  HomeTiles home;

  // --- vibration ---
  bool messageVibration = true;
  bool callVibration = true;
  bool quietCallVibration = false;  // a call in school time is rejected silently unless this is on

  // --- power and connectivity ---
  bool modemSleep = true;
  WifiSettings wifi;

  // --- what the device says ---
  std::string childName;                               // in the SOS texts; empty = anonymous texts
  std::string mapTemplate = maplink::defaultTemplate();
  // The access point name of the mobile data context the modem needs for the IMS (calls over LTE).
  // Empty: the modem's own contexts are left alone.
  std::string modemApn;
  // The size of the battery in mAh. The fuel gauge ships with a factory size that is not ours, and the device
  // writes this value into it at start when they differ (docs/hardware/hardware-lessons.md).
  int batteryCapacityMah = 1500;
  std::string interfaceLanguage = "en";                // language code for the device UI; the child may change it in the options
  std::string smsLanguage = "en";                      // the language of every SMS the device sends
  // Further words that trigger the remote query, besides the trigger word of every language of
  // the catalogs.
  std::vector<std::string> extraTriggers;
  bool remoteQueries = true;  // false: the device refuses every remote query (D-54)
  CommandNotice commandNotice = CommandNotice::History;  // how visible a remote query is on the device
  loglevel::Level logLevel = loglevel::Level::All;

  // --- radio ---
  std::string radioName = "phone-os";  // this device's name in the radio network
  std::string radioPeer;                // default peer to send radio messages to

  // May the child call this number? It must be in the contact book (any spelling of the
  // number) or be the target of a call action.
  bool mayCall(const std::string& number) const;

  // May this sender reach the child, by SMS or by call? The household's central rule: only
  // the contact book and the targets of the call actions. It is the same notion as mayCall
  // (the same number in any spelling; a name, a short code or nothing at all is never allowed),
  // kept as one function so that messages and calls cannot drift apart. There is no switch.
  bool mayReceiveFrom(const std::string& number) const;

  // Whom the SOS button reaches: `target` if given, otherwise the first of sosRecipients(); empty when
  // there is nobody, in which case there is no SOS button.
  std::string sosRecipient(const std::string& target) const;
  // Everybody an SOS goes to: the ticked contacts, or else the first call target alone; empty when there
  // is nobody.
  std::vector<std::string> sosRecipients() const;

  // Is school time running? `epochUtc` 0 (time unknown) never locks. With `endMinute` the minute
  // of the day school time ends is returned.
  bool schoolTime(uint32_t epochUtc, int tzOffsetMinutes, int* endMinute = nullptr) const {
    return quietHours.active(epochUtc, tzOffsetMinutes, endMinute);
  }
};
