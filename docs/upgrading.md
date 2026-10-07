# Upgrading from 3.2 to 4.0

For the owner of a device that runs the 3.2 firmware. The card of a 3.2 device is read by 4.0 without changes: `test/test_upgrade_3_2/` runs the loader, the runtime and the card classes over files in the layout 3.2 wrote (`test/data/card-3.2/`, invented values), and this page is written from what those tests pin. **Nothing of 4.0 has been verified on a device**, so the upgrade ends in the checklist [bring-up.md](bring-up.md), not in this page.

## 1. Before you flash: save the card

Copy everything on the card to a folder on your computer: `/config/` (`settings.toml`, `contacts.toml`, `secrets.toml`), `/messages.log`, `/system.log` and `/system.old.log`, and, if they are there, `/firmware.txt` and `/energy.log`. The folder holds your SIM PIN, the web password and the Wi-Fi passwords: keep it out of any repository and out of a synchronised folder you share.

You need the copy for the way back (section 5): the 3.2 firmware needs the files as 3.2 wrote them, and 4.0 rewrites `settings.toml` when the child changes a setting on the device.

## 2. Look at the three files once

4.0 reads every key 3.2 wrote, with these exceptions, which it reports in the log (a line `config: ...` that is always written) and ignores. On the status page, which exists when `parent_tools` is on, "Configuration: n problems" counts them.

| In 3.2 | In 4.0 |
|---|---|
| `[mesh]` (`name`, `peer`) | read as `[radio]` and reported as `mesh-alias`; the next write of the file uses `[radio]` |
| `features.mesh` | not read, reported as `unknown-key`; the key is `features.radio`, and 4.0 has no radio ([release-4.0.md](release-4.0.md)) |
| `display.font_scale`, `time.gps_utc_offset_minutes`, `child.role`, the top-level `screens` | not read, reported as `unknown-key`; the pages, the time zone and the parent tools are set by `parent_tools`, `[time] timezone` and the code |
| `[[tiles]]` with an action other than `call` (`compose`, `light`, `status`, ...) | ignored, reported as `non-call-tile`; the home screen is fixed and `[[tiles]]` holds only the call buttons, the first of which is who the SOS reaches |

Two things 3.2 accepted are **refused** by 4.0. Both are reported, but the effect is that school time is missing or shorter, so look for them before the child has the device:

- **German day names** (`mo-fr`, `taeglich`) in `[[quiet_hours]]`: 4.0 knows `mon-fri`, `mon,wed,fri` and `daily`. A window with another spelling is dropped (`bad-quiet-hours bad-days`). Write the English names.
- **A third window on one day**: 3.2 allowed three, 4.0 allows two per day ([quiet-hours.md](quiet-hours.md)). The windows are taken in file order and the third one of a day is dropped (`bad-quiet-hours day-full`).

Sizes: 4.0 skips a file larger than 4096 bytes. 3.2 read `settings.toml` in 4096, `contacts.toml` in 3072 and `secrets.toml` in 2048 bytes, so a file that 3.2 read in full fits. The limits of 16 contacts and 4 home networks are the same.

You can try the card before the device does: the simulator reads it (`.pio/build/sim/program --data <folder>`, with the folder holding `config/`, then the console command `log`; [simulator.md](simulator.md)) and lists what it did not understand as `config:` lines. The history `messages.log` is not read by the simulator; the host test reads it.

## 3. Flash 4.0

In a checkout of the tag `v4.0.0`, with the device connected by the USB cable of its processor:

```bash
~/.platformio/penv/bin/pio run -e device -t upload
```

(`pio` alone when it works in your shell; on the owner's Mac the one in `~/.platformio/penv/bin/` is the one that does.) Flashing is the owner's, nobody else flashes the device. Opening the serial port resets the ESP32, which is the USB bridge's habit and not a fault; read the first lines with `pio device monitor -e device`.

The card stays as it is. If you replace it, it is FAT32 with an MBR partition table (`diskutil eraseDisk FAT32 PHONEPLUS MBRFormat /dev/diskN` on macOS) and the saved files go into it.

## 4. What changes at the first start

- **The language.** 3.2 had no language setting; 4.0 reads `[interface] language` and `[sms] language`, and without them both are `en`. The interface comes up in English, **and the texts the device sends to the parents (the SOS, the answer to a location query) are English too.** To keep German, add to `settings.toml`:

  ```toml
  [interface]
  language = "de"

  [sms]
  language = "de"
  ```

  The location query `#WoBistDu` still works; `#WhereAreYou` and `#OuEsTu` work too.
- **The battery gauge.** The chip ships with a wrong battery size. At every start 4.0 compares it with `[battery] capacity_mah` (1500 unless you set it) and writes it only when it differs. The log says which: "the fuel gauge capacity is 1500 mAh, nothing to write", or, the first time on a gauge that holds another size, "the fuel gauge held design ... written 1500 mAh to both and read back". A gauge that was set to 1500 earlier says "nothing to write". It is written once, not at every start. Whether the write takes on the chip is checklist step 8.
- **School time and the contact book** are the ones of the card. The windows are in the log's report if any was refused (section 2). Only people in `contacts.toml` or on a call button may call and write; there is no switch.
- **The history and the log continue.** `messages.log` has the same format and is read as it is: a deleted message stays deleted, the read mark stays, a number that was used is not used again, and an entry that 3.2 wrote as a sentence is shown as that sentence. New entries are appended; entries 4.0 writes for events (a text that begins with `@`) are codes that only 4.0 words. `system.log` is continued; the first new line is `Boot: the runtime runs, firmware 4.0.0`. 4.0 writes no `FIRMWARE-UPDATE` line and does not use `/firmware.txt` or `/energy.log`; it leaves them alone, and you may delete them to free room.
- **The radio is off.** Whatever `[mesh]` says, a request to start the radio is answered with "not available".
- **The tool changed.** `scripts/deviceconfig.py` of 4.0 has `list`, `read`, `write`, `battery`, `sms` and `restart`; `dial`, `hangup`, `query` and `motion` of 3.2's tool are gone ([device-console.md](device-console.md)).
- **The first picture.** The display profile of 4.0 has never been seen on this panel. A picture that is inverted, striped or cut off at 120 rows is checklist steps 2 and 24, not a fault of the card.

Then go through [bring-up.md](bring-up.md) and write the dates yourself.

## 5. Going back to 3.2

1. Flash the 3.2 firmware from the repository of 3.2 (there: `pio run -e tdeckmax -t upload`).
2. **Put back the files you saved in section 1**, at least `/config/settings.toml`. 3.2 does not know what 4.0 wrote: it does not read `[radio]`, `[interface]`, `[sms]`, `[modem]` or `[battery]` (it reports them), so a radio name would be lost, and a file 4.0 rewrote has lost the comments and the keys 4.0 does not read. The saved folder is the only complete copy.
3. `messages.log` and `system.log` can stay: 3.2 reads the lines 4.0 appended, and shows an entry that begins with `@` as plain text.

The fuel gauge keeps the capacity 4.0 wrote; 3.2 does not rewrite it.
