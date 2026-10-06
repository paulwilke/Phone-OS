#pragma once

// The position page: what the satellite receiver knows, for whoever wants to see whether the device finds
// the sky. While it is open it holds the receiver (`setPositionPageOpen`, released when the page is left
// by any way), so the position is searched for as long as somebody looks.
//
// With a current fix the page gives four values, each a small label above a large number: latitude and
// longitude with five decimals (about a metre), the accuracy in metres, the satellites in use. Without one it
// says what is going on — searching (and that indoors there is often no reception), or that positioning is
// off — and, so that an old position is never taken for the present one, the last known position with its
// age. That text scrolls with the arrows when a language makes it long.
//
// A fix is current when the receiver runs and the position is at most a few seconds old; the core remembers
// the last fix (`lastKnownFix`) whether the receiver runs or not.

#include <string>

#include "ui/AppScreen.h"
#include "ui/TextBlock.h"

namespace ui {

class GnssScreen : public AppScreen {
public:
  explicit GnssScreen(ScreenContext context) : AppScreen(context), block_(FontSize::Small) {}

  void onEnter() override;
  void onLeave() override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  // A position seen at most this many seconds ago, while the receiver runs, is the present one.
  static constexpr uint32_t kCurrentSeconds = 5;

  bool current(GnssFix& fix);
  std::string words();
  Rect textArea(const Rect& area);

  TextBlock block_;
  std::string text_;
};

}  // namespace ui
