#pragma once

// Which screen is visible, how the user gets back, and when the device locks. The screens
// belong to the application; the stack only points at them.
//
// The stack is a short path from the root screen (home): opening a screen pushes it, back pops
// it, the home key goes back to the root. Opening a screen that is already on the path goes
// back to it instead of putting it on twice, so "compose -> more -> new number -> compose"
// cannot fill the path and leave "back" in a screen the user just came from. When the path is
// full the newest screen is replaced.
//
// **Visible** is the top of the path, or the lock screen while the device is locked. Every
// change of the visible screen, whatever caused it, calls onLeave() on the screen that was
// visible and onEnter() on the one that is now (see Screen.h). Nothing else calls them.
//
// A screen must not navigate from inside onEnter() or onLeave(); such a call is ignored, so
// that the path stays consistent while a transition is in progress.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>

#include "ui/Screen.h"

namespace ui {

// What a screen may do to the path. Handed to screens by the application.
class Navigation {
public:
  virtual ~Navigation() = default;
  virtual void open(ScreenId id) = 0;
  virtual void back() = 0;
  virtual void home() = 0;     // back to the root screen
  virtual void lock() = 0;
  virtual void unlock() = 0;   // leaves the lock screen for the root screen
};

class ScreenStack : public Navigation {
public:
  static constexpr size_t kMaxDepth = 4;

  explicit ScreenStack(ScreenId root) : root_(root) { path_[0] = root; }

  // Registers a screen. Screens are added before begin().
  void add(ScreenId id, Screen& screen) { screens_[id] = &screen; }
  // Is a screen registered under this name? (The test that every name in ScreenIds.h is wired asks.)
  bool registered(ScreenId id) const { return screens_.count(id) != 0; }
  // Which registered screen is shown while the device is locked. A locked device needs one;
  // without it lock() does nothing.
  void setLockScreen(ScreenId id) { lock_ = id; hasLock_ = true; }
  // Screens the user may not open (a role that has no messages): open() ignores the id when
  // the filter says no. The root, the lock screen and interrupts are not subject to it.
  void setOpenFilter(std::function<bool(ScreenId)> filter) { filter_ = std::move(filter); }
  // What one of the three keys under the screen does. Without an action the key is passed to
  // the visible screen like any other. While locked the keys do nothing here.
  void setHardwareKey(Key key, std::function<void(Navigation&)> action) { actions_[key] = std::move(action); }
  // Seconds without input after which the device locks; 0 turns it off.
  void setAutoLockSeconds(uint32_t seconds) { autoLockSeconds_ = seconds; }

  // Starts at the root screen. `nowSeconds` is any steady clock in seconds.
  void begin(uint32_t nowSeconds);

  // Routes the input; true when something used it. Counts as activity for the auto-lock.
  bool handle(const Input& input, uint32_t nowSeconds);
  void render(Display& display);
  // Call regularly: ticks the visible screen (Screen::onTick), then locks the device after the idle time,
  // unless the visible screen forbids it.
  void poll(uint32_t nowSeconds);

  // Brings a screen to the front even from the lock screen (an incoming call), keeping the path
  // under it. endInterrupt() goes back when that screen is still the visible one, and locks the device
  // again when the interrupt found it locked: a call that rang in a pocket must not leave the device open
  // to the next touch (seen in the simulator: a tap after the call started an SOS).
  void interrupt(ScreenId id, uint32_t nowSeconds);
  void endInterrupt(ScreenId id);

  ScreenId visibleId() const { return locked_ ? lock_ : path_[depth_ - 1]; }
  // The visible screen itself, or null before begin() or when its id is not registered.
  Screen* visible() const { return screen(visibleId()); }
  bool locked() const { return locked_; }
  size_t depth() const { return depth_; }
  ScreenId pathAt(size_t index) const { return path_[index]; }

  // Navigation
  void open(ScreenId id) override;
  void back() override;
  void home() override;
  void lock() override;
  void unlock() override;

private:
  Screen* screen(ScreenId id) const;
  // Runs `change`, then reports the difference in what is visible to the screens.
  template <class Change>
  void transition(Change&& change);
  void push(ScreenId id);

  std::map<ScreenId, Screen*> screens_;
  std::map<Key, std::function<void(Navigation&)>> actions_;
  std::function<bool(ScreenId)> filter_;
  ScreenId root_;
  ScreenId lock_ = 0;
  bool hasLock_ = false;
  ScreenId path_[kMaxDepth];
  size_t depth_ = 1;
  bool locked_ = false;
  bool relockAfterInterrupt_ = false;
  bool begun_ = false;
  bool transitioning_ = false;
  uint32_t lastInput_ = 0;
  uint32_t autoLockSeconds_ = 0;
};

}  // namespace ui
