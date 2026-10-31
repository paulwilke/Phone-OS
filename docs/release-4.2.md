# Release 4.2

Version 4.2.11 of Phone OS (4.2.11: the parents' page asks for its help one tab at a time, a device can refuse remote position queries, the security page, hardened parsers; 4.2.10: the gauge's thresholds lowered after two discharges; 4.2.9: the fuel gauge's end-of-discharge thresholds set for this cell, a whole log read in pieces over the cable and the page; 4.2.8: the charger keeps its 800 mA when the cable comes while the device runs, and the records of the logs of 2026-10-21; 4.2.7: what the review of 2026-10-17 found: the call page keeps its keys and gives the lock back, a call does not cancel the SOS countdown, numbers as written reach the modem, a start without a network searches on, a slow browser no longer stops the device and the modem's bytes are not lost; 4.2.6: what the third night and the charge at the adapter showed, acted on: the modem waits for its IMS, the home network chosen by hand is not searched again, a cut-off message history keeps its read mark, the charger takes 800 mA from the adapter; 4.2.5: the wake lines get pull-ups and wake on the level they do not have, after the second night on the battery; 4.2.4: the night on the battery acted on, the loop reads the battery chips once a second, the gauge keeps what it learned, the sleep is measured; 4.2.3: what a review of the code found on 2026-10-13, the page keeps the password for the tab only, a configuration file is the same file in any spelling, the licence names the owner; 4.2.2: the power symbol, the battery percentage that no longer sticks at 0 %, a bigger USB receive buffer, the loop line with its parts; 4.2.1: two measuring lines in the system log, no change in behaviour; 4.2.0: the rest of this page). It follows the owner's test of 4.1.7 on the device and a day in which the decisions of the project were first written down ([decisions.md](decisions.md), D-1 to D-16). What changed is in [../CHANGELOG.md](../CHANGELOG.md); the acceptance test the owner goes through on the device is [de/abnahmetest.md](de/abnahmetest.md), section 9 for what is new. Everything about 4.1 that this page does not change ([release-4.1.md](release-4.1.md)) stays.

## What changes for the parents

- **Nobody has to know the APN.** Leave `[modem] apn` empty and the device picks it by the SIM card from a list of operators ([apn-table.md](modem/apn-table.md)). A value you have set still wins. Use it if the list is wrong for your SIM.
- **Texts with umlauts are as long as other texts.** An SOS in German that says the child "wants to be picked up" (the word has an o-umlaut) is one message with the umlaut, not three and not spelled with "oe" ([sms-pdu.md](modem/sms-pdu.md)). A long text arrives as one message.
- **Saving on the parents' page from a browser works.** It answered "Request Header Fields Too Large" before.
- **The battery is protected.** The charger is limited to 0.65 C, and the device switches itself off at 3300 mV with the picture "Battery empty" ([battery.md](hardware/battery.md), [device-power.md](hardware/device-power.md)). The bar of the battery is empty at 3300 mV now.
- **`modem_sleep`.** The setting that lets the device sleep is `true` by default. A device with `false` never sleeps and runs for about eleven hours ([power-policy.md](core/power-policy.md)); the status of the open question of Power save is open point 25.

## What changes for the child

- **A Power tile** on the first page, behind the light. It asks "Switch off?" and switches the device off; Cancel is the key that is in focus ([screen-power.md](ui/screen-power.md)). The switch on the side of the case turns the device on again.

## What was checked, and where

| Kind | What | Where |
|---|---|---|
| Tested on the host | every rule above, each with a counter-check where it matters (the old behaviour put back, the test fails); the PDU against a decoder written from the specification | `pio test -e native`, `test/test_gen_apn/test_gen.py` |
| Run in the simulator | every scenario and the click-through demo, with the Power page | `pio run -e sim`, `scripts/make_demo.py` |
| Compiled | the firmware | `pio run -e device` |
| Verified on the device | of 4.2.0 to 4.2.2 from 2026-10-12 to 2026-10-14: the APN from the list with the IMS registered (D-9), texts with umlauts in PDU mode (D-10), the Power tile (D-12), the charge current and the watchdog (D-14), saving from a browser (D-16), the switch-off at 3291 mV (D-5), the percentage from the voltage (D-24), the loop line (D-20, D-25), the USB receive buffer; of 4.2.4 on 2026-10-15: the step of 0.4 ms (D-31), the gauge's learned full charge (D-32), the sleep part of the loop line (D-33); of 4.2.5 on 2026-10-16: the wake lines and a night asleep 98 % (D-34); of 4.2.6 on 2026-10-21: the 800 mA at the adapter from a start with the cable attached (D-38), the percentage from the voltage once more (D-24); of 4.2.8 on 2026-10-21: the limit written again at a plug-in while running (D-43); of 4.2.9 on 2026-10-21 to 24: the gauge's threshold line at two starts (D-45) and the whole log over the cable (D-44), no fall at 3.63 V and a learned full charge that grows over two discharges (D-45, in part), wakes by a key or the ring line (D-34), starts registered automatically within 29 s (D-36, in part), the charge at the adapter at 849 falling to 746 mA (D-38); not yet: 4.2.11 (D-54, D-56), 4.2.10 (D-47), 4.2.3 (D-26), the rest of 4.2.6 (D-35, D-36, D-37), 4.2.7 (D-39, D-40, D-41) and the page's "Load the whole log" (D-44) | the "Verified" line of each decision in [decisions.md](decisions.md); the date columns of [de/abnahmetest.md](de/abnahmetest.md) section 9 are empty |

