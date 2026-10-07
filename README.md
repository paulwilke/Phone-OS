# PhonePlus

A distraction-free emergency phone for children, built for the LilyGO T-Deck Pro MAX
(ESP32-S3, e-ink display, 4G modem, GPS): call the parents, read and write SMS, send the
position at the press of a button, and nothing else. Only the people in the contact book can
call or write to the child. The interface is multilingual: English, German and French ship
with it, and anyone can add a language by writing one text file.

Parents start with the user's guide: [English](docs/user-guide.md),
[Deutsch](docs/de/benutzerhandbuch.md), [Français](docs/fr/guide-utilisateur.md).

**What is verified.** The logic is tested on a laptop (more than 2,400 host tests) and runs in a
simulator; the firmware compiles. Nothing of it has been verified on the device yet: *compiled*,
*tested on the host* and *verified on the device* are kept apart everywhere, and what is still to
be done is in [docs/open-points.md](docs/open-points.md).

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

MIT — see [LICENSE](LICENSE). The display fonts are the exception: the letters are Lucida Sans bitmap fonts under their own licence ([assets/fonts/LICENSE-Lucida-BH.txt](assets/fonts/LICENSE-Lucida-BH.txt), listed in [THIRD_PARTY.md](THIRD_PARTY.md)), and the licence requires these notices:

(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents pending in the U.S. and foreign countries. OPEN LOOK is a trademark of AT&T. Used by written permission of the owners.

(c) Copyright Bigelow & Holmes 1986, 1985. Lucida is a registered trademark of Bigelow & Holmes. Permission to use the Lucida trademark is hereby granted only in association with the images and fonts described in this file.
