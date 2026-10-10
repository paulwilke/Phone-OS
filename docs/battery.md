# The battery numbers

Code: `src/board/BatteryPlausibility.{h,cpp}` (the rules), `src/board/BoardPower.cpp` (the readings), `src/hal/Power.h` (the marks of the cell), `src/ui/screens/StatusScreen.cpp` (the page); tests in `test/test_battery_plausibility/`, `test_board_power/`, `test_screen_status/`. The chips are in [device-power.md](device-power.md); the lesson of the wrong battery size is in [hardware-lessons.md](hardware-lessons.md).

## Where the percentage comes from

The fuel gauge (a BQ27220) counts charge, but it only counts well once it has learned the battery, and this one has not (cycle count 0). Two rules stop the interface from showing a number the voltage contradicts:

1. **A low number against a high cell** ([device-power.md](device-power.md)): the gauge says less than 70 % while the cell is above 4.1 V or the charger says "charge done". The number is withheld.
2. **A very low number against a middling cell, on the battery** (since 4.1.4): the gauge says 30 or more points less than the voltage gives. The percentage is then the voltage's own, the time remaining is withheld, and `BoardPower` logs one line when it starts ("Battery: the gauge says 6 % at 3627 mV; showing about 46 % from the voltage") and one when it ends. The rule needs the charger to have answered and the cable to be absent: a charging cell has a raised voltage. Only this direction is checked, because the voltage dips while the modem sends, and a dip must not make a good gauge look wrong.

**Why rule 2 exists.** On 2026-10-09 the device ran from full on its battery at 135 to 147 mA (the energy log, [energy-log.md](energy-log.md)). The gauge counted down correctly to 55 % (820 mAh), then fell to 6 % (79 mAh) between two lines at 3627 mV and said 0 % for the next three hours and twenty minutes, while the device ran on to 3426 mV with the same load. The display showed 0 and the time remaining showed 0. Switching Wi-Fi on would have been refused for "battery below 20 percent" (`Connectivity::setWifiMode`); nobody tried, so the log has no such line. The probable cause, not checked on the chip: it takes the cell for empty at about 3.6 V because it has not learned this battery. The gauge itself is not touched: changing its configuration could harm the chip and can be tried only on the device.

**The table** (`percentFromVoltage`) is that discharge: time times current gives the charge used (1280 mAh from full to 3426 mV), and each voltage gets the share still left. It holds for a load of about 140 mA, the device awake. With the modem asleep the load is lower, the voltage at the same charge is higher and the number a little too high; during a call or a long transmission it is lower and the number too low. It is a fallback for a gauge that is wrong, not a replacement for a gauge that is right.

## The marks of the cell

`kBatteryEmptyMv` 3430 and `kBatteryFullMv` 4200 (`src/hal/Power.h`) are what the status page shows next to the voltage. 3430 is where **this** cell was flat under the load above; 4200 is the charger's target (4208 mV). They are not the cell's limits: the data sheet of the cell is not in this repository.

## Deep discharge: what is known

- **Measured:** the device kept running at 3426 mV. The firmware has no low-voltage switch-off: nothing in it watches the voltage and powers the device off.
- **Not known:** whether the cell has a protection circuit, and at what voltage it cuts off. The charger (SY6970) and the gauge (BQ27220) are not a protection of the cell: the charger's battery switch is used here only to switch the device off, and the gauge only measures.
- **Therefore** nobody should rely on the hardware protecting the cell until that is looked up. The owner switched the device off by hand on 2026-10-09 for this reason.
- **A mitigation, not built:** the firmware switches the device off by itself when the voltage on the battery stays below a limit for a minute (`BoardPower::powerOff()` exists). Whether to do this, and at what voltage, is the owner's decision: a child's emergency phone that switches off earlier is less reachable, and a cell that is discharged too far is damaged. Open point 18 in [open-points.md](open-points.md).

## Not verified on the device

That the rule shows the right number at a lower load (modem asleep), what the gauge does after a full learning cycle, and the cell's real limits.
