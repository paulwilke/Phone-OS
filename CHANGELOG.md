# Changelog

Each release names what changed for the people who use the device, what was removed and what is not verified. "Host-tested" means a test under `pio test -e native` or a Python or Node test; "compiled" means only built; "verified on the device" means seen on the real hardware, as the "Verified" line of the decision in [docs/decisions.md](docs/decisions.md) records it with a date. An entry says what was known at its release; what was seen on the device later is in those lines. The notes of a release are in `docs/release-<major>.<minor>.md`.

## 4.2.10

What the two discharges of 2026-10-22 and 2026-10-24 with 4.2.9 showed, acted on. Host-tested and compiled; the change is not verified on the device yet (open point 3).

- **The gauge's end-of-discharge thresholds are lowered to 3300/3390/3440 mV** (D-47). With 3470 and 3420 mV the gauge said 0 % about an hour and a half before the switch-off on both discharges (1 h 26 min and 1 h 36 min). The first start after flashing logs "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3390/3440 mV, compensation off (they were 3300/3420/3470 mV, compensation off)". A hold at 7 % may last longer, by about 45 minutes, until the gauge has learned the full charge. Host-tested and compiled; not verified on the device.
- **Seen on the device with 4.2.9** (2026-10-22 to 24): no fall from 45 % to 7 % at 3.63 V any more, and a learned full charge that grows (988, 1244, 1310 mAh; D-45); a day of real use by the child, 21 h 32 min from full to the switch-off at 64 mA, the modem awake 0.8 % of the time, no network lost; wakes by a key or the ring line seen for the first time (D-34); starts after 1 h 41 min and 2 h 56 min switched off registered automatically within 29 s (D-36); the charge at the adapter at 849 falling to 746 mA, 1 to 100 % in 1 h 40 min (D-38).
- **A page "What Phone OS does not protect against"** ([docs/security.md](docs/security.md)) and two decisions: the reply to a remote position query always stays visible in the history and the message log, and a device can refuse queries (D-54); the data on the card stays in plain text and the limit is written down (D-55). Documentation only; the switch itself follows in the next change.

## 4.2.9

Two changes worked out in the cloud on 2026-10-21 from the owner's decisions of that day. Host-tested and compiled; neither is verified on the device yet (open points 3 and 42).

- **The fuel gauge no longer takes the cell for empty at 3.6 V** (D-42, D-45). The gauge fell from about 45 % to 7 % within ten minutes at 3.63 V and learned a smaller full charge at every such discharge (1372, 1107, 988 mAh), while the cell gave seven more hours. At start the firmware now sets the gauge's end-of-discharge thresholds for this cell, 3300/3420/3470 mV with the chip's compensation off, from the chip's reference manual, written once like the battery size; the start logs "Battery: the fuel gauge's end-of-discharge thresholds set to ...". Host-tested and compiled; not verified on the device.
- **A whole log can be read with the card in the device** (D-44): `deviceconfig.py read system.log --all` over the cable, and **Load the whole log** on the page's log tab; without them the end of a long log is shown as before. Host-tested and compiled; not verified on the device.

## 4.2.8

What the logs of 2026-10-21 (4.2.6, two charges and a day on the table) showed, acted on and recorded. Host-tested and compiled; the change is not verified on the device yet (open point 26).

- **The charger keeps its 800 mA when the cable comes while the device runs** (D-43). The chip sets the input limit back to 500 mA from its own detection at every new supply, after what the start wrote; a cable attached to a running device charged at 500 mA as before 4.2.6, and only a start with the adapter attached charged at 800. While a cable feeds the board the firmware now writes the limit again whenever the chip changed it, and says so once per plug-in: "Battery: input current limit set to 800 mA again (it was 500 mA: the charger set its own when it detected the supply)". Host-tested and compiled; not verified on the device.
- **Seen on the device on 2026-10-21** (4.2.6): the charge at the adapter at 812 falling to 759 mA, 13 to 95 % in 70 minutes (D-38); a day on the table at 61.7 mA with the processor asleep 96 to 98 % and every sleep ended by the timer (as in the third night, open point 25); the battery percentage from the voltage once more, with one line at 01:31 and one when the cable came (D-24). The gauge's learned full charge fell from 1372 to 1107 to 988 mAh, because the gauge ends its discharge at 3.63 V while the cell gives seven more hours: what to do about it is decision D-42, open for the owner (open point 3).

