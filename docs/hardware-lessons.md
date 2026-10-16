# Hardware lessons: the T-Deck Pro MAX

Things that went wrong on the real device, why, and the rule that keeps them from coming back. Each entry says what was measured, not what was assumed. The device drivers implement every rule marked **Driver rule** ([hardware-drivers.md](hardware-drivers.md) names the test of each); the host tests cannot prove them on the device, so each rule also names how the owner checks it there ([bring-up.md](bring-up.md)).

## The fuel gauge ships with the wrong battery size

**What happened.** The device seemed not to charge: the status page said "full on the cable" next to 56 %, and the battery bar and the percentage disagreed. A read-only diagnosis of both chips (2026-10-06) showed that the charger was fine and the battery full (cell 4.16 V, charge done, target 4.208 V), but the fuel gauge (BQ27220) held its factory design capacity of **3000 mAh** and a full-charge capacity of 3000 mAh, with a cycle count of 0. The battery is **1500 mAh**. The gauge therefore reported about half the real charge (remaining 1655 mAh, more than the battery can hold) and a time to empty twice as long as the truth (2858 min at 35 mA is 1667 mAh, which is 56 % of 2977). Writing 1500 mAh into both registers once made it read 97 % at 4.13 V.

**Why it is easy to miss.** Every value comes from one chip and looks plausible. Nothing shows an error; the percentage only disagrees with the cell voltage, and nobody compares the two.

**Driver rule.**

