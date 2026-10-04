#pragma once

// How the e-ink panel is refreshed after the picture was drawn. A *partial* refresh is fast
// and does not flash, but leaves ghosts of earlier pictures behind; a *full* refresh flashes
// and clears them. The policy decides which one a frame gets, and that nothing at all is the
// right answer for a frame that looks like the one on the panel.
//
//   * An unchanged picture is never refreshed: it costs time, current and flicker for nothing.
//   * A changed picture gets a partial refresh, and every `fullEvery`-th change a full one,
//     so that ghosts do not pile up.
//   * A picture that stays unchanged for `maxStaticMs` gets a full refresh anyway: a panel
//     that shows the same image for hours loses contrast, and the refresh restores it. After
//     that the next one is due after another `maxStaticMs`.
//
// Time is a free-running millisecond counter; wrapping around is handled.

#include <cstdint>

namespace ui {

struct RefreshConfig {
  uint16_t fullEvery = 10;                          // changes per full refresh; 0 or 1 = every change is full
  uint32_t maxStaticMs = 4UL * 3600UL * 1000UL;     // 0 = never refresh a static picture
};

class RefreshPolicy {
public:
  enum class Action : uint8_t { None, Partial, Full };
  using Config = RefreshConfig;

  // `lastFullMs` is when the panel was last refreshed in full (the driver's start-up clear).
  explicit RefreshPolicy(Config config = Config(), uint32_t lastFullMs = 0)
      : config_(config), lastFullMs_(lastFullMs) {}

  // What to do with the frame that was just drawn.
  Action decide(bool changed, uint32_t nowMs);

  // A full refresh that happened for another reason (the driver cleared the panel).
  void noteFull(uint32_t nowMs) { full(nowMs); }

  uint16_t partialsSinceFull() const { return partials_; }

private:
  Action full(uint32_t nowMs) {
    partials_ = 0;
    lastFullMs_ = nowMs;
    return Action::Full;
  }

  Config config_;
  uint16_t partials_ = 0;
  uint32_t lastFullMs_;
};

}  // namespace ui
