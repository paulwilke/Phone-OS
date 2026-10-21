# The fuel gauge's data memory: what the firmware writes, and how

Code: `board::ensureEndOfDischarge()` and `board::dataMemoryBlock()` in `src/board/FuelGauge.cpp`, the bus sequence in `device/Esp32GaugeWriter.h`; tests in `test/test_fuel_gauge/` and `test/test_board_power/`. Why: decision D-42 and the lessons in [hardware-lessons.md](hardware-lessons.md) ("A night on the battery", "Two charges and a day on the table"). How the start uses it: [device-power.md](device-power.md).

The source is the BQ27220 Technical Reference Manual, Texas Instruments literature number SLUUBD4A (revision A, November 2022). Page numbers below are that PDF's. The repository does not hold the manual; the facts were taken from it on the owner's computer with their pages, and are paraphrased here.

## What is written

Four 16-bit items, all in the chip's RAM copy of CEDV profile 1 (table 3-2, p.30). Nothing else of the data memory is written by this check; the design capacity has its own (D-32).

| Item | Address | Type | Chip's default | Written |
|---|---|---|---|---|
| Gauging Configuration | 0x929B | H2, 0x0 to 0x1FFF | 0x102A | the value read, with bit EDV_CMP (0x0008) cleared |
| Fixed EDV 0 | 0x92B4 | I2, mV | 3031 | 3300 |
| Fixed EDV 1 | 0x92B7 | I2, mV | 3385 | 3420 |
| Fixed EDV 2 | 0x92BA | I2, mV | 3501 | 3470 |

What the items mean (p.12, table 1-1; p.14, table 1-2; p.56-57, table 4-11):

- The gauge watches three voltages and corrects its remaining charge at each: EDV0 is 0 %, EDV1 is 3 %, EDV2 is "Battery Low %", 7 % by default (0x9251, 700 in hundredths of a percent).
- With EDV_CMP set, the chip computes the three from the profile's cell parameters, the load and the temperature; with it clear, "the fixed values programmed in data flash" are used for all three (table 1-2). The shipped profile's parameters are for an 18650 cell and "must be matched exactly" to the cell (p.13); the formula's functions are not in the manual, so compensated thresholds cannot be worked out by hand.
- The full charge is learned at EDV2: the new value is the charge counted from nearly full down to EDV2 plus Battery Low % of the old one (p.11). An EDV2 that the cell reaches early teaches a full charge that is too small, which is what the device showed: 1372, 1107 and 988 mAh after four discharges that reached the compensated EDV2 at about 3630 mV.
- The other bits of the Gauging Configuration (FIXED_EDV0, CSYNC, SME0 in the default) are kept as read; FIXED_EDV0 only matters while EDV_CMP is set.

The three numbers, and the reason for each, are in `src/board/FuelGauge.h` next to `kEdv0Mv`: EDV0 at the firmware's switch-off (D-5), EDV1 and EDV2 where 3 % and 7 % of about 1300 mAh remain on the discharge curve measured at 61 mA, each at the lower end of what the curve allows.

## The sequence

From the manual's worked example (chapter 6, p.65-66), with the compact form of p.52 for the checksum and the length:

1. Read OperationStatus (0x3A, 0x3B). Access bits 1 and 2: 3 is sealed, 1 is full access.
2. Sealed: Control (0x00) 0x0414, then 0x3672. Not in full access: 0xFFFF twice. The chip starts unsealed but not in full access (p.65).
3. Control 0x0090, ENTER_CFG_UPDATE; poll OperationStatus until bit 0x0400 (CFGUPDATE) is set, "up to 1 second" (p.65). The firmware polls every 100 ms for up to 2 s.
4. For each item: the address to 0x3E (low byte) and 0x3F (high byte), one byte each, as the library does (p.63: above 100 kHz single-byte writes).
   - Reading: two bytes from 0x40, the high byte first.
   - Writing: two bytes from 0x40, the high byte first (the example writes 1200 mAh as 0x04, 0xB0), then the checksum to 0x60 and the length to 0x61. The checksum is 0xFF minus the low byte of the sum of the two address bytes and the data bytes; the length is the data bytes plus four. The p.52 note gives both for one example (address 0x9221, one byte 0x00: checksum 0x4C, length 5), and SensorLib's capacity write uses the same formula with length 6. The chip takes the block into its RAM when the checksum and the length arrive (p.65, step 13).
5. Leave: after a write Control 0x0091 (EXIT_CFG_UPDATE_REINIT), as SensorLib does; after a read alone 0x0092 (EXIT_CFG_UPDATE, no re-initialisation), because nothing changed. Poll until CFGUPDATE is clear, up to 3 s.
6. A chip that was sealed is sealed again: Control 0x0030.

A start reads the four items (one update session), writes those that differ (a second) and reads them again (a third). The decision which items to write is `ensureEndOfDischarge()`'s and is host-tested on a fake; `dataMemoryBlock()` builds the bytes of step 4 and is tested against the numbers above. A chip that does not enter the update mode is left again at once (the library leaves it in the mode, unsealed, until the chip's own four-minute timeout, p.52).

The write goes to RAM: "The written data is not persistent, so a POR does resolve the fault" (p.27). The design capacity is written the same way and has held across the device's switch-offs, which disconnect the battery from the board but not the gauge from the cell. Because the check compares at every start, a chip that lost the values gets them again at the next start.

## What is not known

- Which exit makes new thresholds count, 0x0091 or 0x0092: the manual allows either (p.66). The firmware uses the library's 0x0091 after a write.
- Whether a block can be read outside the update mode: not said, so the firmware reads inside it, as the library does.
- What "C" is in the manual's "No EDVs are set if Current < C/32" (p.12) and "Current remains >= 3C/32 when EDV2 is reached" for learning (p.11). If it is the design capacity, the thresholds act from about 47 mA and learning needs about 141 mA at EDV2; the device ran at 61 mA when it learned 988 mAh, so the reading is uncertain. Below that current the gauge follows its count alone.
- Whether the three must be in order (EDV0 below EDV1 below EDV2): implied by the figures, not stated. The firmware's are in order.
- The manual names two unseal keys: 0x8000 twice in section 3.3 (p.27-28) and 0x0414, 0x3672 in the example (p.65). The firmware uses the example's, which the library uses and which has worked on the device for the capacity.
- With the compensation off, the thresholds do not move with the load: with the modem awake (220 mA) the cell's voltage is about 60 mV lower, so the gauge reaches each threshold earlier than at 61 mA.
- Not verified on the device: that the chip accepts the write, holds it, and that a discharge past 3.63 V no longer falls to 7 %.
