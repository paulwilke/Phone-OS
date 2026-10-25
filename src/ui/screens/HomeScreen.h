#pragma once

// The home screen: a grid of tiles, three columns, two pages. The first page is the child's —
// phone, text, SOS, inbox, radio, light, power —, the second is the device's: network, GPS, clock,
// settings, Wi-Fi, motion, tests, status. What a child needs every day is in front, what is looked
// at once a month is behind, and the dots under the tiles say there is a second page.
//
// Which tiles exist follows the profile and the device, and never anything the child can change:
//   * phone and text need somebody to call or write to (a contact, or a call target);
//   * SOS needs somebody to reach (`Profile::sosRecipient`): a device with nobody to send to has
//     no SOS tile, and it is never left out otherwise;
//   * the radio tile needs the profile to allow the radio;
//   * Wi-Fi and status are the parents' tools (`parentTools`), and Wi-Fi also needs a Wi-Fi to
//     switch; the motion tile needs a motion sensor (`ScreenContext::motionSensor`);
//   * every tile of the second page can be switched off by the parents (`[home]` in settings.toml,
//     `Profile::home`); with all of them off there is no second page. Without the Wi-Fi tile the
//     parents' page is still reached over the USB cable (scripts/webbridge.py).
//
// The tile order is fixed: the profile has the numbers the call actions reach, not an order of
// tiles. The words are short because a column is 74 pixels wide (68 inside the tile): a label of
// a shipped language that is wider is a layout bug, found by the layout tests. The pseudo-language
// is longer than any real one on purpose and is exempt from that one rule.
//
// The soft keys are empty: the tiles are the buttons, the dots turn the page, and the keys Up and
// Down still move the selection (owner, 2026-10-08: the arrows only took room). A bar that named
// the action of the tile under the selection would change with every step and cost a redraw.
// Back locks the device: without it the child could not lock it before it goes into the pocket.
//
// School time: the phone tile does not open the phone page; it says "School time until 15:00" and
// stays on the home screen. Everything else opens as usual, SOS with its warning.

#include <vector>

#include "ui/AppScreen.h"
#include "ui/TileGrid.h"

namespace ui {

class HomeScreen : public AppScreen {
public:
  explicit HomeScreen(ScreenContext context);

  enum class Kind {
    Phone, Text, Sos, Inbox, Radio,                                          // page one
    Network, Gps, Clock, Settings, Wifi, Motion, Tests, Status, Light, Power,   // Light and Power are on page one
  };

  // The tiles that exist now, by page, in the order they are drawn.
  std::vector<std::vector<Kind>> pages() const;
  // The tile the selection is on.
  Kind selected();
  size_t page() { rebuild(); return grid_.page(); }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  void rebuild();
  void activate(Kind kind);
  Tile tileOf(Kind kind);

  TileGrid grid_{3, 3};
  std::vector<Kind> order_;  // the tiles of all pages in the order of the grid
};

}  // namespace ui
