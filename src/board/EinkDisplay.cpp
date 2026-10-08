#include "EinkDisplay.h"

#include <string>

#include "../app/Crc32.h"
#include "Uc8253.h"

namespace board {

bool EinkDisplay::begin() {
  configured_ = false;
  const PanelProfile* p = profile_;
  if (!p || p->width != ui::kScreenWidth || p->height != ui::kScreenHeight || p->start.empty() || !p->encodeWindow ||
      p->busyTimeoutMs == 0) {
    log_.log("Display: not configured, the panel is not driven (the panel's values are not in this build)");
    return false;
  }
  wire_.begin();
  configured_ = true;
  log_.log(std::string("Display: profile ") + p->name);
  return true;
}

void EinkDisplay::send(const std::vector<PanelCommand>& commands) {
  const PanelProfile& p = *profile_;
  for (const PanelCommand& c : commands) {
    wire_.command(c.command);
    wire_.data(c.data);
    if (c.waitBusy && !wire_.waitIdle(p.busyTimeoutMs, p.busyIsHigh)) {
      hung_ = true;
      log_.log("Display: the panel stayed busy after a command; the next frame is a full refresh");
    }
    if (c.delayAfterMs) wire_.pause(c.delayAfterMs);
  }
}

void EinkDisplay::wake() {
  const PanelProfile& p = *profile_;
  wire_.reset(p.resetLowMs, p.resetHighMs);
  send(p.start);
}

// The rectangle grown to at least the profile's smallest window, kept inside the screen and on a byte border.
static ui::Rect grow(ui::Rect r, const PanelProfile& p) {
  auto fit = [](int& pos, int& len, int least, int limit) {
    if (len >= least) return;
    pos -= (least - len) / 2;
    len = least;
    if (pos + len > limit) pos = limit - len;
    if (pos < 0) pos = 0;
  };
  fit(r.x, r.w, p.minWindowWidth, ui::kScreenWidth);
  fit(r.y, r.h, p.minWindowHeight, ui::kScreenHeight);
  r.x -= r.x % 8;
  r.w = (r.w + 7) / 8 * 8;
  return r;
}

void EinkDisplay::finish() {
  const PanelProfile& p = *profile_;
  wire_.command(uc8253::kDisplayRefresh);
  wire_.pause(p.refreshSettleMs);
  if (!wire_.waitIdle(p.busyTimeoutMs, p.busyIsHigh)) {
    hung_ = true;
    log_.log("Display: the panel stayed busy after a refresh; the next frame is a full refresh");
  }
  send(p.sleep);
  shown_ = drawn_;
}

void EinkDisplay::endFrame() {
  if (!configured_) return;
  hung_ = false;
  controller_.present(checksum::crc32(drawn_.data(), drawn_.size()), millis_.millis32());
  if (hung_) controller_.invalidate();
}

void EinkDisplay::refreshFull() {
  const PanelProfile& p = *profile_;
  wake();
  send(p.beforeFull);
  const ui::Rect all = {0, 0, ui::kScreenWidth, ui::kScreenHeight};
  wire_.command(uc8253::kDataTransmission1);
  wire_.data(shown_.window(all, p.blackIsZero));
  wire_.command(uc8253::kDataTransmission2);
  wire_.data(drawn_.window(all, p.blackIsZero));
  finish();
}

void EinkDisplay::refreshPartial() {
  const PanelProfile& p = *profile_;
  ui::Rect r;
  if (!drawn_.differences(shown_, r)) return;
  r = grow(r, p);
  const std::vector<uint8_t> window = p.encodeWindow(r);
  if (window.empty()) {  // a window the panel cannot address: the whole picture instead
    refreshFull();
    return;
  }
  wake();
  send(p.beforePartial);
  wire_.command(uc8253::kPartialIn);
  wire_.command(uc8253::kPartialWindow);
  wire_.data(window);
  wire_.command(uc8253::kDataTransmission1);
  wire_.data(shown_.window(r, p.blackIsZero));
  wire_.command(uc8253::kDataTransmission2);
  wire_.data(drawn_.window(r, p.blackIsZero));
  finish();
}

}  // namespace board