## 4.2.7

What the review of 2026-10-17 found, fixed, with the owner's decisions on the SOS countdown and the start without a network. Host-tested and compiled; none of it is verified on the device yet (open points 38 to 41).

- **A call keeps its page.** The three keys under the screen are touch keys; touched during a call, ringing or connected, they took the call page away and nothing brought it back: no way to answer or to hang up, and the device locked in the middle of the call. During a call they do nothing now. Host-tested and compiled; not verified on the device.
- **A call through the lock leaves the device locked.** After a call that rang on a locked device, answered or missed, the device stayed unlocked, and a touch in the pocket could start an SOS. It is locked again when the call is over. Host-tested and compiled; not verified on the device.
- **A call does not cancel the SOS countdown** (D-39). A call that rang during the 40 seconds of the SOS choice cancelled the emergency, and nothing was sent. Now the call can be answered, the countdown goes on and the emergency goes out by itself, and after the call the SOS page shows what was sent. Host-tested and compiled; not verified on the device.
- **Numbers reach the modem however the parents wrote them.** An SOS contact or a call button written with blanks or dashes ("+49 170 1111111", "0170 1111111") was refused by the modem: the SOS said "sent" for a text that never left, and the button did nothing. Every number now goes to the modem in its international form. Host-tested and compiled; not verified on the device.
- **A start without a network searches on** (D-40). Switched on in a cellar, a tunnel or the underground, the modem gave up after about four and a half minutes and stayed without calls, texts and SOS until a restart, even when the network came back. It searches on now, as it does for a network lost later; the log says "Cellular: no network found, searching on". Host-tested and compiled; not verified on the device.
- **A slow browser no longer stops the device, and the modem's bytes are not lost** (D-41). A phone that loaded the parents' page slowly held the whole device, the SOS, the calls and the keys, for as long as it read; the page is now sent in pieces between the device's other work. The modem's receive buffer is 4096 bytes instead of 256, an overflow is logged ("Modem UART: receiving failed ..."), and a text that came in broken on the line stays on the SIM to be read again instead of arriving empty. Host-tested and compiled; not verified on the device (open point 40, the run at the cable).

## 4.2.6

What the log of 2026-10-16 (4.2.5) and the charge at the adapter on 2026-10-15 showed, acted on. Host-tested and compiled; none of it is verified on the device yet (open points 19, 26 and 37).

- **The modem waits for its IMS before it sleeps** (D-35). The modem went to sleep 14 seconds after the network registration, before telephony (the IMS) had registered, and nothing asked again for 40 minutes: the screens said "connecting" and a call was refused. A registered modem now stays awake until the IMS is registered or the network carries SMS only, at most two minutes after the registration. Host-tested and compiled; not verified on the device. To look for: "Modem sleeps" after "Cellular: IMS registered", and "Connected" on the lock screen within half a minute.
- **The home network chosen by hand is not searched again** (D-36). After a long time switched off the modem had not registered within 16 seconds and the driver chose the SIM's own network by hand; a quarter of an hour later, at the first key press, it dropped that network and searched again: about half a minute without calls. The automatic phase now lasts about a minute, and the SIM's own network chosen by hand is taken for the home network (the modem is set back to automatic without a gap). Host-tested and compiled; not verified on the device. To look for: no "searching again" a quarter of an hour after a start at home.
- **A message history that could not be read whole no longer shows 35 unread messages** (D-37). After a start that read the history only in part, the lock screen counted every stored message as unread. The read mark is applied after such a read, the start logs the result ("Messages: restored N (0 unread)"), and nothing is written to the history until a start reads it whole. Host-tested and compiled; not verified on the device.
- **The charger takes 800 mA from the adapter instead of 500** (D-38). The chip took the mains adapter for an unknown adapter and limited its input to 500 mA, so the cell charged at 516 falling to 420 mA for 3 h 10 min and the cell's own limit of 960 mA was never reached. The input limit is set to 800 mA at every start, like the charge current limit; the start logs "Battery: input current limit set to 800 mA (it was 500 mA)". Host-tested and compiled; not verified on the device. To look for: the next charge at the adapter in the energy log.
- **Two texts say what is true.** The modem's start line reads "Modem: no answer, power key pressed" (the key is the expected path after a switch-off, not a failure), and the battery diagnosis marks the charger's ADC values (VBUS, battery, system, charging current) as "not measured" while no conversion runs. They were left-over register contents, not readings. Host-tested and compiled; not verified on the device.

