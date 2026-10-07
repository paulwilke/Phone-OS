# Third-party material

The repository is under the MIT licence, copyright "PhonePlus contributors" ([LICENSE](LICENSE)). It stores one third-party work, the Lucida Sans bitmap fonts, and one file that is a copy (the board description, below). Everything else listed here is fetched by a build or read for facts and is not stored.

| Tool | Licence | Used for |
|---|---|---|
| [PlatformIO Core](https://platformio.org/) | Apache-2.0 | building and running the host tests (`pio test -e native`) |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | MIT | the test framework; PlatformIO downloads it when the tests run and nothing of it is stored here |

### Stored in the repository

| Work | Licence | Used for |
|---|---|---|
| Lucida Sans bitmap fonts `luBS10`, `luRS14`, `luBS18`, `luBS24` (Bigelow & Holmes; in `assets/fonts/*.bdf`, fetched from the `tools/font/bdf/` folder of [u8g2](https://github.com/olikraus/u8g2)) | the Lucida licence, [assets/fonts/LICENSE-Lucida-BH.txt](assets/fonts/LICENSE-Lucida-BH.txt): free for individual and commercial software, the glyphs must not be changed, the two notices below must appear in the user documentation and in the code comments | the letters of the display fonts; `src/ui/fonts/*.h` is generated from them and carries the notices as a comment |

The notices the licence requires:

(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents pending in the U.S. and foreign countries. OPEN LOOK is a trademark of AT&T. Used by written permission of the owners.

(c) Copyright Bigelow & Holmes 1986, 1985. Lucida is a registered trademark of Bigelow & Holmes. Permission to use the Lucida trademark is hereby granted only in association with the images and fonts described in this file.

The `.bdf` files are the upstream files except for one comment line, from which the author's e-mail address was removed because this repository keeps no e-mail addresses (`scripts/privacy_check.py`). The generator moves a glyph into its cell where the source puts ink outside it, and changes no pixel; see [docs/ui-text.md](docs/ui-text.md). The interface symbols in `assets/fonts/symbols.txt` are our own and under the repository's licence.

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
| [Meck](https://github.com/pelgraine/Meck), an MIT-licensed firmware for this board (state of 2026-09-15) | MIT, copyright Scott Powell / rippleradios.com (the copyright line is known from the notes of the earlier project; the verbatim licence file has not been compared) | `boards/t-deck_pro_max.json` **is a copy of Meck's board description** (unchanged); the pin numbers and bus addresses in `src/board/PinMap.h` and the key matrix of `src/board/KeyboardDecoder.cpp` were read from Meck's code and checked on the device; the register sequence of `src/board/Tca8418.cpp` follows Meck's except for one register, where we write the chip's default. No other file of Meck is in this repository. |
| [T-Deck-MAX](https://github.com/Xinyuan-LilyGO/T-Deck-MAX), the manufacturer's repository | MIT (as stated by the earlier project; not compared) | the register sequences of the touch chip (`src/board/Cst66xx.cpp`) and the confirmation of the pin map: facts about the chip, written as our own code |
| Adafruit_DRV2605 (Adafruit) | BSD, Adafruit's usual licence (not compared) | the order of the register writes in `src/board/Drv2605.cpp`, checked against the data sheet; our own code |

**State: not settled.** The board file is a copy, so Meck's copyright and permission notice have to accompany it, and this repository does not hold Meck's licence text, because it was not available to verify. The earlier project kept a reconstruction of the standard MIT text with the copyright line; it is not copied here because a reconstruction is not the licence file. This is open point 13 in [docs/open-points.md](docs/open-points.md): compare Meck's `LICENSE`, then add its text here, or replace the board file with one of our own. The rest of this table is facts (numbers, addresses, register values) expressed in code of our own.

### Not used

| Work | Licence | Why it is listed |
|---|---|---|
| GxEPD2 (display library) | GPL-3.0 | **not used**: it would have made firmware binaries GPL. The panel is driven by our own code from the vendor's documents (above). Nothing of it is in the repository or its build. |
| MeshCore (LoRa mesh firmware) | MIT | **not used**: no include, no file; `scripts/check_layers.py` fails on one. |
| Adafruit GFX | BSD | **not used**: the text engine and the fonts are our own (`src/ui/Text.cpp`). |

### Tools

The Python scripts in `scripts/` use only the standard library.

Anything that is copied, derived or linked into the repository later is added to this list in the same commit that adds it, with its licence.
