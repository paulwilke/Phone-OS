#include "EinkDisplay.h"

#include "../ui/Crc32.h"
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
  wire_.reset(p->resetLowMs, p->resetHighMs);
  send(p->start);
  configured_ = true;
  return true;
}

void EinkDisplay::send(const std::vector<PanelCommand>& commands) {
  for (const PanelCommand& c : commands) {
    wire_.command(c.command);
    wire_.data(c.data);
  }
}

void EinkDisplay::finish() {
  const PanelProfile& p = *profile_;
  wire_.command(uc8253::kDisplayRefresh);
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
  controller_.present(ui::crc32(drawn_.data(), drawn_.size()), millis_.millis32());
  if (hung_) controller_.invalidate();
}

void EinkDisplay::refreshFull() {
  const PanelProfile& p = *profile_;
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
  const std::vector<uint8_t> window = p.encodeWindow(r);
  if (window.empty()) {  // a window the panel cannot address: the whole picture instead
    refreshFull();
    return;
  }
  send(p.beforePartial);
  wire_.command(uc8253::kPartialIn);
  wire_.command(uc8253::kPartialWindow);
  wire_.data(window);
  wire_.command(uc8253::kDataTransmission1);
  wire_.data(shown_.window(r, p.blackIsZero));
  wire_.command(uc8253::kDataTransmission2);
  wire_.data(drawn_.window(r, p.blackIsZero));
  finish();
  wire_.command(uc8253::kPartialOut);
}

}  // namespace board
