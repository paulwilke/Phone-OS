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

#include <optional>
#include <vector>

#include "ui/Display.h"
#include "ui/Input.h"

namespace ui {

// A screen's name in the stack: chosen by the application, opaque to the framework.
using ScreenId = uint8_t;

// A place on the screen that answers a tap: a soft key, a tile, a row of a list, a button. `key` is set when
// the tap stands for a key (the soft keys), empty when the widget under it decides what the tap does (a tile
// selects and opens). The screens report them so that the simulator's walk can tap what a finger can tap
// and take the pictures it reaches, and so that a test can tap the middle of each and expect it to be used.
struct TouchTarget {
  Rect rect;
  std::optional<Key> key;
};

class Screen {
public:
  virtual ~Screen() = default;

  // The screen became the visible one.
  virtual void onEnter() {}
  // The screen stopped being the visible one, for whatever reason.
  virtual void onLeave() {}

  // Called by ScreenStack::poll() on the visible screen only, so often as the application polls. For what
  // happens *while* the screen is shown: reading a sensor, or starting one after the first frame is on the
  // panel. Starting and stopping what runs because the screen is visible stays in onEnter and onLeave.
  virtual void onTick() {}

  // True when the input was used. Only the visible screen gets input.
  virtual bool handle(const Input& input) = 0;

  // Draws the whole screen. Called between Display::beginFrame() and endFrame().
  virtual void render(Display& display) = 0;

  // Screens that want to see the three keys under the screen themselves (a key test) say so
  // here; for every other screen the stack takes them.
  virtual bool wantsHardwareKeys() const { return false; }

  // A screen that must not be locked away while it is shown (a call in progress).
  virtual bool preventsAutoLock() const { return false; }

  // The places that answer a tap, as the screen was drawn last; none by default. Only what does something
  // now is listed (the rows a list shows, the tiles of the page on show), so that a tap in the middle of each
  // is used by handle().
  virtual std::vector<TouchTarget> touchTargets() { return {}; }
};

}  // namespace ui
