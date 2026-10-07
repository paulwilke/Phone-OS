#pragma once

// The e-ink panel as a ui::Display: the picture is drawn into memory, and at the end of the frame the real
// RefreshController (docs/ui-refresh.md) decides whether the panel is refreshed at all, partially or in full;
// this class carries the decision out through the controller's documented commands (board/Uc8253.h) with the
// panel's own data (board/PanelProfile.h).
//
// **Without a profile the panel is not driven**: begin() says "the display is not configured" and every frame
// is dropped. The values a profile holds are not in this repository, because they could not be taken from
// the panel's datasheet and the vendor's demo, and a library's tables are not used (docs/device-display.md).
//
//   * a full refresh sends the old picture to the first data register and the new one to the second, both
//     whole, after the profile's full-refresh commands;
//   * a partial refresh sends only the rectangle that differs from what the panel shows, widened to whole
//     bytes, to both registers, inside a partial window, after the profile's partial-refresh commands;
//   * after each refresh the panel is put to sleep with the profile's sleep commands;
//   * a panel that stays busy longer than the profile allows is said in the log and the next frame is a full
//     refresh, because what it shows is no longer known.

#include "../hal/EventLog.h"
#include "../ui/Display.h"
#include "../ui/RefreshController.h"
#include "Bitmap1.h"
#include "EpdWire.h"
#include "MillisSource.h"
#include "PanelProfile.h"

namespace board {

class EinkDisplay : public ui::Display, private ui::Panel {
public:
  EinkDisplay(EpdWire& wire, const PanelProfile* profile, MillisSource& millis, EventLog& log,
              ui::RefreshConfig config = ui::RefreshConfig())
      : wire_(wire), profile_(profile), millis_(millis), log_(log), drawn_(ui::kScreenWidth, ui::kScreenHeight),
        shown_(ui::kScreenWidth, ui::kScreenHeight), controller_(*this, config, millis.millis32()) {}

  // Resets the controller and sends the start commands. False, and nothing sent, without a usable profile.
  bool begin();
  bool configured() const { return configured_; }
  const ui::RefreshController::Stats& stats() const { return controller_.stats(); }

  int width() const override { return drawn_.width(); }
  int height() const override { return drawn_.height(); }
  void beginFrame() override { drawn_.clear(false); }
  void endFrame() override;
  void setPixel(int x, int y, bool black) override { drawn_.setPixel(x, y, black); }
  void fillRect(const ui::Rect& r, bool black) override { drawn_.fillRect(r, black); }
  void outlineRect(const ui::Rect& r, bool black) override { drawn_.outlineRect(r, black); }
  void drawBitmap(int x, int y, const uint8_t* bits, int w, int h, bool black) override {
    drawn_.drawBitmap(x, y, bits, w, h, black);
  }

private:
  void refreshPartial() override;
  void refreshFull() override;
  void send(const std::vector<PanelCommand>& commands);
  void finish();

  EpdWire& wire_;
  const PanelProfile* profile_;
  MillisSource& millis_;
  EventLog& log_;
  Bitmap1 drawn_;
  Bitmap1 shown_;
  ui::RefreshController controller_;
  bool configured_ = false;
  bool hung_ = false;
};

}  // namespace board
