# The simulated e-ink panel and the PNG writer

Code: `sim/SimPanel.h`, `sim/Png.h` (header-only, on the include path of the host tests as `-I sim`); tests in `test/test_sim_panel/`. The simulator is a platform: it may include `src/` and `testing/`, and `src/` never includes it ([hardware-capabilities.md](hardware-capabilities.md)).

## `SimPanel`

A `ui::Display` that does what the firmware's e-ink driver will: at the end of every frame the picture goes through the real `RefreshController` ([ui-refresh.md](ui-refresh.md)), so the panel is refreshed partially, in full or not at all exactly as the firmware decides. It draws on a `FrameBuffer` (the same one the layout tests use, so no second drawing code exists) and keeps a second one for what the panel shows.

- `shown()` is the picture of the **last refresh**, not of the last frame: a frame that costs no refresh changes nothing on the panel. It is blank until the first refresh, which is always full.
- `previous()` is what it showed before the last refresh; a window keeps it on show while `busy()`.
- `refreshes()` lists every refresh with the frame that caused it, the time and whether it was full; `stats()` counts frames, skipped frames, partial and full refreshes.
- `busy(nowMs)` says whether the last refresh is still running: `kPartialMs` (300) and `kFullMs` (1200) after it started. They are round figures that make the delay visible to the person looking, not measurements. A window uses it to show the flash.
- `png()` and `writePng(path)` give the shown picture as a PNG; `writePng` reports a file it could not write.

The simulator's millisecond counter is passed as a function when the panel is built, and asked at the end of every frame.

**Not simulated:** ghosting (the faint traces of earlier pictures), the true duration of a refresh, and the effect of temperature. A picture that looks right here may ghost on the real panel.

## `encodePng`

Writes a 1-bit greyscale PNG from a packed picture (rows padded to bytes, the leftmost pixel in the top bit, a set bit black, as `ui::Display::drawBitmap` reads it). The pixel data is stored, not compressed: a 240 × 320 panel is under 10 KB that way, the writer needs no dependency, and a stored deflate block is the one part of the format that cannot be got wrong. Pictures larger than one block (65,535 bytes) are split.

## Tests

`test/test_sim_panel` decodes the PNG again with the rules of the format (signature, the CRC of every chunk, the zlib header, stored blocks and their length check, Adler-32, filter bytes) and compares every pixel; widths that are not a multiple of eight; a picture of two blocks; one pixel; a damaged file is refused by that decoder; the exact size of a panel picture. The panel: the first frame is full and the panel blank before it; an unchanged picture costs nothing; every tenth change is full; a static picture is refreshed in full after four hours; the refresh is busy for its duration; the panel keeps the picture of the last refresh and the one before it; the PNG is the shown picture; a file is written and an unwritable path is reported.
