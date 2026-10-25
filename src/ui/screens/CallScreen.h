#pragma once

// A call: dialling, ringing, connected — and the reason a call did not happen. The page follows
// the core's call state, not what the screen did last, so it is right whichever way it was opened:
// from the phone page after a dial, or by the stack when a call rings (an interrupt).
//
//   dialling   "Calling ..."   Back is "Hang up"
//   ringing    "Incoming call" Back is "Reject", the action is "Answer"
//   connected  "In call"       Hang up is the whole key bar; the volume has two buttons on the page,
//                              "Quieter" and "Louder", and the keys Up and Down change it too
//   no call    "No call"       Back goes back
//   failed     "Call failed"   and why: the name, and "busy", "no answer", "no network", ...
//
// The name of the other party is large — a contact's name, or the number, which is never shortened:
// when it does not fit in the large font it is drawn in the next smaller, so all digits are there.
//
// **A call that did not happen stays on the page until Back**: the core clears the call and tells why
// in the notice line, which goes after a few seconds, and the page shows the reason for as long as
// that notice is up, with no call behind it. The page does not close itself; closing is the
// host's decision (`ScreenStack::endInterrupt` or `back`), and it leaves the failure to be read.
//
// The volume is shown as a row of boxes, filled up to the level (seven at most): a number says little
// to a child, and boxes are drawn, so they need no glyphs. It can be changed only during a call, where
// the sound runs through the modem; the core refuses at any other time and the page shows no boxes.
//
// A call in progress keeps the device from locking, and **keeps the three keys under the screen**
// while it is dialled, rings or is connected: they are touch keys, a cheek or a finger touches them
// by accident, and the stack would leave the page with nothing to bring it back during the call (no
// way to answer, to hang up or to see that a call runs). Back hangs up as before; after the call the
// keys work again.

#include "ui/AppScreen.h"

namespace ui {

class CallScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  bool preventsAutoLock() const override;
  bool wantsHardwareKeys() const override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  enum class Phase { Dialling, Ringing, Connected, Failed, None };
  Phase phase() const;

  // Where the volume buttons were drawn last; empty when there are none. A tap is matched against the picture
  // the child saw.
  Rect quieter_, louder_;
};

}  // namespace ui
