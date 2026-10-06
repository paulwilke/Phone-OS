# The motion page

Code: `src/ui/screens/MotionScreen.{h,cpp}` and `src/ui/MotionSource.h`; tests in `test/test_screen_motion/`. The acceleration sensor to look at: a spirit level, and how often the device was shaken.

**The sensor runs only while the page is open.** It is a small computer of its own and costs current; starting it takes about 11 seconds and blocks. So the page first draws "Sensor is starting ..." and only on the next `onTick` (the stack calls it for the visible screen, see [ui-framework.md](ui-framework.md)), when that frame is on the panel, starts it. `onLeave` stops it by every way out of the page (back, home, lock; proven with a real stack). A sensor that does not answer is told ("The sensor does not answer.") and not asked again. Until the first value arrives the page still says it is starting.

**What the page knows of the sensor** is `ui::MotionSource` (available, start, stop, running, read): the screen may not include the hardware interface, so the application hands it an adapter: `boot::SensorMotionSource` (`src/boot/`, tested in `test_runtime`, also without a sensor), and the page tests use `testing/MotionSourceOf.h` over the test sensor. A sample is in g and in the sensor's own axes.

**The ball.** It moves in steps of a field of 0.15 g, seven fields from side to side (about 25 degrees at the edge), clamped at three; flat on a table it sits in the middle, and a tremble inside a field changes nothing in the picture (a changed picture costs the e-ink panel a refresh). The magnitude of the sample is shown in g with one decimal, the shakes counted by `ShakeDetector` under it. Enter sets the counter to zero. Every sample counts for the shakes, however seldom the picture is drawn.

**The mapping from the sensor to the screen.** The sensor is turned 90 degrees against the screen; with the sample as the device reports it the ball sits at `-cell(y)` across (positive = right) and `-cell(x)` up (positive = up). The tests pin it with the values checked on the device on 2026-10-03:

| Sample (g) | The device is tilted | The ball |
|---|---|---|
| x 0, y +0.45 | the left side low | three fields left |
| x 0, y −0.45 | the right side low | three fields right |
| x +0.30, y 0 | the lower edge low (towards the user) | two fields down |
| x −0.30, y 0 | the upper edge low | two fields up |
| flat | — | the middle |

The ball rolls to the lower side. A picture test checks that the drawn ball moves the same way. The first mapping on the device rolled the ball up when the device was tilted left; this one is the correction.

**Open.** The capability description says a device lying flat reads about +1 on z; the sensor library on the T-Deck Max reports about −0.95 (measured on the device). The mapping is verified with the device's own values: whatever hands samples to this page must deliver those, or every direction flips.

**Layout.** Every state is drawn inside the screen in English, German, French and the pseudo-language (40 % longer than any real one); only the one action word beside the arrows may reach into the soft-key bar in the pseudo-language.

## Tests

`test_screen_motion`: no sensor; the start after the first frame and only once; a sensor that does not answer; waiting for the first value; leaving stops it (directly, by back, home and lock); flat in the middle; the four pinned directions; the field size and the clamp; the drawn ball moving the way the mapping says; a tremble not changing the picture; shakes counted and reset; every sample counted without a frame; a slow tilt being no shake; the strength in g; Enter before a value; the words in German; layout in four languages in four phases; pictures in English and German.
