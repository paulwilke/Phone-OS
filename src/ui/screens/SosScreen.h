#pragma once

// The emergency: the choice, the countdown, in school time the five taps, and then what the ping is
// doing and how it ended. The page follows the core's emergency state ([emergency.md](emergency.md));
// it decides nothing about who is reached or when the text goes out.
//
//   choice      "SOS", "Emergency in 40 s", and three large fields: 1 Emergency, 2 Come and get me,
//               3 Cancel. Pressing SOS already started the search; nobody has to choose, and when the
//               countdown runs out the emergency goes by itself. The keys 1, 2 and 3 choose at once.
//   school time "School time until 15:00. The device is locked." and a large button with the number
//               of taps still needed (five, each within ten seconds of the last: a pocket taps now and
//               then and must not add up to an alarm). The last tap sends the emergency and lifts the
//               lock for the rest of that school time. There is no automatic emergency here, and no
//               pick-up: in school time the press must be deliberate.
//   ping        three bars (searching, position found, sent), what is going on in words — how long it has
//               searched, how accurate the position is — and, once sent, how accurate it was and to whom
//               it went, "waits for network" when it is still queued. The text of the message is not
//               shown: its link is longer than the screen and says nothing to a child.
//   not sent    "Not sent. No network and no radio. Try again." with an action that starts again.
//
// **Leaving the page releases the search** (`leaveSos()`, called from `onLeave()`, so every way out —
// back, the lock, the home key — does it): nothing is sent and the receiver is let go. A ping
// that is already running is not stopped by leaving (the core's rule: a text that has been decided
// on must not be lost by a tap on Back), so the soft key says "Back" and not "Cancel" there.
//
// **A call that rings is no way out** (decision D-39): the call page covers this one, the child can
// answer, and the choice keeps its countdown, so the emergency goes out by itself during the call
// when nobody chose. When the call is over the stack comes back here, to the countdown or to what
// the ping did. Only the ringing call is let through: the stack brings the call page up exactly then.
//
// The page keeps the device from locking while an emergency is open or running, because locking
// would leave the page and cancel the choice.

#include "ui/AppScreen.h"

namespace ui {

class SosScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override { selection_ = 0; }
  void onLeave() override {
    if (app().callState() != ModemState::RingingIn) app().leaveSos();
  }
  bool preventsAutoLock() const override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  enum class View { Choice, Quiet, Progress, Idle };
  View view() const;
  bool choose(size_t index);
  void drawChoice(Display& display, const Rect& area, int top);
  void drawQuiet(Display& display, const Rect& area, int top);
  void drawProgress(Display& display, const Rect& area, int top);
  Rect choiceRect(const Rect& area, size_t index);
  std::string quietWords() ;
  std::string quietHint();
  Rect quietButton(const Rect& area);
  int titleBottom(const Rect& area) const;

  size_t selection_ = 0;
};

}  // namespace ui
