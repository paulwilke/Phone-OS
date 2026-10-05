#pragma once

// The status, for the parents who hold the device: what they come here for and no more — the battery
// and how long it lasts, the child's name and the number of contacts, how visible a remote query is,
// the firmware and the hardware — and the way to switch the device off.
//
// **Remote queries.** Enter steps through the three degrees of visibility (silent, in the history,
// with a notice). The change lasts until the next start: the core holds it, the configuration file
// is written by the configuration.
//
// **Switching off** is written on the page, because nobody would find it otherwise, and takes two
// presses of x so that it cannot happen by accident; any other key takes the first back. With the
// cable plugged in the device cannot be switched off (USB keeps the board alive): the application's
// attempt fails and tells the core, which says so in the notice line; the page says it before.
//
// What the firmware and the hardware are called is the application's to say (build constants), so it
// hands them to the page.
//
// It is a text and scrolls with the arrows when a language makes it longer than the page.

#include <string>

#include "ui/AppScreen.h"
#include "ui/TextBlock.h"

namespace ui {

struct DeviceInfo {
  std::string firmware;
  std::string hardware;  // empty: no line
};

class StatusScreen : public AppScreen {
public:
  StatusScreen(ScreenContext context, DeviceInfo info) : AppScreen(context), info_(std::move(info)), block_(FontSize::Small) {}

  void onEnter() override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  std::string words();
  Rect textArea(const Rect& area);
  void stepQueries();

  DeviceInfo info_;
  TextBlock block_;
  std::string text_;   // what the block holds: set again only when the words change, or the position would reset
  bool offArmed_ = false;
};

}  // namespace ui
