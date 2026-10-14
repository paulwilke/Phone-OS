# Phone OS

Phone OS, an open firmware for the LilyGO T-Deck Pro MAX (ESP32-S3, e-ink display, 4G modem,
GPS). It makes the device a distraction-free emergency phone, for a child for example: call
the parents, read and write SMS, send the position at the press of a button, and nothing
else. Only the people in the contact book can call or write to the child. The interface is
multilingual: English, German and French ship with it, and anyone can add a language by
writing one text file.

This is version 4.2.2 ([CHANGELOG.md](CHANGELOG.md), [docs/release-4.2.md](docs/release-4.2.md)); the LoRa radio is not part of it.

Parents start with the user's guide: [English](docs/user-guide.md),
[Deutsch](docs/de/benutzerhandbuch.md), [Français](docs/fr/guide-utilisateur.md).

**What is verified.** The logic is tested on a laptop (more than 2,500 host tests) and runs in a
simulator; the firmware compiles and has run on the device since the first days of October 2026.
Three words are kept apart everywhere: *compiled*, *tested on the host* and *verified on the device*.
What was measured on the device is in [docs/hardware-lessons.md](docs/hardware-lessons.md), each entry of the
[CHANGELOG](CHANGELOG.md) says whether its change is verified on the device or only tested on the host (several are
not yet), the systematic pass is the checklist [docs/bring-up.md](docs/bring-up.md), whose date and result columns the owner
has not filled in, and everything still to be done is in [docs/open-points.md](docs/open-points.md).

## Build, test, run

Requires [PlatformIO Core](https://docs.platformio.org/page/core/installation.html) and Python 3.11 or newer.

```bash
pio test -e native                  # the host tests of the C++ code
pio run -e sim                      # the simulator: the application on fakes, with a console
.pio/build/sim/program --script sim/scripts/smoke.txt   # a scenario; without --script it is interactive (docs/simulator.md)
python3 scripts/make_demo.py        # pictures of every page in three languages and a click-through demo in out/demo/ (docs/demo.md)
pio run -e device                   # the firmware for the T-Deck Pro MAX; compile-only
pio run -e device -t upload         # flashes the connected device: the owner does this, nobody else
```

The checks that run before every commit are listed in [CONTRIBUTING.md](CONTRIBUTING.md); the first start of the
device is the checklist [docs/bring-up.md](docs/bring-up.md).

## How the code is layered

| Directory | Holds | May know |
|---|---|---|
| `src/hal/` | the capabilities: interfaces named after what the software needs (clock, modem, power, positioning, files, ...) | nothing |
| `src/app/`, `src/ui/` | the application (contact book, school time, SOS, messages) and the pages | `src/hal/` only; never a board or a driver |
| `src/drivers/`, `src/board/` | the modem driver, the receiver parser, the chips of the board | `src/hal/` |
| `src/config/`, `src/web/`, `src/boot/` | the configuration files, the parents' page, the one composition that the firmware and the simulator both run | the layers they need, as `scripts/check_layers.py` lists them |
| `device/` | the firmware's platform code: the Arduino and ESP32 side of the seams | `src/` |
| `sim/`, `testing/` | the simulator and the fakes of every capability | `src/` |
| `lang/` | the catalogs: the device's words and the parents' page's words, in every language | |

The rules are enforced by `scripts/check_layers.py` ([docs/device.md](docs/device.md)). Every document is listed in
[docs/README.md](docs/README.md), written from the code it covers.

## Rules this repository keeps

- **English everywhere**: code, comments, documentation, commit messages. The only other
  languages live in `lang/` (the interface) and `docs/<language-code>/` (translated documentation).
- **Comments explain why**, not what.
- **Every module has tests**, and a bug found gets a test that fails when the bug is put back.
- **No leftovers**: no placeholder markers, no documentation of things that do not exist, no code that
  nothing uses; open work is in [docs/open-points.md](docs/open-points.md) and nowhere else.
- **No personal data**: example phone numbers and places are invented.
- **Security limits are the owner's decision**, not a contributor's.

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to work on a change.

## License

MIT — see [LICENSE](LICENSE). Two things in the repository are under other terms, both listed in [THIRD_PARTY.md](THIRD_PARTY.md): the operator list of `src/drivers/ApnTable.cpp` is data from the Android Open Source Project under Apache-2.0 ([assets/licenses/LICENSE-Apache-2.0.txt](assets/licenses/LICENSE-Apache-2.0.txt)), and the letters of the display fonts are Adobe's Helvetica bitmap fonts under their own permission notice ([assets/fonts/LICENSE-Adobe-Helvetica.txt](assets/fonts/LICENSE-Adobe-Helvetica.txt)):

Copyright 1984-1989, 1994 Adobe Systems Incorporated. Copyright 1988, 1994 Digital Equipment Corporation. Helvetica is a trademark of Linotype-Hell AG and/or its subsidiaries.
