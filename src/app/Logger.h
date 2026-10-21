#pragma once

// The core's log: writes a line to the EventLog unless the log level of the profile says it is
// not worth the space on the card. Which lines are worth what follows from their words
// (loglevel::classify, docs/core/log-level.md); a line that must never be dropped uses the
// vocabulary of the "always" class, and no setting can switch that off.
//
// The lines are English and technical: they are for the parents and for whoever repairs the
// device, not for the child, and they are not translated.

#include <string>
#include <string_view>

#include "../hal/EventLog.h"
#include "Profile.h"

class Logger {
public:
  Logger(EventLog& log, const Profile& profile) : log_(log), profile_(profile) {}

  void log(std::string_view line) {
    const std::string text(line);
    if (loglevel::shouldWrite(profile_.logLevel, text.c_str())) log_.log(line);
  }

private:
  EventLog& log_;
  const Profile& profile_;
};
