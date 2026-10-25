#pragma once

// The power page: what the child or a parent does with the device's power. It has one thing, switching it
// off, which asks once before it acts (decisions D-11/D-12): the device is often off in the pocket and on
// when it is used, and a tap in a pocket must not switch it off.
//
//   first view      one button, "Switch off". On a cable the row says "unplug USB first" and does nothing:
//                   USB keeps the board alive, so the battery switch would change nothing.
//   the question    "Switch off?" with two buttons, "Cancel" first (the focus starts there) and "Yes, switch
//                   off". Back, or Cancel, goes back to the first view; leaving the page forgets the question.
//
// "Power save" is not on this page: what it should do is decision D-13, which is open.

#include "ui/AppScreen.h"
#include "ui/OptionRows.h"

namespace ui {

class PowerScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override {
    asking_ = false;
    rows_.resetSelection();
  }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  std::string title();
  void rebuild();
  Rect rowsArea(const Rect& area);

  OptionRows rows_;
  bool asking_ = false;
};

}  // namespace ui
