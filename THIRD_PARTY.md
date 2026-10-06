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
| [SensorLib](https://github.com/lewisxhe/SensorLib) by Lewis He (`lib_deps` of the `device` environment only) | MIT | the fuel gauge's capacity write procedure |

### Tools

The Python scripts in `scripts/` use only the standard library.

Anything that is copied, derived or linked into the repository later is added to this list in the same commit that adds it, with its licence.
