# PhonePlus

A distraction-free emergency phone for children, built for the LilyGO T-Deck Pro MAX
(ESP32-S3, e-ink display, 4G modem, LoRa radio, GPS): call the parents, read and write
SMS, send the position at the press of a button, and nothing else. The user interface
is multilingual — English, German and French ship with it, and anyone can add a
language by writing one text file.

> **Status: being rebuilt from the ground up.** This repository grows module by module;
> each module arrives only when it is finished, tested and documented. What exists today
> is listed below — nothing in this repository describes something that is not there.

## What exists

| Module | What it is | Where |
|---|---|---|
| Translation library | catalog parser, language selection with fallback, `{placeholder}` formatting, plural rules | `src/i18n/`, [docs/i18n.md](docs/i18n.md) |
| Phone numbers, contact book | normalisation and comparison of numbers, the whitelist with parent rights | `src/app/`, [docs/contacts.md](docs/contacts.md) |
| Quiet hours | school-time windows per weekday | `src/app/`, [docs/quiet-hours.md](docs/quiet-hours.md) |
| Time zones | named zones with EU and US summer-time rules, calendar arithmetic | `src/app/`, [docs/time-zones.md](docs/time-zones.md) |
| Log level | which log lines are written, and which are never dropped | `src/app/`, [docs/log-level.md](docs/log-level.md) |
| Map link | map address from a template, five decimals, no float printf | `src/app/`, [docs/map-link.md](docs/map-link.md) |
| Shake detector | counts shakes from acceleration samples | `src/app/`, [docs/shake-detector.md](docs/shake-detector.md) |
| Message store | message history with an append-only log, read/sent/delivered/deleted marks | `src/app/`, [docs/message-store.md](docs/message-store.md) |
| TOML subset | parser and writer for the configuration files, as a plain document tree | `src/app/`, [docs/toml.md](docs/toml.md) |
| Safe paths | validation of file paths from outside | `src/app/`, [docs/safe-path.md](docs/safe-path.md) |
| Operator label | short operator name for the status line, UTF-8 helper | `src/app/`, [docs/operator-label.md](docs/operator-label.md) |
| SOS ping | position-sending state machine; its message wording comes from the catalogs | `src/app/`, [docs/sos-ping.md](docs/sos-ping.md) |
| SOS flow | choice, countdown and five-tap override in front of the ping | `src/app/`, [docs/sos-flow.md](docs/sos-flow.md) |
| SMS command | the remote location query, trigger words per language | `src/app/`, [docs/sms-command.md](docs/sms-command.md) |
| Power policy | the decision what may sleep when | `src/app/`, [docs/power-policy.md](docs/power-policy.md) |
| Hardware capabilities | the interfaces between the software and the hardware: clock, power, positioning, modem, log, vibration, files, WLAN, TCP, motion, radio | `src/hal/`, [docs/hardware-capabilities.md](docs/hardware-capabilities.md) |
| Test doubles | a scriptable fake for every hardware capability | `testing/fakes/`, [docs/test-doubles.md](docs/test-doubles.md) |
| Contract tests | one suite per capability, run against every implementation | `testing/contracts/`, [docs/hardware-capabilities.md](docs/hardware-capabilities.md) |
| Display and frame buffer | the 240 × 320 1-bit display interface; an in-memory buffer with clip detection and golden files for tests | `src/ui/`, `testing/`, [docs/ui-display.md](docs/ui-display.md) |
| Fonts and text engine | four Lucida bitmap fonts with our own interface symbols, the character set, UTF-8 text measuring, fitting, wrapping and drawing | `src/ui/`, `assets/fonts/`, [docs/ui-text.md](docs/ui-text.md) |
| Layout testing in every language | a pseudo-language (accented, 40 % longer, bracketed) and a helper that runs a test in every shipped language and in it | `src/i18n/`, `testing/`, [docs/ui-layout-testing.md](docs/ui-layout-testing.md) |
| UI framework | input model, screen stack with `onEnter`/`onLeave`, soft keys, status bar, tile grid, list, text block, button | `src/ui/`, [docs/ui-framework.md](docs/ui-framework.md) |
| E-ink refresh policy and frame budgets | when to refresh the panel in part, in full or not at all; tests that fail when the interface makes the panel work more than a scenario allows | `src/ui/`, `testing/`, [docs/ui-refresh.md](docs/ui-refresh.md) |
| Screen frame and wording | the base of every screen: status bar from the core's state, notice line, soft keys, touch translation; every code of the core worded from the catalogs | `src/ui/`, `testing/`, [docs/ui-screens.md](docs/ui-screens.md) |
| Settings, Wi-Fi, network, status and language screens | the options, Wi-Fi with what to join it with, the network in full, the status and switching off, the language picker | `src/ui/screens/`, [docs/screen-settings.md](docs/screen-settings.md), [docs/screen-wifi.md](docs/screen-wifi.md), [docs/screen-net.md](docs/screen-net.md), [docs/screen-status.md](docs/screen-status.md), [docs/screen-language.md](docs/screen-language.md) |
| Language catalogs | English (the reference), German, French | `lang/` |
| Checks | catalog parity and character set, generated fonts, documentation and code hygiene, privacy | `scripts/` |

## Build and test

Requires [PlatformIO Core](https://docs.platformio.org/page/core/installation.html) and Python 3.11 or newer.

```bash
pio test -e native              # host tests of the C++ code
python3 scripts/check_i18n.py   # the language catalogs against the English reference
python3 scripts/check_i18n.py --usage # every catalog key is used, no unknown key in the code
python3 scripts/check_docs.py   # leftovers: broken links, missing files, placeholder markers, non-English text
python3 scripts/check_layers.py # no platform headers in src/, interfaces depend on nothing
python3 scripts/make_font.py --check # the generated fonts match their sources
python3 scripts/check_behaviour.py --complete # every row of docs/app-behaviour.md is ticked and has its test
python3 scripts/privacy_check.py
```

## Rules this repository keeps

- **English everywhere** — code, comments, documentation, commit messages. The only other
  languages live in `lang/` (the interface) and `docs/<language-code>/` (translated documentation).
- **Comments explain why**, not what.
- **Every module has tests**, and a bug found gets a test that fails when the bug is put back.
- **No leftovers**: no placeholder markers, no documentation of things that do not exist
  (`scripts/check_docs.py` checks this on every change).
- **No personal data**: example phone numbers and places are invented
  (`scripts/privacy_check.py` checks this on every change).

## License

MIT — see [LICENSE](LICENSE). The display fonts are the exception: the letters are Lucida Sans bitmap fonts under their own licence ([assets/fonts/LICENSE-Lucida-BH.txt](assets/fonts/LICENSE-Lucida-BH.txt), listed in [THIRD_PARTY.md](THIRD_PARTY.md)), and the licence requires these notices:

(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents pending in the U.S. and foreign countries. OPEN LOOK is a trademark of AT&T. Used by written permission of the owners.

(c) Copyright Bigelow & Holmes 1986, 1985. Lucida is a registered trademark of Bigelow & Holmes. Permission to use the Lucida trademark is hereby granted only in association with the images and fonts described in this file.
