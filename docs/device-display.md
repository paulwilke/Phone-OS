# The display: what is built, and what is not

Code: `src/board/` (`Bitmap1`, `EinkDisplay`, `EpdWire`, `PanelProfile`, `Uc8253`, `SpiPort`), `device/Esp32SpiPort.h`; tests in `test/test_eink_display/` against `FakeSpiPort` (`testing/fakes/FakeSpiPort.h`).

## The decision and where it stops

The panel is a GDEQ031T10 on a UC8253 controller. **No GxEPD2** (GPL-3.0 would make every firmware binary GPL), and no table copied from any library. The driver was to be written from the controller's documented command set and the panel vendor's datasheet and published demo.

**That part stops here, as the work package says it must.** The panel's own values (panel setting, power and booster settings, data interval, resolution, the waveform tables of the full, fast and partial refresh, and the format of a partial window) could not be obtained from the datasheet or the vendor's demo in this work: neither was available to the session, and the only source of them it knew is GxEPD2, which is not used. A wrong power or booster value can damage the panel, so none was guessed. What is in the repository instead:

- **`Uc8253.h`**: the command codes only (panel setting 0x00 … partial out 0x92), with no value after any of them.
- **`PanelProfile`**: the panel's data as a structure that is handed to the driver. The driver contains no number of it.
- **No profile in the firmware.** `device/main.cpp` passes none, so `EinkDisplay::begin()` logs "Display: not configured, the panel is not driven" and every frame is dropped. The device boots, runs and is reachable over USB and Wi-Fi, but **shows nothing** until a profile exists.

**What the owner (or the next session) has to supply**: a `PanelProfile` filled from the GDEQ031T10 datasheet and Good Display's demo for it: `start`, `beforeFull`, `beforePartial`, `sleep` command lists, `blackIsZero`, `encodeWindow`, the reset and busy timings and the busy level. Nothing else in the driver changes.

## What is built and tested on the host

Everything that is not the wire:

- the frame buffer and its dirty rectangle (`Bitmap1`): the bounding box of the pixels in which the picture differs from what the panel shows, widened to whole bytes on x;
- the pipeline (`EinkDisplay`): a full refresh sends the whole old picture to data register 1 and the whole new one to register 2; a partial refresh sends only the dirty rectangle to both inside a partial window; a window the panel cannot address becomes a full refresh; after each refresh the profile's sleep commands go out; a panel that stays busy beyond the profile's timeout is logged and the next frame is a full refresh;
- the policy of [ui-refresh.md](ui-refresh.md) through the real `RefreshController`: the first frame is full, an unchanged picture costs nothing, every tenth change is full, a static picture is refreshed in full after four hours;
- the wire (`EpdWire`): commands with the data line low and data with it high, each inside a chip select, the reset pulse, the busy wait.

`test_eink_display` compares the traffic byte for byte with the documented sequence. **The profile it uses is invented test data**, with the shape of a real one and values that mean nothing. What it proves is the driver's sequencing; it does not prove that the panel shows a picture.

## Unverified, and cannot be verified without the panel

Every value of a profile; that the old/new data registers (0x10 and 0x13) behave as the pipeline assumes for this panel's partial waveform; the partial window format; the polarity of the data; the refresh times (measured on the device earlier with another library: about 430 to 480 ms partial, 1.1 s full); the first picture. The owner's checklist ([bring-up.md](bring-up.md)) lists them.
