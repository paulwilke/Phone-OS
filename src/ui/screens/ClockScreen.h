#pragma once

// The clock: the page where the time is, on purpose. It left the status bar because every minute
// there cost a redraw of the panel; here the redraw is wanted, because somebody who opens the page
// wants the time. The time and the date come from the core's wall clock, which has them from the
// network; without one the page says so instead of showing an invented time.
//
// Soft keys: Back. The page has nothing else to do.

#include "ui/AppScreen.h"

namespace ui {

class ClockScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
};

}  // namespace ui
