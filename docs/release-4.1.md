# Release 4.1

Version 4.1.7 of Phone OS (4.1.7: the modem looks for its home network again every 15 minutes while roaming; 4.1.6: the Lock button on the home screen, the GPS note in the middle of the status bar; 4.1.5: an SOS with an umlaut is one message; 4.1.4: the battery percentage from the voltage when the gauge fails, the voltage on the status page; 4.1.3: the energy log is back; 4.1.2: the new name; 4.1.1: the last known position in an SOS without a position, the splash picture, the picture "Switched off"). It tidies the interface after the owner's test of 4.0 on the device (2026-10-07/08) and builds on 4.0.2. What changed is in [../CHANGELOG.md](../CHANGELOG.md); the acceptance test the owner goes through on the device each time, with a column for the date and the result, is [de/abnahmetest.md](de/abnahmetest.md). Everything about 4.0 that this page does not change ([release-4.0.md](release-4.0.md): no radio, no published binary, going back to 3.2) holds for 4.1 too.

## What changes for the parents

- **The settings are on three tabs.** **Basics** (the child's name, the languages, the features, the remote queries, the call buttons), **School time** (the windows) and **Advanced** (time zone, log level, Wi-Fi idle time, battery, modem, map link, radio, the tiles of the home screen; most parents leave these as they are). Each tab has its own **Save**; saving one keeps what the others hold. A change that is not saved says "Unsaved changes" and stays when you look at another tab.
- **The texts' language is kept.** Saving the settings in 4.0 wrote the interface language into the language of the texts the device sends; 4.1 reads every field from its own section.
- **School time is ticked.** Each window has seven boxes Mo to Su, then from and to and "active" in one row; **Add** adds a window, for example Monday and Tuesday afternoon.
- **Who gets the SOS is ticked.** Each contact has a box **SOS**; the SOS goes to every ticked contact with the same text. Without any tick it goes to the first call button, as in 4.0.
- **The home screen's second page** can be thinned out: each of its tiles has a box on the **Advanced** tab. Without the **Wi-Fi** tile the parents' page is still reached over the USB cable (`scripts/webbridge.py`).
- **Files**: what a Mac leaves on the card is hidden (tick **Show hidden files** to see it), **Remove Mac leftovers** deletes its `._` files, and the Mac's folders are named for a computer to delete. `firmware.txt` is marked as a file of 3.2 that may go (`energy.log` was too until 4.1.3, which writes it again).
- **Backup** saves `phone-os-backup.zip` with the settings and the contacts exactly as they are (never the passwords), and **Restore** takes it back. Restoring did not work from the page of 4.0. A backup made with 4.0 can still be restored once.

## What changes for the child

- **Back is always in the same place**, on the left of the key bar, which is a little taller.
- **The home screen** has no arrows: the tiles are touched, and a tap on the left or right half of the strip under the tiles shows the first or the second page. The keys ▲ ▼ still go through every tile. **Light** is on the first page.
- **In a call**, **Hang up** is the whole key bar, and **Quieter** and **Louder** are buttons on the screen.
- **Writing with free text**: the child types at once; while nothing is typed the right key is **Templates**, the ready-made messages.
- **Options** has **Switch off** (press twice); with the cable plugged in it says to unplug first.

## What changes with the new name (4.1.2)

The project is called **Phone OS** from 4.1.2 on; it changes names, not behaviour.

- **The splash picture** says "Phone OS".
- **The parents' page** is titled "Phone OS". A page the browser kept from an earlier version has to be **reloaded once**: the device now asks every change for the header `X-Phone-OS` instead of `X-Phoneplus`, and the old page sends the old one, so its saves are refused with 403 until it is reloaded.
- **Backup** saves `phone-os-backup.zip`; a backup saved by 4.1.0 or 4.1.1 under the old file name is restored as before.
- **The default network names** are `phone-os`: the device's own Wi-Fi (`wifi.access_point.ssid`), its name in the home network (the host name) and its name in the radio network (`radio.name`). **A name already saved on the card stays as it is**: a card of 3.2 or 4.x that holds `ssid = "phoneplus"` keeps that name. Only a card without the key gets the new default.

## What was checked, and where

| Kind | What | Where |
|---|---|---|
| Tested on the host | every rule above, each with a counter-check (the old behaviour put back, the test fails) | `pio test -e native`, `test/test_web_page/test.mjs`, `test/test_schema_gen/test_gen.py` |
| Run in the simulator | every scenario; the parents' page in a real browser (tabs, saving, the backup checked with `unzip`) | `pio run -e sim` |
| Compiled | the firmware | `pio run -e device` |
| Verified on the device | the network name and "Connecting ..." of 4.0.2 (2026-10-08); nothing of 4.1 yet | [de/abnahmetest.md](de/abnahmetest.md) |

## What the owner tries on the device

The acceptance test [de/abnahmetest.md](de/abnahmetest.md) goes through all of it in order. The parts only the device can show: whether the wider page dots and the volume buttons are easy to hit with a finger, whether the taller key bar leaves every page readable, switching off from the options (and that the side key starts the device again), an SOS to two ticked contacts, the backup and the restore over Wi-Fi, and the parents' page loading its words over Wi-Fi in all tabs.

## Decisions that belong to the owner

- **Deleting a folder from the page.** The Mac's folders (`.Spotlight-V100`, `.fseventsd`, `.Trashes`, `.TemporaryItems`) can only be deleted on a computer; the device has no route that deletes a folder. A route for it would be limited to exactly these four names, to folders that are empty once the `._` files inside are gone, and to one level below them; it is not built until the owner decides.
- **The backup size.** One request body holds 8192 bytes (`http::kMaxBody`) and a file 4096 (`kMaxConfigFile`), so an archive of two full-size files cannot come back; the usual files are well under 1 KB each. The limits are unchanged.