## 4.2.5

What the second night on the battery showed (2026-10-14, with 4.2.4), acted on. Host-tested and compiled; the wake lines are to be verified on the device (the start line, then a night).

- **The wake lines get pull-ups and wake on the level they do not have** (D-34). The loop line of the night said: 398,000 sleeps, none ended by the timer or a line, the keyboard's interrupt and the modem's ring line low before every one, the current unchanged at 105 mA. The keyboard chip's interrupt is open-drain and the board has no pull-up; the processor's own pull-ups hold the three lines high now, and each is armed to wake on the level it does not have at that moment. A sleep that nothing ended no longer counts as sleep, and the start logs "Wake lines: keyboard ..., touch ..., ring ...", on the cable too.
- **Seen on the device on 2026-10-15** (4.2.4): the step of the loop costs 0.4 ms instead of 52 (D-31); the gauge's start line names its learned 1372 mAh and writes nothing (D-32); the loop line's sleep part names the lines (D-33); the switch-off at 3291 mV after 12.2 hours; the battery percentage from the voltage from 01:28 to 06:01 with two lines (D-24).

## 4.2.4

What the night of 2026-10-13 on the battery showed, acted on (decisions D-31 to D-33). Host-tested and compiled; none of it is verified on the device yet. The owner flashes it and goes through the test protocol.

