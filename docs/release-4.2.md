# Release 4.2

Version 4.2.3 of Phone OS (4.2.3: what a review of the code found on 2026-10-13, the page keeps the password for the tab only, a configuration file is the same file in any spelling, the licence names the owner; 4.2.2: the power symbol, the battery percentage that no longer sticks at 0 %, a bigger USB receive buffer, the loop line with its parts; 4.2.1: two measuring lines in the system log, no change in behaviour; 4.2.0: the rest of this page). It follows the owner's test of 4.1.7 on the device and a day in which the decisions of the project were first written down ([decisions.md](decisions.md), D-1 to D-16). What changed is in [../CHANGELOG.md](../CHANGELOG.md); the acceptance test the owner goes through on the device is [de/abnahmetest.md](de/abnahmetest.md), section 9 for what is new. Everything about 4.1 that this page does not change ([release-4.1.md](release-4.1.md)) stays.

## What changes for the parents

- **Nobody has to know the APN.** Leave `[modem] apn` empty and the device picks it by the SIM card from a list of operators ([apn-table.md](apn-table.md)). A value you have set still wins. Use it if the list is wrong for your SIM.
- **Texts with umlauts are as long as other texts.** An SOS in German that says the child "wants to be picked up" (the word has an o-umlaut) is one message with the umlaut, not three and not spelled with "oe" ([sms-pdu.md](sms-pdu.md)). A long text arrives as one message.
- **Saving on the parents' page from a browser works.** It answered "Request Header Fields Too Large" before.
- **The battery is protected.** The charger is limited to 0.65 C, and the device switches itself off at 3300 mV with the picture "Battery empty" ([battery.md](battery.md), [device-power.md](device-power.md)). The bar of the battery is empty at 3300 mV now.
- **`modem_sleep`.** The setting that lets the device sleep is `true` by default. A device with `false` never sleeps and runs for about eleven hours ([power-policy.md](power-policy.md)); the status of the open question of Power save is open point 25.

## What changes for the child

- **A Power tile** on the first page, behind the light. It asks "Switch off?" and switches the device off; Cancel is the key that is in focus ([screen-power.md](screen-power.md)). The switch on the side of the case turns the device on again.

## What was checked, and where

| Kind | What | Where |
|---|---|---|
| Tested on the host | every rule above, each with a counter-check where it matters (the old behaviour put back, the test fails); the PDU against a decoder written from the specification | `pio test -e native`, `test/test_gen_apn/test_gen.py` |
| Run in the simulator | every scenario and the click-through demo, with the Power page | `pio run -e sim`, `scripts/make_demo.py` |
| Compiled | the firmware | `pio run -e device` |
| Verified on the device | nothing of 4.2 yet | [de/abnahmetest.md](de/abnahmetest.md) section 9 |

## What the owner tries on the device

For 4.2.3: the parents' page after a reload of the tab (no new login) and after closing and opening it again (a login); the raw editor on the Files tab with a file named `/config/Settings.toml` (refused like the lowercase name when the content is wrong); a restore with the ZIP archive (works) and with any other file (refused, nothing written).

Section 9 of the acceptance test, in this order: `battery` (charge current and watchdog), the Power page, a text with an umlaut and a long text, a restart without an APN in `settings.toml`, saving from the browser. Last, the discharge run to the switch-off, which takes a day.

## Decisions that belong to the owner

- **Power save** (D-13, open point 25): what it should do, once `modem_sleep` is on.
- **The charge current and the switch-off voltage** (D-14, D-5) are the agent's numbers; change them with a new entry if the device shows a reason.
- **No firmware binary is handed out** (D-23, decided by the owner): the releases carry the source, the changelog and the notes.