## What the owner tries on the device

For 4.2.11: the start line of the new version, the parents' page opened in a browser with each tab's help appearing when the tab opens (D-56), and a position query answered as before with `[remote_queries] enabled` absent or `true` (D-54); a device with `false` must refuse it and still show the attempt in the history.

For 4.2.10: the start line after flashing, "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3390/3440 mV, compensation off (they were 3300/3420/3470 mV, compensation off)" and "already, nothing to write" at the next start (D-47); then a discharge to the switch-off: the gauge's 0 % within about half an hour of the switch-off, and a hold at 7 % that shrinks as the full charge converges (open point 3).

For 4.2.9: seen on 2026-10-21 are the start line after flashing, "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3420/3470 mV, compensation off (they were 3031/3385/3501 mV, compensation on)", "already, nothing to write" at the next start (D-45), and the whole log with `deviceconfig.py read system.log --all` (D-44). Still to try: a discharge past 3.63 V that no longer falls to 7 % (open point 3), and the log tab's "Load the whole log" showing the log's first line (open point 42).

For 4.2.8: seen on 2026-10-21 (D-43): the cable attached while the device runs gives "Battery: input current limit set to 800 mA again (it was 500 mA: ...)" once, and `battery` reads 800 mA. Still to see: a charge at the adapter after such a plug-in at about 800 mA in the energy log (open point 26).

For 4.2.7, after flashing it in place of 4.2.6 (whose checks below still hold): the calls of open point 38 (the keys under the screen during a call, the lock after a call, a call during the SOS countdown); an SOS contact written with blanks in `contacts.toml` (open point 41); the start in a cellar (open point 39); and at the cable the run of open point 40 (a slow download of the page while the keys and a call still work, three long texts while the device was off that arrive whole, and no "Modem UART: receiving failed" in the log).

For 4.2.6: the start lines "Battery: input current limit set to 800 mA (it was 500 mA)" (once; "already, nothing to write" afterwards) and "Messages: restored N (0 unread), 0 damaged lines"; after a start "Modem sleeps" comes after "Cellular: IMS registered" and the lock screen says "Connected" within half a minute of the network (open point 37); at home no "searching again" a quarter of an hour after a start (open point 19); the charge at the adapter was seen on 2026-10-21 at 812 falling to 759 mA from a start with the cable attached (open point 26 keeps the plug-in while running, 4.2.8).

For 4.2.5 (seen on 2026-10-16, D-34): the start line "Wake lines: keyboard ..., touch ..., ring ..." right after flashing (on the cable), and after a night on the battery the loop lines' sleep part (most sleeps ended by the timer, "asleep" most of the half hour) and the energy log's current (well under 105 mA).

For 4.2.4 (seen on 2026-10-15, D-31 to D-33; the message count is 4.2.6's D-37, not seen yet), a night on the battery (locked, modem asleep, no Wi-Fi) and then the logs over the cable: the start line about the gauge ("nothing to write; it has learned a full charge of ... mAh"), the loop lines (the step under 5 ms, and the sleep part: how many sleeps, what ended them, which line was low before), the energy log's current (lower than 105 mA, if the sleep now happens), and the lock screen's message count after a restart.

For 4.2.3: the parents' page after a reload of the tab (no new login) and after closing and opening it again (a login); the raw editor on the Files tab with a file named `/config/Settings.toml` (refused like the lowercase name when the content is wrong); a restore with the ZIP archive (works) and with any other file (refused, nothing written).

Section 9 of the acceptance test, in this order: `battery` (charge current and watchdog), the Power page, a text with an umlaut and a long text, a restart without an APN in `settings.toml`, saving from the browser. Last, the discharge run to the switch-off, which takes a day.

## Decisions that belong to the owner

- **Power save** (D-13, open point 25): what it should do, once `modem_sleep` is on.
- **The charge current and the switch-off voltage** (D-14, D-5) are the agent's numbers; change them with a new entry if the device shows a reason.
- **No firmware binary is handed out** (D-23, decided by the owner): the releases carry the source, the changelog and the notes.
