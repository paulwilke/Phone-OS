# Bring-up: the owner's checklist for the first start

Everything in [hardware-drivers.md](hardware-drivers.md) is compiled and tested on the host and **none of it is verified on the device**. This list is how that changes: go through it in order with a USB cable, a SIM card and the SD card, write the date and the result in the columns, and report what differs from "expect". **The firmware is built with `pio run -e device` and flashed by you** (`pio run -e device -t upload`); nobody flashes your device but you. A step that fails stops the steps that depend on it: the order is the order of the dependencies.

Before the first start: format the card FAT32 **with an MBR partition table** (macOS: `diskutil eraseDisk FAT32 PHONEPLUS MBRFormat /dev/diskN`); put the files of `config/examples/` into `/config/` with the values you want; keep the USB cable at hand. Read the console with `pio device monitor -e device` (opening the port resets the device; that is the USB bridge's habit and not a fault).

**Known before you start:** the display is driven with the GDEQ031T10 profile, whose values come from the vendor's documents and are checked against them on the host only ([device-display.md](device-display.md)). **No picture has ever been seen on this panel from this firmware**, so steps 2 and 24 are the first real test of it. Everything else can be checked on the console.

| # | Step | Expect | Date | Result |
|---|---|---|---|---|
| 1 | Flash and start. Read the first console lines | `Board: expander answers, started`; `SD card: ready`; `Keyboard: answers`; `Touch: CST66xx found`; `Display: profile GDEQ031T10 on UC8253, OTP waveform`; `Boot: the runtime runs` | | |
| 2 | Boots and shows a picture | a complete, upright, readable first picture (a full refresh with several flashes, about 3 s); not inverted (black text on white, not the reverse); no stripes, no cut-off at 120 rows; no console line "the panel stayed busy" | | |
| 3 | Keyboard: type letters, Shift, Alt then a letter, Enter, Backspace, the `$` key, the mic key | the input is traced; letters are text, Enter select, Backspace back, `$` up, mic down; the symbol of the layer is the one printed on the key; no letter comes twice | | |
| 4 | The three keys under the screen, left to right | home, inbox, compose (once for each press) | | |
| 5 | Touch: tap in the four corners and the middle | one touch for each tap; the corners are `0,0`, `239,0`, `0,319`, `239,319` within a few pixels; nothing mirrored | | |
| 6 | Haptic: trigger each pattern (a text to the device, a call, the self-test page once there is a picture) | click, ring (long), message (two short), sent (three short), failed (short, long) are different in a pocket | | |
| 7 | Battery: `python3 scripts/deviceconfig.py battery` on battery and on the cable | the gauge's voltage, current and capacity are shown; the verdict line says the percentage agrees with the voltage | | |
| 8 | After the first start the gauge's capacity is 1500 mAh | `battery` shows design and full-charge capacity 1500; the log says the capacity was written once; the second start says "nothing to write" | | |
| 9 | Charge state on the cable | cable in: the charger line says "power good yes" and the phase (charging or charge done); cable out: "power good no"; a full battery on the cable is "charge done", not "no cable" | | |
| 10 | Nothing sleeps with a cable | with the cable in, the console stays alive for ten minutes of no input | | |
| 11 | GNSS fix outdoors | the receiver is on while a position is wanted; a fix within a few minutes; `[gnss]`-lines or the position page show satellites and an HDOP; the clock epoch is set from it | | |
| 12 | SMS in and out | `python3 scripts/deviceconfig.py sms <your number> "test"` arrives; a text to the device shows in the history and in `messages.log` | | |
| 13 | A call out and a call in | out: the modem dials, the sound goes to the modem, the amplifier is on only during the call; in: it rings and vibrates. The log shows `+CIREG: 1,1,15` before the first call (VoLTE: [volte.md](volte.md)) | | |
| 14 | The card: read the configuration | `deviceconfig.py list` and `read settings.toml` show your files; a wrong value in a file is reported in the log and the device still starts | | |
| 15 | The card: an atomic write | `deviceconfig.py write settings.toml <file>`: `CONFIG OK`; pull the USB cable in the middle of a second write: the file is the old or the new one after the restart, never empty | | |
| 16 | The card: the log | `deviceconfig.py read system.log` shows the lines of this session with a time once the network time is known and `- --:--` before | | |
| 17 | No card | start once without the card: the log says "SD card: not found …", the device runs on its defaults and does not crash; push the card in: it is found within 30 s | | |
| 18 | Wi-Fi and the parents' page | switch the access point on (page "Wi-Fi" once there is a picture, or `webbridge.py` over the cable meanwhile); the page answers, asks for the password, the lock-out works after five wrong ones | | |
| 19 | The page over the cable | `python3 scripts/webbridge.py`, open `http://localhost:8081`, log in: the real settings are shown | | |
| 20 | Sleep and wake | on battery, unlocked and idle: the processor sleeps (the console goes quiet), a key press, a touch and an incoming call wake it | | |
| 21 | A restart by the modem | with a bad SIM contact or by `AT+CRESET` through a diagnostic: the log says "Modem restarted by itself (n since start)" and the device recovers; the power key is **not** pressed on a running modem | | |
| 22 | Switch off | on battery: the switch-off asks the modem to say goodbye, then the device is off; on the cable it refuses and says so | | |
| 22a | Restart from the status page | press r twice on the Status page, on battery and on the cable: the log shows "Restart requested on the device", the device restarts and comes back with its history | | |
| 23 | The motion page | the sensor starts (about 11 s), flat on the table z reads about −0.95, the ball rolls to the lower side | | |
| 24 | Refreshing | the first frame is a full refresh, complete; a small change (move the tile selection) is a partial refresh that **changes only the changed area** and does not flash; no inversion in either kind; every tenth change is a full refresh and clears the ghosts; ten partial refreshes in a row leave no unreadable ghosting (the datasheet advises a full one after five: report what you see); a picture left alone for 4 hours is refreshed in full once; the refresh times (note them); the picture also looks right when the device is cold | | |
| 25 | Voice over LTE, the broken state: with a tool on the modem, `AT+CGDCONT=1`, `AT+CFUN=0`, `AT+CFUN=1` | `+CIREG: 1,0,0`; context 8 as `"IP"` | | |
| 26 | Voice over LTE, the fix: flash the firmware | context 1 `IPV4V6`; `+CIREG: 1,1,15` | | |
| 27 | Dial from the device | `+CLCC` 2, 3, 0; the call rings and connects | | |
| 28 | Cold start of the modem, repeat 26 and 27 | the same | | |

Report back **what differs**, with the console lines. A failed step is a finding; the driver that matches it is in [hardware-drivers.md](hardware-drivers.md). The other open points of the project are in [open-points.md](open-points.md).
