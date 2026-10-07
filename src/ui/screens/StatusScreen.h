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
// **Restarting** is written below it and works the same way (r twice, any other key takes the first back),
// but also with the cable plugged in. It logs one line and asks the core (`requestRestart`); the host restarts.
//
// What the firmware and the hardware are called is the application's to say (build constants), so it
// hands them to the page, together with the problems found in reading the configuration: the page shows
// their number and the first one.
//
// It is a text and scrolls with the arrows when a language makes it longer than the page.

#include <string>
#include <vector>

#include "ui/AppScreen.h"
#include "ui/TextBlock.h"

namespace ui {

// A problem found while reading the configuration, as the page needs it: what kind of thing it was and the
// names to put in the sentence. The loader's own report (src/config/) is not the page's to know; the
// application turns it into this, so the page words it from the catalogs.
struct ConfigProblem {
  enum class Kind {
    File,     // a file could not be read (missing, too large, not valid)
    Setting,  // a key in a file was not understood or out of range
    Entry,    // an entry of a list (a contact, a school-time window, a tile) was not used
  };
  Kind kind = Kind::Entry;
  std::string file;  // Kind::File and Kind::Setting
  std::string key;   // Kind::Setting
};

struct DeviceInfo {
  std::string firmware;
  std::string hardware;  // empty: no line
  std::vector<ConfigProblem> problems;  // from the start; none: no line
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
  bool restartArmed_ = false;  // like offArmed_: the first press of r is taken back by any other input
};

}  // namespace ui
