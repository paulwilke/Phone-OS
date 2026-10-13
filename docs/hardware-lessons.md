# Hardware lessons: the T-Deck Pro MAX

Things that went wrong on the real device, why, and the rule that keeps them from coming back. Each entry says what was measured, not what was assumed. The device drivers implement every rule marked **Driver rule** ([hardware-drivers.md](hardware-drivers.md) names the test of each); the host tests cannot prove them on the device, so each rule also names how the owner checks it there ([bring-up.md](bring-up.md)).

## The fuel gauge ships with the wrong battery size

**What happened.** The device seemed not to charge: the status page said "full on the cable" next to 56 %, and the battery bar and the percentage disagreed. A read-only diagnosis of both chips (2026-10-06) showed that the charger was fine and the battery full (cell 4.16 V, charge done, target 4.208 V), but the fuel gauge (BQ27220) held its factory design capacity of **3000 mAh** and a full-charge capacity of 3000 mAh, with a cycle count of 0. The battery is **1500 mAh**. The gauge therefore reported about half the real charge (remaining 1655 mAh, more than the battery can hold) and a time to empty twice as long as the truth (2858 min at 35 mA is 1667 mAh, which is 56 % of 2977). Writing 1500 mAh into both registers once made it read 97 % at 4.13 V.

**Why it is easy to miss.** Every value comes from one chip and looks plausible. Nothing shows an error; the percentage only disagrees with the cell voltage, and nobody compares the two.

**Driver rule.**

1. At start, read the gauge's design and full-charge capacity. When they differ from the configured battery capacity (`[battery] capacity_mah`), write the configured value to both and read it back; write only on a difference, so the chip's memory is not worn by every start.
2. Never show a percentage the voltage contradicts: a gauge that says "less than 70 %" while the cell is above 4.1 V and the charger reports the charge as done is wrong, and the interface must show the state it can trust (the charger's "full") instead of the number.
3. A diagnostic command reads both chips without writing (gauge: voltage, remaining, full-charge and design capacity, cycle count, state of charge; charger: all registers, input current limit, charge state, battery voltage) so that nobody has to flash a special program to find out.

**Owner's check.** After a full charge the gauge reads 100 % within a few percent of the cell voltage chart, and the diagnostic command shows the configured capacity.

## The charger charges with its default unless the firmware says otherwise

Measured 2026-10-12: the SY6970 held a charge current of 2048 mA (the chip's default, more than 1 C for a 1500 mAh cell), an input limit of 500 mA, and a watchdog of 40 seconds that nobody fed, so the fault register read 0x90 (watchdog) and the chip fell back to its defaults every 40 seconds. On the computer's USB port the current stayed near 500 mA because of the input limit; on a mains adapter it would not have. **Driver rule:** at start, limit the charge current to 0.65 C of the configured capacity and switch the watchdog off, reading back what was written ([device-power.md](device-power.md), decision D-14). **Owner's check:** `battery` shows the limited current and "watchdog off".

## The IMS bearer needs a default context of type IPV4V6

Outgoing calls hung the modem two seconds after dialling when the IMS was not registered. Defining PDP context 1 as IPV4V6 and restarting the radio fixed it. The modem driver sets this up itself at start (read the contexts first, write only when they differ) and refuses to dial while the IMS is not registered. How, what is proven and the owner's checklist for the device: [volte.md](volte.md).

## Smaller facts that cost time

- **The charger changed during the production series.** Newer boards carry an SY6970 on I2C address 0x6A, older ones a BQ25896 on 0x6B. A driver that looks at only one address treats the bus as broken.
- **"Cable present" is not "charging".** The charger's power-good bit says a supply is attached; the charge state says it is charging. A full battery on a cable reads "not charging". Decisions about sleep must use the cable bit.
- **The USB console disappears in light sleep.** The USB bridge of the ESP32-S3 is switched off while it sleeps, the connection drops and the device cannot be flashed until it wakes; nothing sleeps while a charger is attached.
- **A zero-length USB write means "buffer full", not "nobody there" (seen 2026-10-07; fix host-tested, not yet verified on the device).** With the transmit timeout at zero, HWCDC takes only what fits its small buffer; a long answer needs retries over milliseconds, and a write larger than the free room is reported as written while the rest is dropped, so the port offers only the free room. Details: [device-console.md](device-console.md).
- **Opening the USB serial port resets the ESP32.** A tool that reads logs over USB restarts the device.
- **A bad SIM contact looks like "no network".** The modem restarts every 30 to 60 seconds, which also stresses the SD card that shares the tray.
- **LTE-only mode gives "no network service" for calls** on this module; voice needs the IMS registration.

## How to add a lesson

A lesson belongs here when it was measured on the device and cost real time. State the measurement, the cause, the rule for the drivers, and how the owner checks it. A lesson that is only a guess does not belong here.
