#pragma once

// The options: what the child may change on the device, one box per setting with the question above
// and the answer below. Enter (or a tap) changes the one in focus:
//
//   volume in calls   one step up, from 1 to 7 and round again; never silent, because a call that
//                     starts mute is a mistake and no wish. Only where the modem has a volume.
//   message vibration on or off; switching it on buzzes once, so that whoever wants it feels it.
//   Wi-Fi             opens the Wi-Fi page. Only for the parents' tools, and only where the device has
//                     Wi-Fi; the same condition as the Wi-Fi tile of the home page.
//   language          opens the language page; the box says the language now in use.
//
// The page changes nothing it does not show: every value is read from the core's state or profile at the
// moment it is drawn.

#include <vector>

#include "ui/AppScreen.h"
#include "ui/OptionRows.h"

namespace ui {

class SettingsScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override { rows_.resetSelection(); }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  enum class Kind { Volume, Vibration, Wifi, Language };

  std::vector<Kind> kinds();
  void rebuild();
  Rect rowsArea(const Rect& area);
  void activate(Kind kind);

  OptionRows rows_;
  std::vector<Kind> shown_;
};

}  // namespace ui
