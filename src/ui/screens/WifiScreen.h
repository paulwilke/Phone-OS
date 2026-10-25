#pragma once

// Wi-Fi, for the parents: three buttons — the home network, the device's own access point, off — and,
// under them, what somebody needs to join, or why it did not work. Enter (or a tap) switches to the one
// in focus; the core refuses what it cannot do (a battery too low, no home network stored) and says so in
// the notice line, and the page then shows the reason it keeps.
//
// The button of what runs now has a second outline (for the "Off" button: while Wi-Fi is off). Under the
// buttons:
//
//   off              what Wi-Fi is for here; the reason first when it went off by itself or did not start
//   connecting       the network it tries
//   connected / AP   the network, for the access point its password (that is why the page is for the
//                    parents' eyes only: the password is shown nowhere else), the address of the page
//                    and the minutes until it switches itself off
//   failed           the reason, and that the access point is still possible
//
// The words of the reasons are the core's codes worded by `wording::wifiReason`.

#include "ui/AppScreen.h"
#include "ui/OptionRows.h"

namespace ui {

class WifiScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override { rows_.resetSelection(); }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  void rebuild();
  Rect rowsArea(const Rect& area);
  std::string details();
  void activate(size_t index);

  OptionRows rows_;
};

}  // namespace ui