- **The loop no longer reads the battery chips every pass** (D-31). The call state was asked through the whole core state, which read the fuel gauge and the charger over I2C five times, 52 ms of every 56 ms pass with nothing to do: the 105 mA of the night. The call state is asked alone, and one reading of the chips serves every question for a second. To look for: the loop line's "step" under 5 ms.
- **The fuel gauge keeps what it learned** (D-32). After the first full discharge it reported a learned full charge of 1372 mAh, and the firmware wrote 1500 over it at every start. The configured size (`[battery] capacity_mah`) is written once, when the gauge's design capacity differs; the learned full charge is the gauge's own, and the start line names it.
- **The loop line says what ends each sleep** (D-33): how often the processor slept, whether the timer or a line woke it, and which of the three wake lines (keyboard, touch, the modem's ring line) was low before the sleep. Sleep was allowed in every pass of the night and happened 1 % of the time; a line held low is the suspect, and this names it. No change of behaviour.
- **Seen on the device on 2026-10-14** (4.2.2): the switch-off at 3291 mV (D-5), the battery percentage from the voltage with two lines in a night (D-24), the loop line (D-25), 12.8 hours on the battery at about 105 mA.

## 4.2.3

What a review of the code on 2026-10-13 found, fixed on the host; none of it is verified on the device yet. Nothing changes for the child; the parents' page and the tools behave the same unless a sentence below says otherwise.

- **The parents' page keeps the password for the tab only** (D-26). It was kept in the browser's `localStorage`, for good and under an address every ESP32 access point shares; now it dies with the tab. A reload in the same tab does not ask again, closing the tab does.
- **A configuration file is the same file in any spelling.** The card is FAT, but the page compared names exactly: `/CONFIG/SECRETS.TOML` could be deleted and `/config/Settings.toml` written unchecked through the raw editor. Both meet the rules of the lowercase name now. The rule that a SIM PIN is four to eight digits is the loader's, so it holds for a file written over the USB console or on the card in a computer too.
- **The page's JSON reader refuses nesting deeper than 16 levels** instead of running the device out of stack (a restart, with the web password). A fuzz suite covers it and the ZIP reader (`test_web_fuzz`).
- **The JSON bundle of 4.0 is no longer restored** (D-30): a restore takes the ZIP archive and nothing else.
- **A password that does not fit its buffer locks the page with a message** instead of being compared by its first 95 characters; a `%` in a query takes exactly two hex digits.
- **Written down, not changed** (D-28, D-29): the USB console asks for no password (the user's guide says so; a restriction is open point 33), and the access point's random password stays eight digits.
- **The licence names the owner** (D-27): "Paul Wilke and the Phone OS contributors". The Apache-2.0 text that the operator list requires is stored, and SensorLib is pinned to 0.5.0.
- **The simulator's crawl stops with the reason** when a device cannot start, instead of a segmentation fault (seen with a stale work directory in the temp folder; CI never saw it).

## 4.2.2

Fixes and measurements from the night of 2026-10-13. Host-tested and compiled; none of it is verified on the device yet (the open points named below).

- **The Power tile shows the power symbol** (D-21), a circle with a bar, and no word: "Strom" was a poor translation, and no word is right in every language. The page behind it is titled "Switch off" / "Ausschalten" / "Eteindre". Open point removed; to look at on the device.
- **The battery percentage no longer sticks at 0 % while the cell is not empty** (D-24). The gauge said 0 % at 3579 mV and the device ran four hours more; the screen showed 0 % all that time, and the rule flipped about once a second at the border (62 log lines in three minutes). Now a gauge at 3 % or less is not believed while the voltage gives 6 % or more, and the way back has a margin. Open point 32. **Seen on the device on 2026-10-14:** two lines in a night instead of one a second, and the screen followed the voltage to the switch-off.
- **The USB console keeps what a computer writes** (point 29). The receive buffer was the default 256 bytes and overflowed when a file was written; it is 4096 bytes now. **Seen on the device on 2026-10-13:** the settings file (615 bytes, more than the 512 that got through before) was written over the cable and read back identical.
- **The Loop line tells where the time goes** (D-25): the share of passes in which the power policy allows sleep, and the average cost per pass of the console, the keys, the modem, the application step and the housekeeping. Open points 20 and 25. **Seen on the device on 2026-10-14:** the step costs 52 of 56 ms, sleep is allowed in every pass and happens 1 % of the time ([docs/hardware/hardware-lessons.md](docs/hardware/hardware-lessons.md)).

## 4.2.1

A version for measuring (D-20); it changes no behaviour. Host-tested and compiled; the lines it writes are to be read on the device (open points 20, 25 and 30).

- **The system log says how the loop is doing.** Every half hour one line: "Loop: longest pass 11320 ms, 400 passes in 1800 s, asleep 12 %". The longest pass is the longest time the loop was busy without a break, which is how long a key press could have waited (the sensor start, the e-ink wait and the card are the suspects, D-8); "asleep" is the share of the half hour that the processor spent in light sleep, which says whether the power policy's permission to sleep is used (the energy log showed about 105 mA with the modem asleep). Kept on the levels `all` and `events`.
- **The system log says how the parents' page was asked for its words.** The three requests of the page after the login (`/api/lang/<code>`, `/labels`, `/help`) leave one line each, "WEB: GET /api/lang/en -> 200, sent 2498 bytes", so that a page that shows keys instead of labels can be explained from the log. Nothing else that went well is logged.

## 4.2.0

Every change names the decision (D-number) in [docs/decisions.md](docs/decisions.md) that holds its reasons. Host-tested and compiled; **none of it is verified on the device yet** (the open points named below).

- **The charger is limited** (D-14). The SY6970 charged with its default of 2048 mA, more than 1 C for the 1500 mAh cell, and its watchdog (40 seconds, nobody fed it) can set the settings back to the default. At every start the firmware now sets the charge current to 960 mA (0.65 C of `[battery] capacity_mah`) and switches the watchdog off, writing only on a difference and reading back; the log says "charge current limited to 960 mA (it was 2048 mA)". `battery` shows the watchdog. **Seen on the device on 2026-10-12:** 960 mA and "watchdog off" after the start. Open point 26 (charging with an adapter).
- **The device switches itself off before the cell is damaged** (D-5). When the voltage on the battery stays below 3300 mV for a minute, it switches off like the menu entry does, with the picture "Battery empty"; a dip while the modem sends does not count. The battery bar now ends at 3300 mV (it ended at 3430) and the status page says "Empty 3300". Seen on the device on 2026-10-13: it switched off at 3291 mV after a night on the battery.
- **A Power tile** is the last tile of the first page (D-12). It opens a page with **Switch off**, which asks "Switch off?" with Cancel in focus and then switches off; with the cable plugged in the page says to unplug it. Power save is not on the page (D-13). Open point 23.
- **Messages go out in PDU mode** (D-10). The German umlauts and the sharp s are in the 7-bit alphabet of an SMS, so a text with an umlaut has 160 places and the word is sent as written; the replacement by "oe" of 4.1.5 is gone. A text with a character outside the alphabet is UCS2 (70); a longer text is one concatenated message that the receiving phone shows as one, where it was several. The log says "Messages: sent in 2 parts, GSM 7-bit". Open point 22.
- **The APN is chosen from a list** (D-9). With `[modem] apn` empty the driver asks the SIM for its operator and takes the APN from a list of 1146 operators made from the Android project's sample APN file (Apache-2.0, [THIRD_PARTY.md](THIRD_PARTY.md)). A configured APN always wins, and an unknown operator leaves the modem's contexts alone. Open point 21.
- **Saving a setting from a browser works** (D-16). The parents' page answered "Request Header Fields Too Large" when the browser's headers were more than 1024 bytes; the limit is 4096. Open point 27.
- **The modem firmware is not updated** (D-15): LilyGO's answer and the decision are in [docs/modem/volte.md](docs/modem/volte.md).

## 4.1.7

- **The modem looks for its home network again** when it is on a foreign network (roaming) or on one that was chosen by hand: every 15 minutes, when nothing else is going on, it deregisters (`AT+COPS=2`) and lets the modem search again (`AT+COPS=0`), which prefers the home network. After the trip over the German-French border the device stayed on the French network for a long time: the modem does not leave a foreign network by itself while that one still carries, and a network chosen by hand (the driver does this when the automatic search finds nothing, and takes the first of the list) stays until somebody changes it, which nothing did. At home the appointment is dropped; a call, a text to send or a sleeping modem come first. Host-tested with a scripted modem; **not verified on a real network** (see [docs/modem/modem-driver.md](docs/modem/modem-driver.md)).

## 4.1.6

- **The home screen has a visible Lock button** in the bar at the bottom ("Lock", "Sperren", "Verrouiller"). Back locks the device there, and since 4.1.0 the bar was empty (the arrows were removed on purpose, and the Back key went with them), so nothing said how to lock it. The word never changes with the selection, so the bar costs no redraw. The arrows stay away. Host-tested in all three languages; not verified on the device.
- **The GPS note stands in the middle of the status bar** while the position is being determined, instead of after the left text. It gives way only when the left text or the date would run into it. Host-tested; not verified on the device.

## 4.1.5

- **An SOS with an umlaut is one message, not three.** One o-umlaut (German "moechte" written with it) turned the text into UCS2, which allows 70 characters per message and has no concatenation header, so an SOS of about 130 characters arrived as three separate messages and cost three. A text that would take more messages as UCS2 than in plain ASCII is now sent in plain ASCII when every non-ASCII character has a stand-in ("moechte" for the word with the umlaut, "e" for e-acute, the typographic quote as `'`, the euro sign as `EUR`); the history keeps the original. A text with an emoji or another script keeps its characters. Host-tested; not verified on the device. See [docs/modem/modem-driver.md](docs/modem/modem-driver.md).

## 4.1.4

- **The battery shows what the voltage says when the gauge says almost nothing.** On 2026-10-09 the fuel gauge fell from 55 % to 6 % in one step at 3627 mV and then said 0 % for three hours while the device ran on to 3426 mV at the same load. The gauge had never learned the battery (cycle count 0). On the battery, when the gauge says 30 or more points less than the voltage gives, the percentage is now the voltage's own number (a table measured in that discharge), the time remaining is withheld, and the log says so once. Before, the display showed 0, and switching Wi-Fi on would have been refused for "battery below 20 percent" (nobody tried in that time, so the log has no such line). Host-tested; not verified on the device.
- **The status page shows the voltage** in millivolts with the flat and the full mark (two short lines: "Voltage: 3561 mV" and "Flat 3430, full 4200 mV") and says "Battery almost empty" at 10 % or less on the battery. Host-tested.
- **Deep discharge is not protected by the firmware, and nothing shows that the cell is.** Written down in [docs/hardware/battery.md](docs/hardware/battery.md) with the decision for the owner (open point 18).

## 4.1.3

- **The energy log is back.** `/energy.log` on the card gets one line every ten minutes: the time, the uptime, the fuel gauge's percentage, voltage, current and remaining charge, and whether a cable is attached, the modem may sleep or the device is awake. It exists to explain a battery that ran down fast (on 2026-10-09 the log showed no sleep at all: `modem_sleep = false` keeps modem and processor awake). It is read over the cable (`deviceconfig.py read energy.log`) and on the Files tab, and rotates to `energy.old.log` at 64 KB. 3.2 wrote a file of that name in another format; the Files tab no longer marks it as unused. Host-tested; not verified on the device. See [docs/hardware/energy-log.md](docs/hardware/energy-log.md).

## 4.1.2

The project is called **Phone OS**: generic words that nobody can register as a trade mark ("PhonePlus" is registered by others). Only names change, no behaviour; what the parents notice is in [docs/release-4.1.md](docs/release-4.1.md).

- **The splash picture** says "Phone OS". Host-tested; not verified on the device.
- **The parents' page** is titled "Phone OS", and every change it sends carries the header `X-Phone-OS: 1` instead of `X-Phoneplus: 1`; the device refuses a change without the new header. A page the browser kept from an earlier version must be reloaded once. Host-tested; not verified on the device.
- **Backup** saves `phone-os-backup.zip`; the 4.0 bundle is still read. Host-tested.
- **The default network names are `phone-os`**: the access point, the host name in the home network and the radio name. A name already saved on the card stays. Host-tested; not verified on the device.
- **The licence** names "Phone OS contributors"; the MIT text is unchanged.

## 4.1.1

- **An SOS without a position carries the last known one**, said as such and with its age ("No GPS. LAST KNOWN position, 25 min ago", "LETZTE BEKANNTE Position vor 25 Min."), so that nobody takes it for the present place. Seen on the device 2026-10-08: an SOS from inside a house went out without any place. The last position is kept while the device runs (from the start-up fix, an SOS, a query, the GPS page), not across switching off. Host-tested; not verified on the device.
- **The splash picture is back**: "PhonePlus", "T-Deck Pro MAX" and the version while the device starts, as in 3.x. Compiled; not verified on the device.
- **Switching off shows "Switched off"** before the battery is disconnected: e-ink keeps its last picture, and a device that went off showing a page looked on. With a cable the switch-off is refused at once, with the notice, without shutting the modem down (it was asked again and again before); every request is logged ("Power: switch off asked for"). Compiled and host-tested where it can be; not verified on the device.

## 4.1.0

The interface, tidied after the owner's test of 4.0 on the device (2026-10-07/08). The notes for the parents and the child, and what to try on the device, are in [docs/release-4.1.md](docs/release-4.1.md); the acceptance test to go through each time is [docs/de/abnahmetest.md](docs/de/abnahmetest.md).

### The parents' page

- **The settings are on three tabs**: Basics, School time and Advanced. Which key is on which tab is data of `config/schema.toml`; saving one tab keeps what the others hold. Host-tested and tried in the simulator.
- **Fixed: the settings form saved the interface language as the language of the texts** (and the child's name as the radio name): two keys of the same name were read from the wrong section. Host-tested.
- **School-time windows are compact and their days are ticked** (Mo to Su), from and to are time fields. Host-tested.
- **The Files tab hides what a Mac leaves on the card**, and "Remove Mac leftovers" deletes its `._` files; `energy.log` and `firmware.txt` are marked as files of 3.2. Host-tested.
- **The backup is a ZIP archive** of `settings.toml` and `contacts.toml`, byte for byte, and the restore works: in 4.0 the page sent it to a route the device does not have. A backup of 4.0 is still restored. Host-tested; checked with `unzip` against the simulator.
- **The tiles of the home screen's second page can be switched off** (`[home]`, tab Advanced).
- **A contact can be ticked "SOS"**: the SOS goes to every ticked contact; without a tick, to the first call button as before. Sent as soon as one text is out, failed only when all were refused. Host-tested.
- **A language comes in three answers** (words, labels, help) so that each keeps half of the device's response buffer free.

### The device

- **Back has one place on every screen**, on the left; the soft-key bar is 4 pixels taller.
- **The home screen has no arrows**; the page dots are the two halves of the strip under the tiles; the light is on the first page.
- **In a call**, Hang up is the whole bar and the volume has the buttons Quieter and Louder on the screen.
- **With free text**, the ready-made messages are behind the key Templates; typing starts at once.
- **The options can switch the device off** (two presses; not on the cable).

Host-tested and run in the simulator; not verified on the device.

## 4.0.2

- **The status line names the network again** ("Telekom", "Orange"). The modem answered the question for the operator with a blank, so the line said only "Network". The device now asks for the network's code when the name is blank and turns the code into the operator's name (Germany, its neighbours and the usual holiday countries; elsewhere the code). The log says `Cellular: network Telekom` when the network changes. Host-tested; verified on the device (2026-10-08).
- **"Connecting ..." while the device connects.** After switching on, the status line said "No network", "Network" and "No calls yet" in turn before the operator's name. It now says "Connecting ..." until the network and the calls are ready; a network lost later is "No network" as before, and the network page still says which part is missing. Host-tested; not verified on the device.

## 4.0.1

- **The display fonts are Adobe's Helvetica bitmaps** instead of Lucida Sans, whose licence grants their use but says nothing about distributing or changing them; Adobe's notice allows both. The sizes and the characters are the same; the letters are a little narrower, so some texts take fewer lines. Host-tested; not verified on the device.
- **A text that was scrolled and then gets more room shows its first lines again.** Before, the lines above the scroll position stayed hidden although the whole text fitted. Host-tested.
- **The French warning after a wrong PIN is shorter** ("PIN faux. Ne pas redémarrer ...") and fits the page without scrolling. Host-tested.
- **The firmware is built for PlatformIO's own ESP32-S3 board** with the settings of the T-Deck Pro MAX, instead of a board description copied from another project. The build settings and the partition table are the same as before. Compiled; not verified on the device.

## 4.0.0

Version 4.0 is a new code base, written from the start in English, layered and tested on a laptop. It replaces the 3.2 firmware. The release notes with the owner's checklist are in [docs/release-4.0.md](docs/release-4.0.md).

### What is new against 3.2

- **The interface speaks English, German and French.** The language is `[interface] language` in `settings.toml` or the page **Language** on the device; the language of the texts the device sends to the parents is `[sms] language`. A further language is one catalog file on the card (`/lang/xx.toml`, at most 32,768 bytes) and needs no new firmware; [docs/translating.md](docs/translating.md) goes through it step by step. The parents' page has its own catalogs in the same three languages.
- **Only the contact book may call and write.** Calls and texts from numbers that are not in `contacts.toml` or on a call button are dropped without the child learning of it, and there is no switch for it. A text from the provider is dropped too and kept in the log.
- **Calls over VoLTE are set up by the driver.** The modem driver defines the data context 1 as `IPV4V6` and cycles the radio once when it differs, so that the IMS registers; it never dials without a registered IMS, does not believe a "call begun" report on its own and survives a restart of the modem during a call. The operator's access point name is `[modem] apn`.
- **The parents' page was rewritten.** Tabs for status, settings, contacts, secrets, files, log and backup; every file is checked before it is stored; five wrong passwords lock the page for sixty seconds; the password has at least 4 characters.
- **The battery gauge is checked at every start.** The chip ships with a wrong battery size; the device compares it with `[battery] capacity_mah` (default 1500) and writes it only when it differs. A percentage that the cell voltage contradicts is shown as "unknown".
- **New pages and values:** the network page shows whether the IMS is registered, the position page shows the last known position with its age, the status page shows the configuration problems it found, and a self-test page checks the board.
- **The USB console** (`scripts/deviceconfig.py`, `scripts/webbridge.py`) reads and writes the three configuration files with a checksum and a safe write, reads the logs, sends a text by hand and prints the diagnosis of the fuel gauge and the charger.
- **A school-time window can be switched off.** Each `[[quiet_hours]]` window has `enabled` (default `true`, so every existing card locks as before) and the parents' page shows it as a checkbox "active". A card without any window gets one built-in window, Monday to Friday 08:00 to 12:30, switched off: no school time runs until a parent ticks it. A switched-off window is ignored everywhere ([docs/quiet-hours.md](docs/quiet-hours.md)).
- **A simulator and generated pictures:** the whole application runs on a laptop on fakes of the hardware (`pio run -e sim`), and `scripts/make_demo.py` renders every page in three languages and a click-through demo.

### What changed in the files

A 3.2 card is read by 4.0 without changes ([docs/upgrading.md](docs/upgrading.md) goes through the upgrade and the way back); what is reported and ignored is this:

- `[mesh]` is read as `[radio]` and reported once; `features.mesh` is not read (the key is `features.radio`);
- `display.font_scale`, `time.gps_utc_offset_minutes`, `child.role` and the top-level `screens` list are not read and are reported as unknown keys;
- `[[tiles]]` entries are call buttons only; a tile with another action is reported and ignored;
- German day names in school time (`mo-fr`) and a third window on one day, which 3.2 accepted, are refused and reported: 4.0 knows `mon-fri` and allows two windows a day;
- a card without `[interface] language` and `[sms] language` gets English for the screens and for the texts sent to the parents; the German of 3.2 is `language = "de"` in both;
- the message history `messages.log` keeps its format; entries of earlier versions that hold a sentence are shown as it is.

### What was removed

- **The radio is not part of 4.0.** The interface, the radio page and the message path exist and are tested against a fake, but there is no driver for the radio chip and no protocol of our own, so the feature cannot be switched on in a way that does anything: the tile is off and a request to start the radio is answered with "not available". The owner decided to leave it out of 4.0 and to add it in a later release ([docs/open-points.md](docs/open-points.md), item 9).
- **Save mode ("Sparmodus": radio only, cellular and Wi-Fi off), its cell probe and its notices.** It needs the radio, which 4.0 does not have, so it could only say "switch the radio on first". The owner decided to remove it completely; it is planned together with the radio, where an emergency radio mode will switch battery saving on implicitly ([docs/open-points.md](docs/open-points.md), item 9).
- **The hard-wired German interface words of 3.2.** German is now a catalog like any other.
- **The USB commands `dial`, `hangup`, `query` and `motion` of 3.2's tool.** The tool of 4.0 has `list`, `read`, `write`, `battery`, `sms` and `restart`.

### Known limits

- **Nothing of 4.0 has been verified on the device.** The logic runs more than 2,400 host tests and a simulator, and the firmware compiles. The display profile, the keyboard, the touch, the haptic motor, the fuel gauge, the charger, the receiver, the card, Wi-Fi, sleep and calls over VoLTE are checked against datasheets and measurements of the earlier firmware and by the host tests only. The owner's checklist [docs/hardware/bring-up.md](docs/hardware/bring-up.md) has 28 steps and its date and result columns are empty until the owner fills them.
- **The first picture on the panel is the first real test of the display profile** (steps 2 and 24 of the checklist); a wrong profile shows as an inverted, striped or cut-off picture.
- **A firmware binary is for the owner's own device.** It contains a sensor image of Bosch Sensortec whose redistribution terms are not verified, and LGPL code whose conditions for a published binary have not been assessed ([THIRD_PARTY.md](THIRD_PARTY.md), open points 13 and 14).
- **The wording of German and French was read by the author, not by a native speaker** ([docs/open-points.md](docs/open-points.md), item 15).
