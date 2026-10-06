#pragma once

// The status bar at the top of every screen and the alert bar under it.
//
// The status bar is built so that it changes rarely, because every change of the picture is a
// redraw of the e-ink panel: it shows the date, not the time (the clock page shows the time);
// the battery as five bars, not a percentage; and the network as a name or a word, not as
// signal bars. What a parent needs to know exactly is on the status page.
//
// Left: the most important of: the SIM problem, school time, "SMS only", the network name (a
// general word when the network has none).
// "GPS" follows whenever the position is being determined (a device that locates in secret is
// what this one must not be). Right: the date and the battery.
//
// The words are fixed catalog texts and are never shortened; only the operator's name, which
// comes from the network, is (documented in drawStatusBar). If the weekday does not fit next
// to the left side, the weekday goes first: it is the most dispensable part.

#include <string>

#include "i18n/Translator.h"
#include "ui/Display.h"

namespace ui {

struct StatusModel {
  enum class Sim { Ready, Missing, PinRequired, Locked };
  // None: no network at all. Operator: connected to a mobile network named operatorName.
  // Radio: the long-range radio is the way out; RadioOnly: the radio only, to save power.
  enum class Link { None, Operator, Radio, RadioOnly };
  // What the network allows beyond messages. NotReady is a network that is there while the IMS is
  // not: a state that usually passes, which "SMS only" (a restriction) does not.
  enum class Voice { Ready, SmsOnly, NotReady };

  Sim sim = Sim::Ready;
  Link link = Link::None;
  std::string operatorName;
  bool quietHours = false;       // school time
  Voice voice = Voice::Ready;    // connected, but only SMS works, or telephony is not ready
  bool locating = false;         // the position is being determined
  int batteryPercent = -1;       // -1 = unknown

  bool dateKnown = false;
  int month = 0, day = 0;        // 1..12, 1..31
  int weekday = 0;               // 0 = Sunday .. 6 = Saturday
};

// Battery as 0..5 bars, rounded up so that 1 % still shows a bar: an empty frame while there
// is enough left for an emergency would be the wrong information. -1 (unknown) gives 0.
int batteryBars(int percent);

// The date as the catalog writes it ("Tue 09/30", "Di 30.09."), with or without the weekday;
// the "unknown" form when the model has no date.
std::string dateText(const StatusModel& model, i18n::Translator& t, bool withWeekday);

// What the left side says, and whether it is the operator's name (the one text that may be
// shortened).
struct StatusLeft {
  std::string text;
  bool isOperatorName = false;
};
StatusLeft statusLeft(const StatusModel& model, i18n::Translator& t);

// Draws the bar in `area` (kStatusBarHeight high): the texts, the battery, and the rule under
// them.
void drawStatusBar(Display& display, const Rect& area, const StatusModel& model, i18n::Translator& t);

// Draws the alert bar: inverted with `text` when there is one, and nothing when there is not.
// The area is reserved either way (see Layout.h). The text is shortened if it is too long: an
// alert may carry a name, and what matters is also in the message history.
void drawAlertBar(Display& display, const Rect& area, const std::string& text);

// The width the alert bar leaves for its text: the bar minus the margin it keeps on each side.
constexpr int kAlertBarMargin = 6;

}  // namespace ui
