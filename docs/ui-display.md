# Display and frame buffer

Code: `src/ui/Display.h`, `src/ui/Crc32.h`, `testing/FrameBuffer.h`, `testing/Golden.h`; tests in `test/test_frame_buffer/`. Pure C++17; the layer check keeps `src/ui/` free of application and hardware includes.

## The panel

240 × 320 pixels, one bit per pixel (black or white), origin top-left; the e-ink panel cannot do more, so there is no grey and no colour anywhere in the interface. The constants are `ui::kScreenWidth` and `ui::kScreenHeight`; `ui::Rect` is `x, y, w, h` with `right()` and `bottom()` as the first column and row *outside*.

## `ui::Display`

The interface everything draws on: `width()`, `height()`, `beginFrame()`, `endFrame()`, `setPixel`, `fillRect`, `outlineRect` (the one-pixel outline inside the rectangle) and `drawBitmap`.

- **The caller clips.** The display does not scale, wrap or shorten; a pixel outside the screen is dropped.
- **Bitmaps** are packed in rows that each start at a byte boundary (`(w + 7) / 8` bytes per row), the leftmost pixel in the top bit. Only the **set** bits are drawn, in the given colour; clear bits leave the background alone, so a glyph can be drawn on anything.
- A frame is drawn between `beginFrame()` and `endFrame()`; a driver may hold the image back and decide at `endFrame()` how to refresh the panel.
- `ui::crc32()` is the checksum the refresh logic compares: an unchanged picture costs no refresh.

## `FrameBuffer`

An in-memory implementation for tests and the later simulator. Beyond the pixels it keeps the layout honest.

**Clip detection.** A widget declares the rectangle it owns with `beginRegion(rect)` / `endRegion()`. Every drawing call that touches a pixel outside it is recorded as a `ClipEvent` (the call, the region, how many pixels fell outside, how many off the screen, the first offender), and so is every pixel off the screen, region or not. A widget test asserts `!fb.clipped()` and prints `describeClips()` on failure. Details that matter:

- Only pixels a call would **change** are touched: a bitmap's clear bits are not, a fill touches every pixel of its rectangle, white or black.
- Regions nest; the innermost is the rule. A region that reaches beyond the screen does not make the off-screen part acceptable. An empty region clips everything.
- Without a region only the screen edge is checked.

**ASCII art.** `toAscii()` (or `toAscii(rect)`) writes lines of `#` (black) and `.` (white) with a line break after each; `fromAscii()` reads it back and refuses uneven lines and foreign characters. A rectangle partly off the screen reads white there.

**CRC.** `crc()` covers the packed content and the size, so a 240 × 320 and a 320 × 240 buffer with the same bits differ. `frames()` / `endedFrames()` count `beginFrame` / `endFrame`.

## Golden files

`golden::expect(fb, "name", region)` compares the ASCII art of a rectangle with a file named after it under `test/golden/`. The files are small (only the region the widget owns, not the screen) and reviewed like code: a change in how something looks is a change of a file in the diff of the pull request.

- With the environment variable **`UPDATE_GOLDEN`** set (to anything) `expect()` writes the file and passes. Review the diff before committing.
- A **missing file fails** and says how to create it; it is never created silently.
- A mismatch prints the rows that differ, expected above actual, at most six, and the count of the rest, instead of two screens. A difference in size is stated.
- The comparison is exact: one pixel is a failure.

`golden::compare()` does the work without failing the test, with the directory and the update flag as arguments; the tests of the helper use it on a scratch directory.
