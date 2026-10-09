# Third-party material

The repository is under the MIT licence, copyright "Phone OS contributors" ([LICENSE](LICENSE)). It stores one third-party work, the Adobe Helvetica bitmap fonts. Everything else listed here is fetched by a build or read for facts and is not stored.

| Tool | Licence | Used for |
|---|---|---|
| [PlatformIO Core](https://platformio.org/) | Apache-2.0 | building and running the host tests (`pio test -e native`) |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | MIT | the test framework; PlatformIO downloads it when the tests run and nothing of it is stored here |
| [SDL2](https://www.libsdl.org/) | zlib | the window of the simulator (`pio run -e sim-sdl`, optional: `brew install sdl2`); CI and the other simulator build do not use it and nothing of it is stored here |
| [pyserial](https://github.com/pyserial/pyserial) | BSD-3-Clause | `scripts/deviceconfig.py` and `scripts/webbridge.py` talk to the device through it; PlatformIO brings it along (`~/.platformio/penv/bin/python`) and nothing of it is stored here |
| [Node.js](https://nodejs.org/) | MIT | runs `test/test_web_page/test.mjs` and `test/test_demo_page/test.mjs`; they import only its built-in modules |

### Stored in the repository

| Work | Licence | Used for |
|---|---|---|
| Adobe Helvetica bitmap fonts `helvB10`, `helvR14`, `helvB18`, `helvB24` (X.org `font-adobe-100dpi` with Markus Kuhn's ISO 10646-1 extension; in `assets/fonts/*.bdf`, fetched from the `tools/font/bdf/` folder of [u8g2](https://github.com/olikraus/u8g2)) | Adobe's and Digital's permission notice, [assets/fonts/LICENSE-Adobe-Helvetica.txt](assets/fonts/LICENSE-Adobe-Helvetica.txt): use, copy, modify, distribute and sell for any purpose without fee, with the copyright notices and the permission notice kept in all copies and in the supporting documentation; the names of Adobe and Digital are not to be used in advertising | the letters of the display fonts; `src/ui/fonts/*.h` is generated from them and carries the notices as a comment |

The notices:

Copyright 1984-1989, 1994 Adobe Systems Incorporated. Copyright 1988, 1994 Digital Equipment Corporation. Helvetica is a trademark of Linotype-Hell AG and/or its subsidiaries.

The `.bdf` files are the upstream files except for one comment line, from which Markus Kuhn's e-mail address was removed because this repository keeps no e-mail addresses (`scripts/privacy_check.py`). The generator moves a glyph into its cell where the source puts ink outside it, and changes no pixel; see [docs/ui-text.md](docs/ui-text.md). The generated fonts are not called Helvetica, because they also hold the interface symbols in `assets/fonts/symbols.txt`, which are our own and under the repository's licence.

### Fetched by the firmware build, not stored here

| Work | Licence | Used for |
|---|---|---|
| [SensorLib](https://github.com/lewisxhe/SensorLib) by Lewis He (`lib_deps` of the `device` environment only) | MIT | the fuel gauge's capacity write procedure and the driver of the motion sensor hub |
| the Bosch Sensortec driver sources inside SensorLib (`src/bosch/`: BHI260 host driver, BMM150, BMA4xx) | BSD-3-Clause (the files carry the notice) | compiled into the firmware through SensorLib; the licence asks for its notice with a binary |
| the firmware image of the motion sensor hub BHI260AP (Bosch Sensortec, delivered inside SensorLib as `bosch_app30_shuttle_bhi260`) | **not verified**: the image files carry no licence text | the sensor loads it into its RAM at every power-up; it is not stored in this repository |

**Binaries that contain the Bosch firmware image are for the owner's own device until its redistribution terms are verified.** The image is fetched by the build as part of SensorLib; nothing of it is committed here, and a binary of the `device` environment must not be published or handed to others.

### Platform of the firmware, fetched by the firmware build

| Work | Licence | Used for |
|---|---|---|
| the PlatformIO platform `espressif32` 6.11.0 | Apache-2.0 | building the firmware (`platformio.ini`) |
| the Arduino core for the ESP32 (`framework-arduinoespressif32`) | GNU LGPL 2.1 or later (the files of the core carry that notice); its bundled libraries `SD`, `FS`, `SPI`, `WiFi`, `Wire` carry Espressif's Apache-2.0 notice | the processor's peripherals behind the seams in `device/` |
| ESP-IDF (inside the Arduino core) | Apache-2.0 | the system layer under the core |

None of it is stored here. A firmware binary contains LGPL code from the Arduino core; the conditions that puts on handing a binary to others have not been assessed (open point 14 in [docs/open-points.md](docs/open-points.md)).

### Read for values, not stored here

| Work | Licence | Used for |
|---|---|---|
| Good Display, *GDEQ031T10* product specification, revision 1.0, 2022-03-08 | none stated by the vendor | the panel's data: resolution, BUSY, operation flow, power sequence (`src/board/GDEQ031T10Profile.cpp`, [docs/device-display.md](docs/device-display.md)) |
| UltraChip, *UC8253* datasheet, `UC8253c_A0.6` | none stated | the meaning of the controller's commands and registers the profile sends |
| Good Display's Arduino/ESP32 demo for the GDEQ031T10 (`A32-GDEQ031T10-2FP-20230616`, `AU-GDEQ031T10-FP-20221013`), published for use with the panel | none stated | the values the vendor sends to this panel (panel setting, temperature override, data interval, timings, window bytes, sleep). Hardware parameters only: **no code and no comment of it is copied** |

The documents were fetched by the owner's permission and are not in the repository; the vendor states no licence for them. The values are facts about the hardware, written in our own words and checked against the datasheets (the places where the documents disagree are listed in [docs/device-display.md](docs/device-display.md)). GxEPD2 (GPL-3.0) is not used.

### Derived from other projects

| Work | Licence | What was taken |
|---|---|---|
| [Meck](https://github.com/pelgraine/Meck), a firmware for this board (state of 2026-09-15) | MIT, "Copyright (c) 2025 Scott Powell / rippleradios.com" (compared with Meck's `license.txt` on 2026-10-08) | the pin numbers and bus addresses in `src/board/PinMap.h` and the key matrix of `src/board/KeyboardDecoder.cpp` were read from Meck's code and checked on the device; the register sequence of `src/board/Tca8418.cpp` follows Meck's except for one register, where we write the chip's default. Facts, written as our own code: no file, line or comment of Meck is in this repository. |
| [T-Deck-MAX](https://github.com/Xinyuan-LilyGO/T-Deck-MAX), the manufacturer's repository | MIT (the repository's licence, seen on 2026-10-08) | the register sequences of the touch chip (`src/board/Cst66xx.cpp`) and the confirmation of the pin map: facts about the chip, written as our own code |
| [Adafruit_DRV2605](https://github.com/adafruit/Adafruit_DRV2605_Library) (Adafruit) | MIT (the repository's licence, seen on 2026-10-08) | the order of the register writes in `src/board/Drv2605.cpp`, checked against the data sheet; our own code |

The board is PlatformIO's own `esp32-s3-devkitc-1`, set in `platformio.ini`; no board description of another project is stored here.

### Not used

| Work | Licence | Why it is listed |
|---|---|---|
| GxEPD2 (display library) | GPL-3.0 | **not used**: it would have made firmware binaries GPL. The panel is driven by our own code from the vendor's documents (above). Nothing of it is in the repository or its build. |
| MeshCore (LoRa mesh firmware) | MIT | **not used**: no include, no file; `scripts/check_layers.py` fails on one. |
| Adafruit GFX | BSD | **not used**: the text engine and the fonts are our own (`src/ui/Text.cpp`). |

### Tools

The Python scripts in `scripts/` use only the standard library, except the two USB tools, which need pyserial (above).

Anything that is copied, derived or linked into the repository later is added to this list in the same commit that adds it, with its licence.
