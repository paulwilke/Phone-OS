#pragma once

// The network, for adults: what the status bar says in a word, said in full. One line each for the SIM
// (only when there is a problem with it), the network, whether calls and messages work, the signal
// as a number ("4/5" reads even when the display is small), cellular power, the modem's sleep, whether the
// time is known, and how many messages wait. Under them a paragraph explains what a problem with the SIM
// is, or why a network that allows messages only is no fault of the device.
//
// It is a text, so it scrolls with the arrows when a language makes it longer than the page.

#include <string>

#include "ui/AppScreen.h"
#include "ui/TextBlock.h"

namespace ui {

class NetScreen : public AppScreen {
public:
  NetScreen(ScreenContext context) : AppScreen(context), block_(FontSize::Small) {}

  void onEnter() override { block_.setText(std::string()); text_.clear(); }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  std::string words();
  Rect textArea(const Rect& area);

  TextBlock block_;
  std::string text_;   // what the block holds: it is set again only when the words change, or the position would reset
};

}  // namespace ui
