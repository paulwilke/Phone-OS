#include "ui/ScreenStack.h"

namespace ui {

Screen* ScreenStack::screen(ScreenId id) const {
  auto it = screens_.find(id);
  return it == screens_.end() ? nullptr : it->second;
}

template <class Change>
void ScreenStack::transition(Change&& change) {
  if (transitioning_ || !begun_) return;
  const ScreenId before = visibleId();
  change();
  const ScreenId after = visibleId();
  if (before == after) return;
  transitioning_ = true;
  if (Screen* s = screen(before)) s->onLeave();
  if (Screen* s = screen(after)) s->onEnter();
  transitioning_ = false;
}

void ScreenStack::begin(uint32_t nowSeconds) {
  if (begun_) {
    transitioning_ = true;
    if (Screen* s = screen(visibleId())) s->onLeave();
    transitioning_ = false;
  }
  depth_ = 1;
  path_[0] = root_;
  locked_ = false;
  lastInput_ = nowSeconds;
  begun_ = true;
  transitioning_ = true;
  if (Screen* s = screen(root_)) s->onEnter();
  transitioning_ = false;
}

void ScreenStack::push(ScreenId id) {
  for (size_t i = 0; i < depth_; ++i) {
    if (path_[i] == id) {  // already on the path: go back to it
      depth_ = i + 1;
      return;
    }
  }
  if (depth_ < kMaxDepth) path_[depth_++] = id;
  else path_[depth_ - 1] = id;
}

void ScreenStack::open(ScreenId id) {
  if (locked_ || !screen(id)) return;
  if (id != root_ && filter_ && !filter_(id)) return;
  transition([&] { push(id); });
}

void ScreenStack::back() {
  if (locked_) return;
  transition([&] {
    if (depth_ > 1) --depth_;
  });
}

void ScreenStack::home() {
  if (locked_) return;
  transition([&] { depth_ = 1; });
}

void ScreenStack::lock() {
  if (locked_ || !hasLock_ || !screen(lock_)) return;
  transition([&] { locked_ = true; });
}

void ScreenStack::unlock() {
  if (!locked_) return;
  transition([&] {
    locked_ = false;
    depth_ = 1;
  });
}

void ScreenStack::interrupt(ScreenId id, uint32_t nowSeconds) {
  if (!screen(id)) return;
  lastInput_ = nowSeconds;
  transition([&] {
    if (visibleId() != id) relockAfterInterrupt_ = locked_;  // the same interrupt again keeps what the first found
    locked_ = false;
    push(id);
  });
}

void ScreenStack::endInterrupt(ScreenId id) {
  const bool relock = relockAfterInterrupt_;
  relockAfterInterrupt_ = false;
  if (locked_ || path_[depth_ - 1] != id) return;
  // One transition: the screen under the interrupt is not entered on the way back to the lock.
  transition([&] {
    if (depth_ > 1) --depth_;
    if (relock && hasLock_ && screen(lock_)) locked_ = true;
  });
}

bool ScreenStack::handle(const Input& input, uint32_t nowSeconds) {
  if (!begun_ || transitioning_) return false;
  lastInput_ = nowSeconds;
  Screen* shown = screen(visibleId());
  if (!shown) return false;
  if (!locked_ && input.isHardwareKey() && !shown->wantsHardwareKeys()) {
    auto action = actions_.find(input.key);
    if (action != actions_.end()) {
      action->second(*this);  // each navigation call it makes is a transition of its own
      return true;
    }
  }
  return shown->handle(input);
}

void ScreenStack::render(Display& display) {
  if (Screen* s = screen(visibleId())) s->render(display);
}

void ScreenStack::poll(uint32_t nowSeconds) {
  if (!begun_) return;
  Screen* shown = screen(visibleId());
  if (shown) shown->onTick();
  if (locked_ || autoLockSeconds_ == 0) return;
  shown = screen(visibleId());   // a tick may have changed what is visible
  if (shown && shown->preventsAutoLock()) return;
  if (nowSeconds - lastInput_ >= autoLockSeconds_) lock();
}

}  // namespace ui
