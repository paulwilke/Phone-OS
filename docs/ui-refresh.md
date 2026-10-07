# Refreshing the e-ink panel, and frame budgets

An e-ink panel shows a picture without power, and costs time, current and a visible flash every time it is changed. The interface therefore tries to change the panel as rarely as it can, and the tests state how rarely.

Code: `src/ui/RefreshPolicy.h`, `FrameTracker.h`, `RefreshController.h`; test helpers `testing/RefreshRig.h`; tests in `test/test_refresh/` and `test/test_frame_budget/`. The panel calls `RefreshController::present()` at the end of each frame: the simulator's panel ([simulated-panel.md](simulated-panel.md)) and the device's ([device-display.md](device-display.md)).

## Two kinds of refresh

A **partial** refresh is quick and does not flash, but leaves faint ghosts of earlier pictures behind. A **full** refresh flashes and clears them.

## The policy

`RefreshPolicy::decide(changed, nowMs)` answers `None`, `Partial` or `Full` for the frame that was just drawn:

- **An unchanged picture is never refreshed.**
- **A changed picture** gets a partial refresh, and every `fullEvery`-th change (default 10) a full one, so ghosts do not pile up. `fullEvery` of 0 or 1 makes every change full.
- **A static picture** is refreshed in full after `maxStaticMs` (default 4 hours): a panel that shows one image for hours loses contrast. The next one is due after another `maxStaticMs`; 0 turns this off. A full refresh caused by changes moves the deadline; a partial one does not.
- Time is a free-running millisecond counter; the wrap-around of the counter is handled.
- `noteFull()` tells the policy about a full refresh that happened for another reason.

## Tracker and controller

`FrameTracker` keeps the CRC-32 of the picture on the panel (`ui::crc32` over the packed picture, the same function `FrameBuffer::crc()` uses) and says whether a new frame differs. The first frame always does: nothing is known about the panel at start-up.

`RefreshController` is what the driver uses: `present(crc, nowMs)` runs the tracker and the policy and calls `Panel::refreshPartial()` or `Panel::refreshFull()` (or neither). **The first frame is always a full refresh**, and so is the first after `invalidate()` (the panel was powered off and its content is unknown), changed or not. `stats()` counts frames, skipped frames, partial and full refreshes.

## Frame budgets

A *frame budget* states how often the panel may be refreshed in a scenario. The point is not the policy, which skips unchanged frames by construction, but the interface: a screen that draws something different every second (a clock with seconds, a blinking cursor, a percentage that ticks) makes a frame "changed" for nothing. A budget test redraws a screen the way the application will, feeds the pictures to a `RefreshController` through `RefreshRig`, and fails — with a list of the refreshes, frame and time — when the panel works more than the scenario allows.

```cpp
RefreshRig rig;
rig.frame(0, [&](ui::Display& d) { home.draw(d); });     // boot: one full refresh
rig.mark();                                              // count from here
for (uint32_t s = 1; s <= 3600; ++s) rig.frame(s * 1000, [&](ui::Display& d) { home.draw(d); });
rig.expect("an idle hour", /*partial*/ 0, /*full*/ 0);
```

The scenarios in `test/test_frame_budget/`:

| Scenario | Budget |
|---|---|
| an idle hour, redrawn every second, battery draining inside one bar, network polled | no refresh |
| nine idle hours | one full refresh at 4 h and one at 8 h |
| nine moves of the tile selection, then a tenth | nine partial, the tenth full |
| a key that moves nothing (one tile, first or last row of a list, a text that does not scroll) | no refresh |
| battery inside a bar / across a bar; the GPS note on and off; a new day; the network polled again | none / one / two / one / none |
| an alert shown for five seconds | two (appears, goes) |
| open a screen, go back, then look for a minute, then lock and wait three hours | one per screen change; none for the minute; the lock screen is one change and then nothing |
| the same state drawn twice, in every language | the same picture |

The budgets caught what they are for: with a pixel in the status bar that depends on the battery percentage, the idle-hour scenario fails with the history of refreshes, and with a list that wraps at its first row, the "up on the first row" scenario does.

## What the status bar does for the budget

It shows the date, not the time; the battery as five bars, not a percentage; the network as a name or a word, not as signal bars; and nothing that changes without a reason a parent would care about ([ui-framework.md](ui-framework.md)). Each of those is a decision for the budget: a clock with minutes would be 1,440 refreshes a day, a percentage up to a hundred.
