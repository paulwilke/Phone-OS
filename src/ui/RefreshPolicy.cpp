#include "ui/RefreshPolicy.h"

namespace ui {

RefreshPolicy::Action RefreshPolicy::decide(bool changed, uint32_t nowMs) {
  if (changed) {
    // A count beyond the limit (fullEvery 0 or 1) means every change is a full refresh.
    if (++partials_ >= config_.fullEvery) return full(nowMs);
    return Action::Partial;
  }
  if (config_.maxStaticMs != 0 && static_cast<uint32_t>(nowMs - lastFullMs_) >= config_.maxStaticMs) return full(nowMs);
  return Action::None;
}

}  // namespace ui
