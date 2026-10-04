#pragma once

// A screen: one full-height page of the interface (home, a conversation, the lock screen).
// Screens are owned by the application and managed by a ScreenStack, which decides which
// one is visible.
//
// **onEnter and onLeave bracket the time a screen is visible**, and they are the only place
// where a screen starts or stops anything that runs because it is visible (the position
// fix while the position page is open, the motion sensor while its page is open, a timer).
// The stack calls them on every way a screen can appear or disappear: opening another screen,
// going back, the home key, locking, unlocking, an incoming call. A screen therefore never
// has to guess from the outside whether it is still on show, and nothing is left running
// because one of those ways was forgotten. They always alternate: onEnter, onLeave, onEnter...

#include "ui/Display.h"
#include "ui/Input.h"

namespace ui {

// A screen's name in the stack: chosen by the application, opaque to the framework.
using ScreenId = uint8_t;

class Screen {
public:
  virtual ~Screen() = default;

  // The screen became the visible one.
  virtual void onEnter() {}
  // The screen stopped being the visible one, for whatever reason.
  virtual void onLeave() {}

  // True when the input was used. Only the visible screen gets input.
  virtual bool handle(const Input& input) = 0;

  // Draws the whole screen. Called between Display::beginFrame() and endFrame().
  virtual void render(Display& display) = 0;

  // Screens that want to see the three keys under the screen themselves (a key test) say so
  // here; for every other screen the stack takes them.
  virtual bool wantsHardwareKeys() const { return false; }

  // A screen that must not be locked away while it is shown (a call in progress).
  virtual bool preventsAutoLock() const { return false; }
};

}  // namespace ui
