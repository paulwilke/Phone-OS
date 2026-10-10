# Changelog

Each release names what changed for the people who use the device, what was removed and what is not verified. "Host-tested" means a test under `pio test -e native` or a Python or Node test; "compiled" means only built; "verified on the device" means seen on the real hardware, which is only what [docs/bring-up.md](docs/bring-up.md) records with a date. The notes of a release are in `docs/release-<major>.<minor>.md`.

## 4.1.4

- **The battery shows what the voltage says when the gauge says almost nothing.** On 2026-10-09 the fuel gauge fell from 55 % to 6 % in one step at 3627 mV and then said 0 % for three hours while the device ran on to 3426 mV at the same load. The gauge had never learned the battery (cycle count 0). On the battery, when the gauge says 30 or more points less than the voltage gives, the percentage is now the voltage's own number (a table measured in that discharge), the time remaining is withheld, and the log says so once. Before, the display showed 0, and switching Wi-Fi on would have been refused for "battery below 20 percent" (nobody tried in that time, so the log has no such line). Host-tested; not verified on the device.
- **The status page shows the voltage** in millivolts with the flat and the full mark (two short lines: "Voltage: 3561 mV" and "Flat 3430, full 4200 mV") and says "Battery almost empty" at 10 % or less on the battery. Host-tested.
- **Deep discharge is not protected by the firmware, and nothing shows that the cell is.** Written down in [docs/battery.md](docs/battery.md) with the decision for the owner (open point 18).

## 4.1.3

- **The energy log is back.** `/energy.log` on the card gets one line every ten minutes: the time, the uptime, the fuel gauge's percentage, voltage, current and remaining charge, and whether a cable is attached, the modem may sleep or the device is awake. It exists to explain a battery that ran down fast (on 2026-10-09 the log showed no sleep at all: `modem_sleep = false` keeps modem and processor awake). It is read over the cable (`deviceconfig.py read energy.log`) and on the Files tab, and rotates to `energy.old.log` at 64 KB. 3.2 wrote a file of that name in another format; the Files tab no longer marks it as unused. Host-tested; not verified on the device. See [docs/energy-log.md](docs/energy-log.md).

## 4.1.2

The project is called **Phone OS**: generic words that nobody can register as a trade mark ("PhonePlus" is registered by others). Only names change, no behaviour; what the parents notice is in [docs/release-4.1.md](docs/release-4.1.md).

- **The splash picture** says "Phone OS". Host-tested; not verified on the device.
- **The parents' page** is titled "Phone OS", and every change it sends carries the header `X-Phone-OS: 1` instead of `X-Phoneplus: 1`; the device refuses a change without the new header. A page the browser kept from an earlier version must be reloaded once. Host-tested; not verified on the device.
- **Backup** saves `phone-os-backup.zip`; the 4.0 bundle is still read. Host-tested.
- **The default network names are `phone-os`**: the access point, the host name in the home network and the radio name. A name already saved on the card stays. Host-tested; not verified on the device.
- **The licence** names "Phone OS contributors"; the MIT text is unchanged.

## 4.1.1

- **An SOS without a position carries the last known one**, said as such and with its age ("No GPS. LAST KNOWN position, 25 min ago", "LETZTE BEKANNTE Position vor 25 Min."), so that nobody takes it for the present place. Seen on the device 2026-10-08: an SOS from inside a house went out without any place. The last position is kept while the device runs (from the start-up fix, an SOS, a query, the GPS page), not across switching off. Host-tested; not verified on the device.
- **The splash picture is back**: "PhonePlus", "T-Deck Pro MAX" and the version while the device starts, as in 3.x. Compiled; not verified on the device.
- **Switching off shows "Switched off"** before the battery is disconnected: e-ink keeps its last picture, and a device that went off showing a page looked on. With a cable the switch-off is refused at once, with the notice, without shutting the modem down (it was asked again and again before); every request is logged ("Power: switch off asked for"). Compiled and host-tested where it can be; not verified on the device.

## 4.1.0

The interface, tidied after the owner's test of 4.0 on the device (2026-10-07/08). The notes for the parents and the child, and what to try on the device, are in [docs/release-4.1.md](docs/release-4.1.md); the acceptance test to go through each time is [docs/de/abnahmetest.md](docs/de/abnahmetest.md).

### The parents' page

