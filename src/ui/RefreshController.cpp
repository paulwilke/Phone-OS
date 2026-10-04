#include "ui/RefreshController.h"

namespace ui {

RefreshPolicy::Action RefreshController::present(uint32_t crc, uint32_t nowMs) {
  ++stats_.frames;
  const bool changed = tracker_.update(crc);
  RefreshPolicy::Action action = policy_.decide(changed, nowMs);
  if (forceFull_) {
    forceFull_ = false;
    policy_.noteFull(nowMs);
    action = RefreshPolicy::Action::Full;
  }
  switch (action) {
    case RefreshPolicy::Action::None: ++stats_.skipped; break;
    case RefreshPolicy::Action::Partial: ++stats_.partial; panel_.refreshPartial(); break;
    case RefreshPolicy::Action::Full: ++stats_.full; panel_.refreshFull(); break;
  }
  return action;
}

void RefreshController::invalidate() {
  forceFull_ = true;
}

}  // namespace ui