1. At start, read the gauge's design and full-charge capacity. When the design capacity differs from the configured battery capacity (`[battery] capacity_mah`), write the configured value to both and read it back; write only on that difference, so the chip's memory is not worn by every start. The full-charge capacity is left to the gauge once the design is right: it learns it from a discharge (1372 mAh after the night of 2026-10-13), and writing the setting over it at every start undid the learning (decision D-32).
2. Never show a percentage the voltage contradicts: a gauge that says "less than 70 %" while the cell is above 4.1 V and the charger reports the charge as done is wrong, and the interface must show the state it can trust (the charger's "full") instead of the number.
3. A diagnostic command reads both chips without writing (gauge: voltage, remaining, full-charge and design capacity, cycle count, state of charge; charger: all registers, input current limit, charge state, battery voltage) so that nobody has to flash a special program to find out.

**Owner's check.** After a full charge the gauge reads 100 % within a few percent of the cell voltage chart, and the diagnostic command shows the configured capacity.

## The charger charges with its default unless the firmware says otherwise

Measured 2026-10-12: the SY6970 held a charge current of 2048 mA (the chip's default, more than 1 C for a 1500 mAh cell), an input limit of 500 mA, and a watchdog of 40 seconds that nobody fed (register 07 read 0x9D), which on this family of chips sets the settings back to their defaults when it runs out. The fault register read 0x90; after the fix, with the watchdog off, it still reads 0x90 (checked on the device on 2026-10-12), so that value is not a watchdog fault and its meaning on the SY6970 is not known. On the computer's USB port the current stayed near 500 mA because of the input limit; on a mains adapter it would not have. **Driver rule:** at start, limit the charge current to 0.65 C of the configured capacity and switch the watchdog off, reading back what was written ([device-power.md](device-power.md), decision D-14). **Owner's check:** `battery` shows the limited current and "watchdog off" (seen on the device on 2026-10-12: 960 mA, register 07 = 0x8D).

## The IMS bearer needs a default context of type IPV4V6

Outgoing calls hung the modem two seconds after dialling when the IMS was not registered. Defining PDP context 1 as IPV4V6 and restarting the radio fixed it. The modem driver sets this up itself at start (read the contexts first, write only when they differ) and refuses to dial while the IMS is not registered. How, what is proven and the owner's checklist for the device: [volte.md](volte.md).

## A night on the battery: the state reads the chips every pass, and the gauge empties at 3.6 V

**What was measured** (2026-10-13 to 14, firmware 4.2.2, modem asleep, radio off, no Wi-Fi, the device locked). The energy log: 12.8 hours from 4099 mV (gauge 91 %, 1363 mAh) to the switch-off at 3291 mV, at 100 to 113 mA the whole night, about 1340 mAh. The loop line every half hour: 32,000 passes in 1800 s, the power policy allowed sleep in 100 % of them, the processor slept 1 % of the time, and of each 56 ms pass the application step cost 52.0 ms, the keys 2.8 ms, the rest under 0.5 ms. The gauge fell from 45 % (663 mAh) to 7 % (93 mAh) between two lines at 3629 and 3623 mV, said 0 % from 3596 mV on, and the cell gave five more hours; the rule of D-24 showed the voltage's number from 20:19 (one line) until 00:55 (one line). After the charge the gauge reports a learned full charge of 1372 mAh.

**What it means.** The 105 mA are the processor: the modem's sleep saves what it should, but the loop never rests, because `Runtime::followCalls` builds the core's whole state every pass, and that state reads the fuel gauge and the charger over I2C at 100 kHz five times (about thirty transactions, the 52 ms); and a light sleep that was allowed and lasted 1 % of the time ends at once, which points at a wake line that is held low (the keyboard's, the touch chip's or the modem's ring line). The gauge takes this cell for empty at about 3.6 V, which is why D-24 exists, and it has now learned a full charge from a real discharge.

**Driver rule.** D-31: the state is not built every pass, and a reading of the battery chips is kept for a second. D-32: the learned full charge is left to the gauge. D-33: the sleep is measured (which line was low before it, what ended it). All three in 4.2.4. **Owner's check:** the loop line after 4.2.4 says the step costs a few milliseconds and what ends the sleep; the start line says "nothing to write; it has learned a full charge of ... mAh".

## The second night: the wake lines idle low, so no sleep lasts

**What was measured** (2026-10-14 to 15, firmware 4.2.4, the same conditions). The step costs 0.4 ms of a pass (52.0 the night before: D-31 holds), the loop makes 398,000 passes in 1800 s, sleep is allowed in every one, and the new sleep part of the line says: 398,000 sleeps, none ended by the timer, none by a line, all "other"; before every one of them the keyboard's interrupt line and the modem's ring line read low, the touch chip's high. "Asleep 23 %" with the current unchanged at 103 to 114 mA is the time of going down and coming up 220 times a second. The battery: 12.2 hours from 99 % to the switch-off at 3291 mV. The gauge: "nothing to write; it has learned a full charge of 1372 mAh" at the start (D-32 holds), cycle count 1, and once more from 52 % to 7 % in ten minutes at 3.63 V with the voltage's number shown from 01:28 to 06:01 (D-24 holds).

**What it means.** A light sleep wakes on a level. Two of the three lines were armed to wake on low and were low already: the keyboard chip's interrupt is open-drain and the board gives it no pull-up, and the ring line idles low too, whether by the modem's polarity or for the same reason. So every sleep ended before it began, and the processor never rested.

**Driver rule.** The three wake lines get the processor's pull-ups at start (`board::powerUp`); `device/Sleep.h` arms each line with the level it does not have at that moment; a sleep that nothing ended counts as no sleep; the start logs "Wake lines: keyboard ..., touch ..., ring ..." (decision D-34). **Owner's check:** the start line after 4.2.5 shows the idle levels (high is what the pull-ups should give the keyboard's line); after a night the loop line says most sleeps were ended by the timer, "asleep" is most of the half hour, and the energy log's current is well under 105 mA.

## The third night: the processor sleeps, 62 mA remain

**What was measured** (2026-10-15 22:22 to 2026-10-16 07:40, firmware 4.2.5, locked, modem asleep, Wi-Fi off). The start line: "Wake lines: keyboard high, touch high, ring high". Every loop line of the night: 882 to 883 passes in 1800 s, asleep 97 to 98 %, sleep allowed in every pass, every sleep ended by the timer, no line low before a sleep; per pass keys 2.9 ms, step 32.6 ms, housekeeping 4.0 ms (a pass is now about two seconds long and holds one frame and one battery reading, so the per-pass figures grew while the work per half hour fell from 160 s to 29 s). The energy log: 58 to 63 mA, 61.9 mA on average from the slope, 1248 to 679 mAh in 9.19 hours, 92 % to 50 %. A call at 07:40 after nine hours of modem sleep went out at once ("Call: dialling", "ringing at the other end"). The display: five long passes in the night, three partial refreshes (the date at midnight, two bar changes) and two full refreshes of four-hour-old pictures. Switched off from 07:42 to 13:08 the gauge lost 1 mAh: the off state draws nothing that matters.

**What it means.** D-34 holds: the pull-ups and the arming on the opposite level made the light sleep last. The processor is awake about 2 % of the time, about 1 mA on average, so the remaining 55 to 60 mA are elsewhere: the modem in its sleep (`AT+CSCLK=1` and DTR high, a few mA by the manual, tens if the sleep does not take), the display controller, the card, the keyboard and touch chips, the gauge and the charger, or a rail the firmware believes off (the keyboard backlight pin 42 is defined and never driven). Nothing in the logs tells them apart. By projection the cell gives about 21 hours from full to the switch-off, against 12 to 13 before.

**The charge at the adapter the day before** (2026-10-15, 14:04 to 22:22): 516 falling to 420 mA, input-limited to 500 mA; termination after 3 h 10 min at 4200 mV and 255 mA; afterwards the cell fed the board with 35 mA while the adapter was attached, and a top-up started whenever the cell fell to about 4.10 V, five times in five hours; the gauge stayed between 90 and 93 % and never said 100 % (open point 26).

**Driver rule.** None new. **Owner's check:** open point 25 (the ammeter) and 26 (the charger after "done").

## Smaller facts that cost time

- **The charger changed during the production series.** Newer boards carry an SY6970 on I2C address 0x6A, older ones a BQ25896 on 0x6B. A driver that looks at only one address treats the bus as broken.
- **"Cable present" is not "charging".** The charger's power-good bit says a supply is attached; the charge state says it is charging. A full battery on a cable reads "not charging". Decisions about sleep must use the cable bit.
- **The USB console disappears in light sleep.** The USB bridge of the ESP32-S3 is switched off while it sleeps, the connection drops and the device cannot be flashed until it wakes; nothing sleeps while a charger is attached.
- **A zero-length USB write means "buffer full", not "nobody there" (seen 2026-10-07; fix host-tested, not yet verified on the device).** With the transmit timeout at zero, HWCDC takes only what fits its small buffer; a long answer needs retries over milliseconds, and a write larger than the free room is reported as written while the rest is dropped, so the port offers only the free room. Details: [device-console.md](device-console.md).
- **The switch on the side of the case restarts the device on a long press, and the firmware cannot read it (2026-10-12).** It is on the charger chip. The charger's control register reads 0x44 on the device: bit 2 is set, which on the BQ25896 map that the SY6970 follows is the chip's own reset function (a long press resets the board, which looks like a restart). The key is not a processor input, so nothing can be done with it in the firmware; switching off is the Power tile. Not checked against the SY6970's data sheet, which is not available here.
- **A text of more than a few hundred bytes over the USB console is lost in part (2026-10-12).** `CONFIG WRITE` of a file of 626 bytes stored 512 of them or got no answer in five tries, though the tool sends 64 bytes every 10 ms and waited 40 seconds after opening the port; an `sms` command line of 241 bytes got no answer, one of 186 bytes went through. Reading works. Cause: the receive buffer of the USB port is the default 256 bytes (only the transmit buffer was enlarged), and the loop reads it once in about 56 ms. Fixed in 4.2.2 (receive buffer 4096 bytes) and verified on the device on 2026-10-13 with a 615-byte file.
- **A text queued by `deviceconfig.py sms` is lost if the port is opened again before it has gone out.** Opening the port resets the processor, and the modem needs about a minute to register after a start. Wait for the log line "Messages: sent" before the next call of the tool.
- **Opening the USB serial port resets the ESP32.** A tool that reads logs over USB restarts the device.
- **A bad SIM contact looks like "no network".** The modem restarts every 30 to 60 seconds, which also stresses the SD card that shares the tray.
- **LTE-only mode gives "no network service" for calls** on this module; voice needs the IMS registration.

## How to add a lesson

A lesson belongs here when it was measured on the device and cost real time. State the measurement, the cause, the rule for the drivers, and how the owner checks it. A lesson that is only a guess does not belong here.
