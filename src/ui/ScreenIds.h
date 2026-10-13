#pragma once

// The names of the screens in the stack (ScreenStack.h). They live in one place because the screens
// open each other by name and none may know another's class: the home screen opens every page, the
// phone page opens the number and the call page, settings open Wi-Fi and the language page.
// `kHome` is the root of the stack and must stay the first.

#include "ui/Screen.h"

namespace ui {
namespace screen {

enum : ScreenId {
  kHome,
  kLock,
  kClock,
  kPhone,
  kNumber,
  kCall,
  kMessages,
  kCompose,
  kRecipient,
  kSos,
  kSettings,
  kWifi,
  kNet,
  kStatus,
  kLanguage,
  kGnss,
  kRadio,
  kMotion,
  kSelfTest,
  kPower,
  kCount  // not a screen: the number of names
};

static_assert(kHome == 0, "the root of the stack comes first");
static_assert(kCount <= 255, "a screen name is one byte");

}  // namespace screen
}  // namespace ui
