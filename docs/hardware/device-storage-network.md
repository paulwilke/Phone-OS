# The device's card, Wi-Fi and web page

Code: `src/board/` (`SdVolume`, `SdCard`, `SdConfigStore`, `SdEventLog`, `SdMessageLog`, `WifiRadio`, `BoardWifi`, `TcpListener`, `BoardTcpServer`), `src/hal/MessageLog.h`; the device's part in `device/Esp32SdVolume.h`, `Esp32Spi.h`, `Esp32WifiRadio.h`, `Esp32TcpListener.h`, `FlashAssets.h`; tests in `test/test_sd_card/`, `test_sd_config_store/`, `test_sd_logs/`, `test_board_network/` and, for the `ConfigStore`, `EventLog`, `MessageLog`, `Wifi` and `TcpServer` contracts, `test/test_hardware_contracts/` (`testing/BoardStorageRigs.h`, `testing/BoardNetworkRigs.h`). The scripted parts are `FakeSdVolume` (an in-memory FAT volume that runs full, fails halfway, loses power at a chosen step, is pulled out), `FakeWifiRadio` and `FakeTcpListener`.

Everything above the three seams (`SdVolume`, `WifiRadio`, `TcpListener`) is plain C++ and tested on the host; the device implements the seams with the Arduino libraries and nothing else. **Compiled and tested on the host is not verified on the device.**

## The card

`SdCard` mounts the volume at a descending bus clock, 4 MHz, 1 MHz, 400 kHz: the card hangs on the bus that the display and the radio share. All three chip selects are held high first, there are 100 ms of quiet before the first try and 250 ms before each further one, and the volume is **released between the attempts**, because a library that is not released keeps its failure state and does not ask the card again (an intact card was "not found" for that reason on 2026-09-30). There is exactly one SPI object for the shared bus (`device/Esp32Spi.h`).

**A missing or unreadable card is not an error of the device.** It is said once in the log ("not found or not readable (FAT32 with an MBR partition table is read, a GPT card is not); the device runs on its defaults"), every store fails cleanly (read and size say "no such file", writes are false), the log goes to the console and waits in memory, and `poll()` asks again every 30 seconds, so a card pushed in later is found. A card that fails three operations in a row is taken for pulled out. Formatting: macOS formats with GPT by default, which this card reader cannot read; `diskutil eraseDisk FAT32 PHONEOS MBRFormat /dev/diskN` makes one it can.

## The files

`SdConfigStore` keeps the promises of the `ConfigStore` capability on top of the plain volume. **A write that fails leaves the old content intact**: the data goes to a scratch file (the name plus `.tmp`, reserved by SafePath) and is checked to be complete; then the old file is removed and the scratch file renamed (FAT does not rename over an existing file).

| Where the power goes | What the card holds | What `recover()` does at the next start |
|---|---|---|
| before the scratch file is complete | the old file and a half scratch file | deletes the scratch file |
| after the scratch file, before the removal | the old file and a complete scratch file | deletes the scratch file (the old content stays) |
| between the removal and the rename | **no target** and a complete scratch file | renames it into place (the new content) |
| after the rename | the new file | nothing |

`test_sd_config_store` cuts the power after every step of a write and asserts that the file is then the old content or the new one, never neither and never with a scratch file left. A rename that fails twice reports the write as failed and leaves the new content in its scratch file for `recover()`.

Further rules, each tested: a write does not start when the card has less room than the data and a reserve of 8 KB, and then touches nothing; one missing directory level is created and two are not; a read takes at most the bytes asked for, in blocks of 1 KB, and a card that fails in the middle gives "no such file" and not half a file; a listing hides scratch files and names that SafePath would refuse (the page could not use them); directories are neither removed nor read as files; every path is checked with SafePath first.

## The logs

`SdEventLog` writes `/system.log`: `<epoch> <HH:MM> <text>` with the local time of day, or `- --:-- <text>` while the time is not known, never 1970. **A line that must be provable is not lost because the card was busy or missing**: it waits in memory (64 lines at most, and when more were thrown away one line says how many) and is written, in order, as soon as the card allows. The log does not eat the card: below 32 KB free the lines wait instead of taking the last room that a configuration write needs, and at 256 KB the file becomes `/system.old.log` (replacing the older one), so the two never exceed 512 KB. Every line also goes to the console at once. A card that failed in the middle of a line leaves a piece of it; a line break first keeps that piece from joining the next line, so it is a line of its own.

`SdMessageLog` is the history's file `/messages.log` (the `MessageLog` capability, moved into `src/hal/` so that the board can implement it): appended to and never rewritten, so a power loss costs at worst the last line. A line that cannot be written is `false` (the store counts it and keeps the message on the screen), also below 32 KB free. Reading hands every line over without its line break, accepts `\r\n`, skips empty lines and hands a line of more than 511 bytes over cut, so that it counts as damaged in the store and does not vanish. The file is read in blocks of 512 bytes and the volume opens it anew for every block; a block that fails is read once more, and when it fails again the read stops and says so (`Read::StoppedEarly`, not `Missing`, even for the first block), with the lines before it handed over. The store's own behaviour with damaged lines is in [message-store.md](../core/message-store.md); the two work together in `test_sd_logs`.

## Wi-Fi and the web server

`BoardWifi` is the `Wifi` capability on a `WifiRadio`: names of 1 to 32 characters, an access point's password of 8 to 63, a station's password of at most 63 (empty means an open network), starting one mode shuts the chip down before the other comes up, a start that is refused does not touch the radio and leaves what ran running, an access point that is valid but does not start powers the chip down and leaves the state Off. The device's radio (`Esp32WifiRadio`) stores nothing in the flash, uses channel 6, does not hide the network and takes at most two stations.

`BoardTcpServer` is the `TcpServer` capability on a `TcpListener`: one connection at a time; accepting the next client closes the one before; `read` gives 0 for "nothing now", -1 once the other side has gone and its bytes are used up (and after `close`), and `write` gives the bytes taken, 0 after close or when the other side is gone.

The parents' page is part of the firmware: `board_build.embed_txtfiles = web/index.html`, handed to the web server by `FlashAssets`. `web/index.html` is generated by `scripts/gen_web.py` and CI checks that it is current (`gen_web.py --check`), so the page in the firmware is the page of the repository.

## Not verified on the device

The card at 4 MHz on the shared bus and the fallback clocks, the Arduino library's `rename`, `remove` and `usedBytes` on a real FAT volume (the sequence is the tested one, the library's behaviour is not), the time `usedBytes` takes on a large card, the access point and the station against real networks, and the page being served from the flash.
