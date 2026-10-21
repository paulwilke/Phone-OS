# The energy log

Code: `src/board/EnergyLog.{h,cpp}`, called from the main loop in `device/main.cpp`; tests in `test/test_energy_log/`. Nothing in the application layer knows about it: it is a record of what the board's fuel gauge says, written to the card.

## What it is for

The system log ([log-level.md](../core/log-level.md)) says what happened; it cannot say how fast the battery emptied while it happened. On 2026-10-09 a device ran down fast and the system log held no battery value at all; the only clue was an absence (no `Modem sleeps` line in hours) and the setting behind it (`modem_sleep = false`). The energy log is the series of readings that answers the question next time.

## The file

`/energy.log` on the card, one line every ten minutes, the first at the first pass of the loop after the start:

```
<epoch> <uptime s> <percent> <mV> <mA> <mAh> <state>
1791489827 600 63 3754 -42 939 modem-sleep
```

| Field | Meaning |
|---|---|
| epoch | Unix time (UTC), or `-` while the clock does not know it. The uptime restarts with every boot, so alone it cannot order lines across a restart. |
| uptime s | seconds since the start |
| percent, mV, mA, mAh | the **fuel gauge's own** readings, not the plausibility-checked percentage on the screens ([device-power.md](device-power.md)): a gauge that is wrong has to be visible here. mA is negative while discharging; mAh is the charge the gauge says is left. All four are `-` when the gauge does not answer. |
| state | `cable`: a supply is attached and nothing sleeps. `modem-sleep`: the power policy lets the modem sleep, and with it the processor ([power-policy.md](../core/power-policy.md)). `awake`: neither, for example with `modem_sleep = false`, during a call or with work pending. |

Read it over the cable with `python3 scripts/deviceconfig.py read energy.log` (the last 8 KB, about two days) or on the Files tab of the parents' page.

## Rules

- **It does not eat the card.** Below 32 KB free nothing is written (the configuration needs the last room), and at 64 KB the file is renamed to `/energy.old.log`, replacing the older one: the two hold about a month.
- **A line that cannot be written is lost**, not queued. A reading is worth something at its own time; a late line would lie about it. A failed write counts toward the card's three failures like every other.
- **The next line is due ten minutes after the last attempt**, written or not, and the interval survives the wrap of the 32-bit millisecond counter.
- **A gauge that does not answer still leaves a line** (with dashes), because "no reading" is itself a finding.

## How to read it

A discharge with the state `awake` all day is the setting, not the battery. A discharge with `modem-sleep` that is still fast points at something else that is awake: Wi-Fi, the receiver, the front light. The system log has the lines for those.

## On the device

Seen on the device since 2026-10-12: a line every ten minutes, read over the cable; the states `awake` and `cable` on 2026-10-12 (decision D-13); the nights and charges from 2026-10-13 on were read from it ([hardware-lessons.md](hardware-lessons.md): about 105 mA, then 58 to 63 mA with the processor asleep, 812 mA at the adapter). Host-tested only: the rotation to `/energy.old.log` at 64 KB and the line with dashes when the gauge does not answer; the records quote the current without its sign.
