# Changelog

Each release names what changed for the people who use the device, what was removed and what is not verified. "Host-tested" means a test under `pio test -e native` or a Python or Node test; "compiled" means only built; "verified on the device" means seen on the real hardware, which is only what [docs/bring-up.md](docs/bring-up.md) records with a date. The notes of a release are in `docs/release-<major>.<minor>.md`.

## 4.0.0

Version 4.0 is a new code base, written from the start in English, layered and tested on a laptop. It replaces the 3.2 firmware. The release notes with the owner's checklist are in [docs/release-4.0.md](docs/release-4.0.md).

### What is new against 3.2

- **The interface speaks English, German and French.** The language is `[interface] language` in `settings.toml` or the page **Language** on the device; the language of the texts the device sends to the parents is `[sms] language`. A further language is one catalog file on the card (`/lang/xx.toml`, at most 32,768 bytes) and needs no new firmware; [docs/translating.md](docs/translating.md) goes through it step by step. The parents' page has its own catalogs in the same three languages.
- **Only the contact book may call and write.** Calls and texts from numbers that are not in `contacts.toml` or on a call button are dropped without the child learning of it, and there is no switch for it. A text from the provider is dropped too and kept in the log.
- **Calls over VoLTE are set up by the driver.** The modem driver defines the data context 1 as `IPV4V6` and cycles the radio once when it differs, so that the IMS registers; it never dials without a registered IMS, does not believe a "call begun" report on its own and survives a restart of the modem during a call. The operator's access point name is `[modem] apn`.
- **The parents' page was rewritten.** Tabs for status, settings, contacts, secrets, files, log and backup; every file is checked before it is stored; five wrong passwords lock the page for sixty seconds; the password has at least 4 characters.
- **The battery gauge is checked at every start.** The chip ships with a wrong battery size; the device compares it with `[battery] capacity_mah` (default 1500) and writes it only when it differs. A percentage that the cell voltage contradicts is shown as "unknown".
- **New pages and values:** the network page shows whether the IMS is registered, the position page shows the last known position with its age, the status page shows the configuration problems it found, and a self-test page checks the board.
- **The USB console** (`scripts/deviceconfig.py`, `scripts/webbridge.py`) reads and writes the three configuration files with a checksum and a safe write, reads the logs, sends a text by hand and prints the diagnosis of the fuel gauge and the charger.
- **A school-time window can be switched off.** Each `[[quiet_hours]]` window has `enabled` (default `true`, so every existing card locks as before) and the parents' page shows it as a checkbox "active". A card without any window gets one built-in window, Monday to Friday 08:00 to 12:30, switched off: no school time runs until a parent ticks it. A switched-off window is ignored everywhere ([docs/quiet-hours.md](docs/quiet-hours.md)).
- **A simulator and generated pictures:** the whole application runs on a laptop on fakes of the hardware (`pio run -e sim`), and `scripts/make_demo.py` renders every page in three languages and a click-through demo.

### What changed in the files

A 3.2 card is read by 4.0 without changes ([docs/upgrading.md](docs/upgrading.md) goes through the upgrade and the way back); what is reported and ignored is this:

- `[mesh]` is read as `[radio]` and reported once; `features.mesh` is not read (the key is `features.radio`);
- `display.font_scale`, `time.gps_utc_offset_minutes`, `child.role` and the top-level `screens` list are not read and are reported as unknown keys;
- `[[tiles]]` entries are call buttons only; a tile with another action is reported and ignored;
- German day names in school time (`mo-fr`) and a third window on one day, which 3.2 accepted, are refused and reported: 4.0 knows `mon-fri` and allows two windows a day;
- a card without `[interface] language` and `[sms] language` gets English for the screens and for the texts sent to the parents; the German of 3.2 is `language = "de"` in both;
- the message history `messages.log` keeps its format; entries of earlier versions that hold a sentence are shown as it is.

### What was removed

- **The radio is not part of 4.0.** The interface, the radio page and the message path exist and are tested against a fake, but there is no driver for the radio chip and no protocol of our own, so the feature cannot be switched on in a way that does anything: the tile is off and a request to start the radio is answered with "not available". The owner decided to leave it out of 4.0 and to add it in a later release ([docs/open-points.md](docs/open-points.md), item 9).
- **The hard-wired German interface words of 3.2.** German is now a catalog like any other.
- **The USB commands `dial`, `hangup`, `query` and `motion` of 3.2's tool.** The tool of 4.0 has `list`, `read`, `write`, `battery`, `sms` and `restart`.

### Known limits

- **Nothing of 4.0 has been verified on the device.** The logic runs more than 2,400 host tests and a simulator, and the firmware compiles. The display profile, the keyboard, the touch, the haptic motor, the fuel gauge, the charger, the receiver, the card, Wi-Fi, sleep and calls over VoLTE are checked against datasheets and measurements of the earlier firmware and by the host tests only. The owner's checklist [docs/bring-up.md](docs/bring-up.md) has 28 steps and its date and result columns are empty until the owner fills them.
- **The first picture on the panel is the first real test of the display profile** (steps 2 and 24 of the checklist); a wrong profile shows as an inverted, striped or cut-off picture.
- **A firmware binary is for the owner's own device.** It contains a sensor image of Bosch Sensortec whose redistribution terms are not verified, and LGPL code whose conditions for a published binary have not been assessed ([THIRD_PARTY.md](THIRD_PARTY.md), open points 13 and 14).
- **The wording of German and French was read by the author, not by a native speaker** ([docs/open-points.md](docs/open-points.md), item 15).
