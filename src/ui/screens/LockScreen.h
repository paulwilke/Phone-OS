#pragma once

// The lock: a key lock without a PIN. Instead of a clock, what one wants to know without unlocking
// — is the device reachable, is there something new. There is no clock because the lock is the
// permanent state, and a clock would be the one thing that changes by itself: a redraw of the panel
// every minute, visible flicker, and current for nothing.
//
// **Unlocking takes the keyboard's Enter key twice in a row.** A tap on the screen and the three
// sensor keys under it never unlock: in a pocket the cloth touches the screen, and two taps came
// together too easily (found on the device). Any other key between the two presses starts again.
// For the same reason the screen shows no soft keys: a button that did nothing when tapped would be
// a lie. Unlocking goes to the home screen (the stack's `unlock`).
//
// The front light goes off when the device locks: a light that burns on in a pocket empties the
// battery over a night.

#include "ui/AppScreen.h"

namespace ui {

class LockScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override;

  // The first Enter has been seen.
  bool armed() const { return armed_; }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  bool armed_ = false;
};

}  // namespace ui
