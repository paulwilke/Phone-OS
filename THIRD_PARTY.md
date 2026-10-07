# Third-party material

The repository contains one third-party work, the Lucida Sans bitmap fonts, and uses two tools that are not part of it. The fonts are stored here; the tools are not.

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
| the firmware image of the motion sensor hub BHI260AP (Bosch Sensortec, delivered inside SensorLib as `bosch_app30_shuttle_bhi260`) | **not verified** | the sensor loads it into its RAM at every power-up; it is not stored in this repository |

**Binaries that contain the Bosch firmware image are for the owner's own device until its redistribution terms are verified.** The image is fetched by the build as part of SensorLib; nothing of it is committed here, and a binary of the `device` environment must not be published or handed to others.

### Read for values, not stored here

| Work | Licence | Used for |
|---|---|---|
| Good Display, *GDEQ031T10* product specification, revision 1.0, 2022-03-08 | none stated by the vendor | the panel's data: resolution, BUSY, operation flow, power sequence (`src/board/GDEQ031T10Profile.cpp`, [docs/device-display.md](docs/device-display.md)) |
| UltraChip, *UC8253* datasheet, `UC8253c_A0.6` | none stated | the meaning of the controller's commands and registers the profile sends |
| Good Display's Arduino/ESP32 demo for the GDEQ031T10 (`A32-GDEQ031T10-2FP-20230616`, `AU-GDEQ031T10-FP-20221013`), published for use with the panel | none stated | the values the vendor sends to this panel (panel setting, temperature override, data interval, timings, window bytes, sleep). Hardware parameters only: **no code and no comment of it is copied** |

The documents were fetched by the owner's permission and are not in the repository; the vendor states no licence for them. The values are facts about the hardware, written in our own words and checked against the datasheets (the places where the documents disagree are listed in [docs/device-display.md](docs/device-display.md)). GxEPD2 (GPL-3.0) is not used.

### Tools

The Python scripts in `scripts/` use only the standard library.

Anything that is copied, derived or linked into the repository later is added to this list in the same commit that adds it, with its licence.
