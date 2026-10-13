# The battery numbers

Code: `src/board/BatteryPlausibility.{h,cpp}` (the rules), `src/board/BoardPower.cpp` (the readings), `src/hal/Power.h` (the marks of the cell), `src/ui/screens/StatusScreen.cpp` (the page); tests in `test/test_battery_plausibility/`, `test_board_power/`, `test_screen_status/`. The chips are in [device-power.md](device-power.md); the lesson of the wrong battery size is in [hardware-lessons.md](hardware-lessons.md).

## Where the percentage comes from

The fuel gauge (a BQ27220) counts charge, but it only counts well once it has learned the battery, and this one has not (cycle count 0). Two rules stop the interface from showing a number the voltage contradicts:

1. **A low number against a high cell** ([device-power.md](device-power.md)): the gauge says less than 70 % while the cell is above 4.1 V or the charger says "charge done". The number is withheld.
2. **A very low number against a middling cell, on the battery** (since 4.1.4, changed in 4.2.2): the gauge says 30 or more points less than the voltage gives, **or** it says 3 % or less while the voltage gives 6 % or more (a gauge that has not learned the cell: on 2026-10-13 it said 0 % at 3579 mV and the device ran four hours more to 3291 mV, and the rule of 4.1.4, which asked for 30 points, believed it below 3579 mV). The percentage is then the voltage's own, the time remaining is withheld, and `BoardPower` logs one line when it starts ("Battery: the gauge says 6 % at 3627 mV; showing about 46 % from the voltage") and one when it ends. **The way back has a margin** so that a reading on the border does not switch the rule on and off with every sample (about once a second for minutes on 2026-10-13, in the log and on the screen): once the voltage's number is shown it stays until the gauge is within 15 points of the voltage and, for an empty gauge, until the voltage gives less than 4 %. The rule needs the charger to have answered and the cable to be absent: a charging cell has a raised voltage. Only this direction is checked, because the voltage dips while the modem sends, and a dip must not make a good gauge look wrong.

**Why rule 2 exists.** On 2026-10-09 the device ran from full on its battery at 135 to 147 mA (the energy log, [energy-log.md](energy-log.md)). The gauge counted down correctly to 55 % (820 mAh), then fell to 6 % (79 mAh) between two lines at 3627 mV and said 0 % for the next three hours and twenty minutes, while the device ran on to 3426 mV with the same load. The display showed 0 and the time remaining showed 0. Switching Wi-Fi on would have been refused for "battery below 20 percent" (`Connectivity::setWifiMode`); nobody tried, so the log has no such line. The probable cause, not checked on the chip: it takes the cell for empty at about 3.6 V because it has not learned this battery. The gauge itself is not touched: changing its configuration could harm the chip and can be tried only on the device.

**The table** (`percentFromVoltage`) is that discharge: time times current gives the charge used (1280 mAh from full to 3426 mV), and each voltage gets the share still left. It holds for a load of about 140 mA, the device awake. With the modem asleep the load is lower, the voltage at the same charge is higher and the number a little too high; during a call or a long transmission it is lower and the number too low. It is a fallback for a gauge that is wrong, not a replacement for a gauge that is right.

## The marks of the cell

`kBatteryEmptyMv` 3300 and `kBatteryFullMv` 4200 (`src/hal/Power.h`) are the ends of the battery bar and what the status page shows next to the voltage. 3300 is where the firmware switches the device off (below); 4200 is the charger's target (4208 mV). The cell is a 3.7 V lithium-polymer cell of 1500 mAh according to the maker's page; its data sheet is not in this repository, and the maker's page names no protection circuit or cut-off voltage. Before 4.2.0 the empty mark was 3430, where **this** cell was flat under 140 mA on 2026-10-09; the device kept running below it.

The voltage table (`percentFromVoltage`) was measured down to 3430 mV; from there to 3300 mV it is a straight line from 3 % to 0 %, which is an extension and not a measurement.

## Deep discharge: the firmware switches off (decision D-5)

- **Measured:** on 2026-10-09 the device kept running at 3426 mV, with nothing watching the voltage.
- **Not known:** whether the cell has a protection circuit, and at what voltage it cuts off. The charger (SY6970) and the gauge (BQ27220) are not a protection of the cell, and the maker's page says nothing about one. Nobody should rely on the hardware protecting the cell.
- **What the firmware does since 4.2.0:** `LowVoltageGuard` (`src/board/LowVoltageGuard.h`, tests in `test/test_low_voltage_guard/`). When the voltage on the battery stays below 3300 mV for 60 seconds, the main loop logs "Battery: <mV> mV for a minute, switching off to protect the cell" and asks for the same switch-off as the menu entry, so the modem says goodbye first and the picture says "Battery empty". The loop reads the voltage every five seconds. A reading at or above the limit, a cable, or an unknown reading starts the minute again, so a dip while the modem sends does not switch the device off. On a cable the switch-off is refused as always.
- **The number is the agent's.** 3300 mV is above the 3.0 V below which lithium-polymer cells are generally harmed and 130 mV under the voltage where this cell was flat. If a dip switches the device off too early on the device, a new entry in [decisions.md](decisions.md) changes it.

## Not verified on the device

That the rule shows the right number at a lower load (modem asleep), what the gauge does after a full learning cycle, and the cell's real limits.