- **The settings are on three tabs**: Basics, School time and Advanced. Which key is on which tab is data of `config/schema.toml`; saving one tab keeps what the others hold. Host-tested and tried in the simulator.
- **Fixed: the settings form saved the interface language as the language of the texts** (and the child's name as the radio name): two keys of the same name were read from the wrong section. Host-tested.
- **School-time windows are compact and their days are ticked** (Mo to Su), from and to are time fields. Host-tested.
- **The Files tab hides what a Mac leaves on the card**, and "Remove Mac leftovers" deletes its `._` files; `energy.log` and `firmware.txt` are marked as files of 3.2. Host-tested.
- **The backup is a ZIP archive** of `settings.toml` and `contacts.toml`, byte for byte, and the restore works: in 4.0 the page sent it to a route the device does not have. A backup of 4.0 is still restored. Host-tested; checked with `unzip` against the simulator.
- **The tiles of the home screen's second page can be switched off** (`[home]`, tab Advanced).
- **A contact can be ticked "SOS"**: the SOS goes to every ticked contact; without a tick, to the first call button as before. Sent as soon as one text is out, failed only when all were refused. Host-tested.
- **A language comes in three answers** (words, labels, help) so that each keeps half of the device's response buffer free.

### The device

- **Back has one place on every screen**, on the left; the soft-key bar is 4 pixels taller.
- **The home screen has no arrows**; the page dots are the two halves of the strip under the tiles; the light is on the first page.
- **In a call**, Hang up is the whole bar and the volume has the buttons Quieter and Louder on the screen.
- **With free text**, the ready-made messages are behind the key Templates; typing starts at once.
- **The options can switch the device off** (two presses; not on the cable).

Host-tested and run in the simulator; not verified on the device.

## 4.0.2

- **The status line names the network again** ("Telekom", "Orange"). The modem answered the question for the operator with a blank, so the line said only "Network". The device now asks for the network's code when the name is blank and turns the code into the operator's name (Germany, its neighbours and the usual holiday countries; elsewhere the code). The log says `Cellular: network Telekom` when the network changes. Host-tested; verified on the device (2026-10-08).
- **"Connecting ..." while the device connects.** After switching on, the status line said "No network", "Network" and "No calls yet" in turn before the operator's name. It now says "Connecting ..." until the network and the calls are ready; a network lost later is "No network" as before, and the network page still says which part is missing. Host-tested; not verified on the device.

## 4.0.1

- **The display fonts are Adobe's Helvetica bitmaps** instead of Lucida Sans, whose licence grants their use but says nothing about distributing or changing them; Adobe's notice allows both. The sizes and the characters are the same; the letters are a little narrower, so some texts take fewer lines. Host-tested; not verified on the device.
- **A text that was scrolled and then gets more room shows its first lines again.** Before, the lines above the scroll position stayed hidden although the whole text fitted. Host-tested.
- **The French warning after a wrong PIN is shorter** ("PIN faux. Ne pas redémarrer ...") and fits the page without scrolling. Host-tested.
- **The firmware is built for PlatformIO's own ESP32-S3 board** with the settings of the T-Deck Pro MAX, instead of a board description copied from another project. The build settings and the partition table are the same as before. Compiled; not verified on the device.

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
- **Save mode ("Sparmodus": radio only, cellular and Wi-Fi off), its cell probe and its notices.** It needs the radio, which 4.0 does not have, so it could only say "switch the radio on first". The owner decided to remove it completely; it is planned together with the radio, where an emergency radio mode will switch battery saving on implicitly ([docs/open-points.md](docs/open-points.md), item 9).
- **The hard-wired German interface words of 3.2.** German is now a catalog like any other.
- **The USB commands `dial`, `hangup`, `query` and `motion` of 3.2's tool.** The tool of 4.0 has `list`, `read`, `write`, `battery`, `sms` and `restart`.

### Known limits

- **Nothing of 4.0 has been verified on the device.** The logic runs more than 2,400 host tests and a simulator, and the firmware compiles. The display profile, the keyboard, the touch, the haptic motor, the fuel gauge, the charger, the receiver, the card, Wi-Fi, sleep and calls over VoLTE are checked against datasheets and measurements of the earlier firmware and by the host tests only. The owner's checklist [docs/bring-up.md](docs/bring-up.md) has 28 steps and its date and result columns are empty until the owner fills them.
- **The first picture on the panel is the first real test of the display profile** (steps 2 and 24 of the checklist); a wrong profile shows as an inverted, striped or cut-off picture.
- **A firmware binary is for the owner's own device.** It contains a sensor image of Bosch Sensortec whose redistribution terms are not verified, and LGPL code whose conditions for a published binary have not been assessed ([THIRD_PARTY.md](THIRD_PARTY.md), open points 13 and 14).
- **The wording of German and French was read by the author, not by a native speaker** ([docs/open-points.md](docs/open-points.md), item 15).
