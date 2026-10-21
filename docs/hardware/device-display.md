# The display: the panel's profile, the pipeline, and what is verified

Code: `src/board/` (`Bitmap1`, `EinkDisplay`, `EpdWire`, `PanelProfile`, `GDEQ031T10Profile`, `Uc8253`, `SpiPort`), `device/Esp32SpiPort.h`; tests in `test/test_eink_display/` (the pipeline, with an invented profile) and `test/test_gdeq031t10_profile/` (the panel's own profile) against `FakeSpiPort` (`testing/fakes/FakeSpiPort.h`).

## The panel and where its values come from

The panel is a GDEQ031T10 on a UC8253 controller. **No GxEPD2** (GPL-3.0 would make every firmware binary GPL), and no table copied from any library or from the vendor's code. The values a driver needs are hardware parameters: command codes, register values, reset and busy timing, busy polarity, the window format, the data polarity and the sleep sequence. They were taken from the documents below and written in our own words in `src/board/GDEQ031T10Profile.cpp`, one comment per value.

| Document | Version | What was taken from it |
|---|---|---|
| Good Display, *GDEQ031T10* product specification | revision 1.0, 2022-03-08 (file `GDEQ031T10-0805.pdf`) | resolution 240 x 320 (1.3), the pin list and BUSY (1.5), the update times and the advice on full refreshes (3.3, 3.4), the operation flow (3.6.1) and the power sequence (3.6.2) |
| UltraChip, *UC8253* datasheet | `UC8253c_A0.6` (file `UC8253.pdf`) | the meaning of every command and register the profile uses: panel setting 0x00, power off 0x02, power on 0x04, deep sleep 0x07, data 0x10 and 0x13, refresh 0x12, data interval 0x50, partial window 0x90, partial in 0x91, cascade setting 0xE0, force temperature 0xE5, the data and LUT tables of the KW mode |
| Good Display's demo for the panel (Arduino/ESP32), archives `A32-GDEQ031T10-2FP-20230616` and `AU-GDEQ031T10-FP-20221013` | 2023-06-16 and 2022-10-13 | the values the vendor sends: 0x1F, 0xE0/0xE5/0xD7 and the sequence of calls, the 10 ms reset, the 1 ms after the refresh command, the 100 ms before deep sleep, the busy level, the window bytes, white as 1, and a picture that is upright when read as 240 pixels per row, most significant bit first |

None of the three is in this repository. The vendor states no licence for the PDF files or the demo; the demo is published for use with the panel. Nothing of its code or comments was copied. The files were fetched by the owner's permission; `THIRD_PARTY.md` lists them.

**The panel runs on the controller's own waveform (OTP).** There is no waveform table, no power setting and no booster setting in the profile, because neither the demo nor the datasheet's flow sends one. A wrong value of those can damage the panel, so none is added, and a test fails if one appears.

### What the values mean

- **Full refresh**: reset, panel setting 0x1F, power on and wait, the old picture to 0x10, the new one to 0x13, refresh 0x12, wait, power off, deep sleep. The controller measures the temperature itself and takes the waveform for it; the screen flashes several times (datasheet 3.3).
- **Partial refresh**: the same start, then **0xE0 = 0x02** (bit TSFIX of the cascade setting: the temperature comes from register 0xE5 and not from the sensor) and **0xE5 = 0x79** (that register: the temperature the waveform is chosen for, coded as in the sensor table, so 121 degrees), the data interval 0x50 = 0xD7 (the reset default), partial in 0x91, the window 0x90, old and new data of the window only. **This is the vendor's way to a partial refresh without a waveform table: the OTP holds one waveform per temperature range, and a claimed temperature far above the panel's own range selects the short, non-flashing one.** The UC8253 datasheet documents the register but not which range holds which waveform; that 0x79 gives the partial one is the demo's statement only. The panel's own range is 0 to 50 degrees (datasheet 3.1), so how the picture looks at the cold end is not known. The same trick with 0x5A or 0x6E is the vendor's "fast" refresh (about 1.0 s and 1.5 s, one flash): the interface's policy has no use for it, and it is not in the profile.
- **The window**: x start and x end as bit positions 7..3 of a byte (a multiple of 8, and the last pixel of a byte), y start and y end with nine bits, and a last byte 1 (gates scan inside and outside the window, the default). The controller says the end must be greater than the start, so a window is at least 16 pixels wide and 2 rows high; a smaller change is sent as the window around it (`minWindowWidth`, `minWindowHeight`), with the unchanged pixels in old and new data alike.
- **The polarity**: 1 is white, 0 is black. The demo clears with 0xFF and fills black with 0x00, and with KW/R = 1 and DDX = 01 the controller reads data 0 as the black-keeping waveform. The picture is not rotated: 240 pixels per row, most significant bit first, rows from the top.
- **The sleep**: power off, wait for BUSY, 100 ms, deep sleep with the check code 0xA5. Deep sleep is left only by a hardware reset, so **every refresh starts with the reset and the start commands**, as the vendor's demo does ("re-initialization is required for every full screen update").
- **BUSY** is low while the panel is busy. After 0x12 the line stays "free" for a moment, so the driver waits 1 ms (the demo's value; the datasheet's minimum is 200 microseconds) before it looks. A panel that stays busy for 10 seconds (a full update takes about 3 s) is logged as hung.

### Where the documents disagree, or do not say

- **Panel setting 0x1F (demo) or 0x0F (flow chart in the datasheet).** They differ in the KW/R bit: 1 is black/white with one data plane, 0 is black/white/red with a second. The panel has no red and the demo is the vendor's code for this panel, so **0x1F**.
- **Resolution.** The flow chart has a resolution step, the demo sends none (the panel setting's RES field is 00, "240 x 120"), and the picture is 240 x 320. The profile follows the demo: it sends no resolution command, which changes nothing the OTP values do not already decide. If the picture turns out to be cut to 120 rows, 0x61 (resolution) is the first thing to look at.
- **Power off is sent twice by the demo** (after the refresh and again in its sleep function). Once is sent here.
- **The datasheet recommends a full refresh after five partial or fast refreshes** (3.3). The interface's policy ([ui-refresh.md](../ui/ui-refresh.md)) does one after ten, as measured on the previous code base; this work does not change the policy.
- **The partial window data size**: the demo sends the picture byte by byte, each byte in its own chip select; the driver sends a window in one write. The bytes are the same.
- **The partial-out command (0x92)** is not sent: the demo never sends it, and the reset before the next refresh leaves partial mode.

## What is built and tested on the host

- the frame buffer and its dirty rectangle (`Bitmap1`): the bounding box of the pixels in which the picture differs from what the panel shows, widened to whole bytes on x;
- the pipeline (`EinkDisplay`): every refresh starts with the hardware reset and the profile's start commands; a full refresh sends the whole old picture to data register 1 and the whole new one to register 2; a partial refresh sends only the dirty rectangle, grown to the profile's smallest window, to both inside a partial window; a window the panel cannot address becomes a full refresh; after each refresh the profile's sleep commands go out; a command that leaves the panel busy beyond the profile's timeout is logged and the next frame is a full refresh;
- the policy of [ui-refresh.md](../ui/ui-refresh.md) through the real `RefreshController`: the first frame is full, an unchanged picture costs nothing, every tenth change is full, a static picture is refreshed in full after four hours;
- the wire (`EpdWire`): commands with the data line low and data with it high, each inside a chip select, the reset pulse, the busy wait;
- **the panel's profile** (`test_gdeq031t10_profile`): the exact bytes of the start, a full refresh, a partial refresh with its window, and the sleep, compared with the sequences as the documents above give them, written down in the test in its own notation; the pictures (old white, new with black as 0), the window's rules and edges, the timing (10 + 10 ms reset, 1 ms after the refresh command, 100 ms before deep sleep) and that no power, booster, resolution or waveform command is sent. Each of those tests has a counter-check that changes one value of the profile and requires the comparison to fail.

`test_eink_display` uses an invented profile with values that mean nothing: what it proves is the driver's sequencing.

**What the host tests prove is that the profile sends what the documents say. They do not prove that the panel shows a picture.** Verified on the host: the sequences against the vendor's documentation, the pipeline, the policy. Not verified: everything in the next section. That the demo's sequence is what this driver sends is checked by reading; that the panel then draws is the owner's check with the device.

## In the firmware

`device/main.cpp` hands `gdeq031t10Profile()` to the display, and the start log says which profile is used (`Display: profile GDEQ031T10 on UC8253, OTP waveform`). A build without a profile still says `Display: not configured` and does not drive the panel. **That the firmware compiles with the profile is shown by `pio run -e device`; that it shows a picture has not been seen.**

## Unverified, and cannot be verified without the panel

The first picture; that the window format and the old/new data of a partial refresh behave on this panel as the controller's datasheet says; that the temperature override gives the partial waveform (and how it looks at the cold end); the polarity and the orientation on the real glass; the refresh times (measured on the device earlier with another library: about 430 to 480 ms partial, 1.1 s full); ghosting after ten partial refreshes. The owner's checklist ([bring-up.md](bring-up.md)) lists them.
