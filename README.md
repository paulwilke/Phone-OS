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
| Language catalogs | English (the reference), German, French | `lang/` |
| Checks | catalog parity, documentation and code hygiene, privacy | `scripts/` |

## Build and test

Requires [PlatformIO Core](https://docs.platformio.org/page/core/installation.html) and Python 3.11 or newer.

```bash
pio test -e native              # host tests of the C++ code
python3 scripts/check_i18n.py   # the language catalogs against the English reference
python3 scripts/check_docs.py   # leftovers: broken links, missing files, placeholder markers, non-English text
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

MIT — see [LICENSE](LICENSE).
