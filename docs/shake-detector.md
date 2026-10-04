# Shake detector

Code: `src/app/ShakeDetector.h`, tests in `test/test_shake_detector/`. Header only, pure C++17.

`ShakeDetector::feed(ax, ay, az)` takes one acceleration sample in g and returns true when it begins a new shake.

- A shake is a burst with a magnitude of at least `highG` (default 1.8 g). Each burst counts **once**, on its rising edge, however many samples it lasts.
- The burst is over when the magnitude falls to `lowG` (default 1.3 g) or below; only then can the next burst count. This hysteresis stops a shake from counting again while it fades.
- The magnitude is the length of the vector, so the direction of the shake and the orientation of the device do not matter. A device lying still or tilted slowly stays near 1 g and never counts.
- Both thresholds are inclusive. The comparison is on the squared magnitude, so there is no square root and a sample exactly on a threshold behaves the same everywhere.
- A `lowG` above `highG` is lowered to `highG`. Otherwise a long burst would end after every sample and count again and again.
- A sample without a value (NaN, from a failed read) is neither a shake nor the end of one.

`count()` is the number of shakes since construction or `reset()`; `reset()` also forgets a burst that was under way.

The detector has no clock and no opinion on what a number of shakes means. Whoever feeds it decides how many shakes in how much time are intended.
