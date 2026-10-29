# Decision log

The running log of decisions: what was chosen, what was refused, why, and how it is checked. It exists so that a decision made in a conversation does not live only in that conversation, and so that the owner and the agent both read and update it. The way of working is in [CLAUDE.md](../CLAUDE.md); open work is in [open-points.md](open-points.md), lessons measured on the device are in [hardware-lessons.md](hardware/hardware-lessons.md).

Entries are added at the end and are never rewritten or deleted. To change a decision, add a new entry that supersedes the old one; the only edits to an old entry are its status line and its "Verified" line, which says what has been tried since.

## Entry format

    ## D-<next number>: <short title>
    Date: <YYYY-MM-DD>   Status: decided | superseded by D-<n> | open
    Decided by: owner | agent (confirmed by owner on <date>)

    Context: the situation and what forced a choice, in two or three sentences.
    Options: each option in one line, with the main cost of it.
    Decision: what was chosen.
    Reason: why, in one or two sentences.
    Consequences: what this changes, what it rules out, and how it is checked
    (test name, bring-up step, open point number).
    Verified: host-tested | verified on the device on <date> | not verified

"Decided by" names the person who made the choice. An agent's proposal that the owner has not confirmed is never recorded as the owner's decision: its status is open and it is listed in [open-points.md](open-points.md).

## Entries

The first seven entries were written on 2026-10-11 from what the changelog and the open points already say; the dates of the original choices are not in the repository. Entries whose "Decided by" says that the owner has not confirmed them are waiting for that confirmation.

## D-1: The LoRa radio is not in 4.0
Date: 2026-10-11 (recorded afterwards)   Status: decided
Decided by: owner

Context: The interface, the radio page, the settings and the message path exist and are tested against a fake, but nothing drives the chip. A driver and a protocol of our own are needed because MeshCore is not used.
Options: Build the driver and protocol for 4.0 (delays the release, nothing to try it against yet). Leave it for a later release.
Decision: Not in 4.0; in a later release. Save mode (radio only, cellular and Wi-Fi off) was removed from 4.0 and comes back with the radio project, where an emergency radio mode switches battery saving on implicitly.
Reason: Not recorded beyond the owner's decision.
Consequences: The radio page stays on the fake until the driver exists. Open point 9 keeps the build for the later release; [device.md](hardware/device.md), [messaging.md](core/messaging.md) and [screen-radio.md](ui/screen-radio.md) describe what exists.
Verified: host-tested (against a fake); no driver for the chip

## D-2: Old school-time windows stay refused
Date: 2026-10-11 (recorded afterwards)   Status: decided
Decided by: owner

Context: A 3.2 card accepted German day names (`mo-fr`, `taeglich`) and three windows on one day. 4.0 refuses both and reports them, so the school time is then shorter than the card says.
Options: Keep accepting the old forms (two formats to maintain, ambiguity in a safety setting). Refuse them and tell the parents.
Decision: Both refusals stay.
Reason: Not recorded beyond the owner's decision.
Consequences: Parents write the windows anew in the 4.0 format (`mon-fri`, at most two windows a day) before the first start; the status page counts what is refused. See [upgrading.md](upgrading.md) and [quiet-hours.md](quiet-hours.md); open point 17.
Verified: host-tested

## D-3: A catalog on the card is limited to 32,768 bytes
Date: 2026-10-11 (recorded afterwards)   Status: decided
Decided by: owner

Context: Language catalogs are read from the card into the device's memory. The size of one file is a security limit.
Options: A larger limit (more memory risk on the device). 32,768 bytes for each file.
Decision: 32,768 bytes for each file.
Reason: Set by the owner as a security limit.
Consequences: Not to be weakened by the agent on its own initiative. To be looked at again with real memory numbers from the device (open point 11); [translating.md](translating.md) and [i18n.md](core/i18n.md) describe the limit.
Verified: host-tested. Memory numbers from the device (2026-10-12): about 8.3 MB of heap free; loading the three web catalogs, about 42 KB of text together, took about 106 KB of heap. A catalog of 32,768 bytes costs well under 100 KB, so the limit is far from the memory; it stays as the owner set it (a security limit).

## D-4: The APN stays explicit in the configuration
Date: 2026-10-11 (recorded afterwards)   Status: superseded by D-9
Decided by: agent (not confirmed by owner)

Context: The modem needs an APN to register the IMS. A built-in table by operator (MCC and MNC) or an empty APN with an `IPV4V6` context could remove the setting.
Options: A table: silently wrong for operators that are not in it. An empty APN with an `IPV4V6` context: not tried on a modem that was never configured. The explicit APN in the configuration.
Decision: Neither alternative is built; the explicit APN stays. This is the recommendation in the open points, not yet the owner's decision.
Reason: A wrong table entry fails silently, and a missing APN fails visibly.
Consequences: Waits for the owner (open point 10); what an empty APN does is open point 5. See [volte.md](modem/volte.md).
Verified: not verified

## D-5: The firmware switches off before the cell is damaged
Date: 2026-10-12   Status: decided
Decided by: owner (the direction); agent (the voltage, which the owner left to the agent on 2026-10-12)

Context: On 2026-10-09 the device ran on down to 3426 mV while the fuel gauge had said 0 % for hours. The owner assumed that a protection circuit guards the cell and does not want to open the case to look. The maker's page for the T-Deck MAX names the cell (3.7 V, 1500 mAh, so lithium-polymer, full at 4.2 V) and the chips (SY6970 charger, BQ27220 gauge), and says nothing about a protection circuit or a cut-off voltage. Neither charger nor gauge protects the cell ([battery.md](hardware/battery.md)).
Options: Rely on a protection circuit that nobody has shown to exist. Switch the device off by itself at a voltage with a margin above the damaging one (the phone is then unreachable earlier). Both.
Decision: The firmware switches the device off before the voltage reaches a damaging level, and the battery bar ends at that voltage, so the bar is empty when the device goes off. The voltage: off when the voltage on the battery stays below 3300 mV for 60 seconds (a minute, so that a dip while the modem sends does not switch it off), and 3300 mV is the empty end of the bar.
Reason: A deeply discharged cell is damaged, and 3300 mV is above the 3.0 V below which lithium-polymer cells are generally harmed, with margin; it is 130 mV under the 3430 mV where this cell was flat at 140 mA. With no cut-off in the documentation, the firmware must do it.
Consequences: Open point 18 (built; to be checked on the device). `kBatteryEmptyMv` (3430) and the voltage table in `BatteryPlausibility` change with it; the table was measured down to 3426 mV only, so below that it extends a measurement. The number is the agent's: if the device shows a dip that switches it off too early, a new entry changes it. The cell size of 1500 mAh in the firmware agrees with the maker's page for this model.
Verified: host-tested; compiled; on the device on 2026-10-13: the battery ran down overnight (about 105 mA) and the log has "Battery: 3291 mV for a minute, switching off to protect the cell", then "Power: switching off" at 07:07. A dip while the modem sends did not trigger it in 12 hours. The gauge said 0 % for four hours before that (open point 32).

## D-6: A text with an umlaut is sent in ASCII to stay one message
Date: 2026-10-12   Status: superseded by D-10
Decided by: agent (not confirmed by owner)

Context: The driver sends every text that is not plain ASCII as UCS2, which holds 70 characters per message, and cuts a longer text into separate messages because it has no concatenation header (that needs PDU mode, which is not built). On 2026-10-09 an SOS of about 130 characters arrived as three messages. The fix of 4.1.5 sends such a text in ASCII when that takes fewer messages (the umlaut becomes "oe", "ae" or "ue"). It applies to every outgoing text, not only to SOS. The owner doubts the diagnosis: umlauts normally go through SMS, and the cause may be another character, for instance in the map link. The map link of the default template is plain ASCII; of the default SOS wordings only the German pickup text (the word for "wants", with an o-umlaut) has an umlaut, and a child's name with an umlaut would add one.
Options: Keep the folding (built; it changes the child's words). Also write the pickup wording without an umlaut (cosmetic; names remain). Build PDU mode with the GSM 7-bit alphabet, which contains the umlauts and the sharp s, and with concatenation (the real fix; a larger project). Remove the folding.
Decision: None yet. The folding stays in 4.1.5 until the owner decides.
Reason: The cause of the three messages has not been confirmed for the owner's case: which text it was and which character made it UCS2 are needed.
Consequences: Open point 22. Texts that need a stand-in are shown unchanged in the history; only the message on the air is folded ([modem-driver.md](modem/modem-driver.md)).
Verified: host-tested; not verified on the device

## D-7: The home screen has a Lock button and no arrows
Date: 2026-10-11 (recorded afterwards)   Status: decided
Decided by: agent (confirmed by owner on 2026-10-12)

Context: Since 4.1.0 the arrows are gone from the home screen on purpose, and the Back key went with them, so nothing said how to lock the device.
Options: Bring the arrows back (the choice of 4.1.0 is undone). A visible Lock button in the bottom bar.
Decision: A Lock button in the bottom bar ("Lock", "Sperren", "Verrouiller"); its word never changes with the selection, so the bar costs no redraw. The arrows stay away.
Reason: The device must say how to lock it without redrawing the bar.
Consequences: Released in 4.1.6; [screen-home.md](ui/screen-home.md). The owner confirmed that it works. A real switch-off is a different thing and is not this button: see open point 23.
Verified: host-tested in all three languages; the owner used it on the device (2026-10-12)

## D-8: What to do about operations that block the main loop
Date: 2026-10-12   Status: open
Decided by: agent (not confirmed by owner)

Context: The loop in `device/main.cpp` is cooperative and single-threaded. The owner sees the screen blocked while the motion page starts the accelerometer, which takes about 11 seconds by the documentation and about 10 seconds by the owner's feel ([screen-motion.md](ui/screen-motion.md)). An outside proposal suggested moving the modem, the GNSS parsing and the card onto core 0 with command and event queues; the modem and the receiver are already non-blocking state machines, so that proposal did not address what was observed.
Options: Leave it: the page says what it does, but an SOS during the 11 seconds waits. Take only the sensor start off the main loop (a one-shot task polled through a flag): a small piece of concurrency. A general two-core split with queues: breaks the single-threaded, host-tested core, shares core 0 with the Wi-Fi stack, and has no measurement behind it.
Decision: None yet. Measure the longest loop pass and the key-to-reaction time on the device first; then choose.
Reason: No measurement shows that the loop as a whole is too slow, only that one known operation blocks.
Consequences: Open point 20. The general split is not started. Nothing changes in the code until the owner chooses.
Verified: not verified

## D-9: The APN is chosen automatically from a built-in list by operator
Date: 2026-10-12   Status: decided
Decided by: owner

Context: The network does not tell the device which APN to use; the device brings one ([volte.md](modem/volte.md): the APN differs from operator to operator). Today it must be typed into `settings.toml`, and ordinary users do not know their APN and should not have to. Phones solve this with a list keyed by the SIM's MCC and MNC.
Options: The explicit APN only (D-4: a parent must know it). A built-in list by MCC and MNC (right for the operators in it, silently wrong for the others). An empty APN with an `IPV4V6` context (tried on no modem that was never configured). Fetching the list over the network at run time (the device has no data connection before it knows the APN).
Decision: The firmware carries a list of operators with their APNs and chooses the entry that matches the SIM when it is inserted. The user sets nothing. `[modem] apn` in `settings.toml` stays as an override for the rare case, and an operator that is not in the list gets what the device does today (the modem's own contexts are left alone) and one log line that says so. This supersedes D-4.
Reason: A device for children and parents must work with the SIM they put in. The list is data, which a public source can supply.
Consequences: Agent's proposal for how: a script writes the list into the source at build time from a maintained public list (the Android Open Source Project's `apns-conf.xml` is Apache-2.0 and would be named in [THIRD_PARTY.md](../THIRD_PARTY.md)), with a test; an update of the list comes with a firmware update. No entry is written by hand from memory. The IMS bearer (context 8, APN "ims") is set up by the modem and is not part of the list. Open point 21.
Verified: host-tested; compiled; on the device on 2026-10-12 with the APN field cleared on the parents' page: "APN internet.v6.telekom chosen for the operator 26201", then "IMS registered". A call with that APN has not been made. Built from the sample file apns-full-conf.xml of the Android project (1146 operators); the file named above, apns-conf.xml, was not found at the addresses tried.

## D-10: Texts go out in PDU mode, in the GSM alphabet, as one concatenated message
Date: 2026-10-12   Status: decided
Decided by: agent (the owner left the choice to the agent on 2026-10-12 and wants to test it)

Context: The driver sends every text that is not plain ASCII as UCS2, which holds 70 characters, and cuts a longer text into separate messages because it has no concatenation header (that needs PDU mode). A German umlaut therefore halves the length and may split the message, although the umlauts, the sharp s and the common accented letters are in the GSM alphabet and cost nothing there. The fix of 4.1.5 (D-6) hides this for some texts by writing "oe" for the umlaut. The owner does not want a text shortened or changed because it has an umlaut.
Options: Keep the folding to ASCII (changes the child's words). Rewrite single wordings (names still bring umlauts). PDU mode with the GSM 7-bit alphabet where every character is in it, UCS2 where not, and a concatenation header for long texts (a larger piece of work in the driver). Remove the folding and accept the split.
Decision: PDU mode. A text whose characters are all in the GSM alphabet goes out as 7-bit (160 characters, 153 per part when long), any other as UCS2 (70, or 67 per part), and long texts as one concatenated message that the receiving phone shows as one. The folding of D-6 is removed when this works.
Reason: It removes the cause instead of working around it, and the words of an SOS stay as written.
Consequences: Open point 22 with its "done". The folding to ASCII of 4.1.5 is removed with it. Receiving stays as it is. The A7682 must accept PDU sending with `AT+CMGF=0`; that is the first thing to try on the device. The driver now logs "Messages: sent in N parts, GSM 7-bit" or "UCS2" for every text, which the system log lacked on 2026-10-09 when an SOS arrived as three messages.
Verified: host-tested; compiled; on the device on 2026-10-12 (owner): a text of 127 characters with an umlaut went out as "1 part, GSM 7-bit" and arrived as one message with the umlaut right; a text of 181 characters with umlauts went out as "2 parts, GSM 7-bit" and arrived complete.

## D-11: A switch-off button on the touch screen, with a question
Date: 2026-10-12   Status: superseded by D-12
Decided by: owner (button, question, place near the top); agent (the exact place and look)

Context: The device is often switched off in the pocket and switched on when it is used, which makes the battery last. The switch-off that exists sits in the second page of the home screen under Settings and on the status page, which the owner finds too hidden. The micro switch on the right of the case is on the charger chip; the firmware never reads it, and the owner knows of no signal from it, so "press twice, then ask" on that switch is not possible.
Options: Leave the menu entry. A tile on the second page of the home screen (hidden, and at the bottom of the screen like the page dots). A touch button near the top of the home screen that opens a question. Read the side switch (not possible as far as is known).
Decision: A switch-off button near the top of the home screen, at the right end of the status bar. A tap opens a page that asks "Switch off?" with the two choices Switch off and Cancel, in the three languages; Switch off shows the "Switched off" picture and switches the device off as the menu entry does. The existing menu entry stays.
Reason: It is where a hand looks first, away from the soft keys where the child moves between pages, and the question keeps a tap in a pocket from switching off.
Consequences: Open point 23. The status bar is crowded (operator, GPS note, battery, date), so the place is checked on a picture from the simulator before it is built; the date gives up room, not the battery. A locked device ignores taps, so the button is reached only after unlocking. The switch-off still refuses on the cable, and the page says why. The side switch is open point 24.
Verified: not verified; not built

## D-12: A power tile on the first page of the home screen, with a choice
Date: 2026-10-12   Status: decided
Decided by: owner

Context: D-11 put a button in the status bar. The owner meant something else: a tile like the others, below the status bar, on the first page, that opens a choice. The first page has six tiles of nine places.
Options: A button in the status bar (D-11; crowded). A tile on the second page (hidden). A tile on the first page.
Decision: A tile "Power" on the first page. It opens a page with the choices Switch off, Power save (D-13) and Cancel. Switch off asks once more ("Switch off?") before it acts, and then shows the "Switched off" picture. The menu entry under Settings stays. This supersedes D-11.
Reason: It is findable, it is where the other things the device does are, and the question keeps a tap from switching off.
Consequences: Open point 23. Built as the tile `home.power` (the last of the first page) and the page `PowerScreen` with the choices Switch off (asks "Switch off?", Cancel in focus) and, for later, Power save (D-13, not on the page yet). The switch-off still refuses on the cable and says why. The tile is on the child's page; the question is what protects against a tap.
Verified: host-tested; compiled; the tile was used on the device on 2026-10-12 and switched the device off; the picture "Switched off" is right and the question "Switch off?" with Cancel is shown and works (owner, 2026-10-12). The cable text is not yet looked at.

## D-13: What "Power save" does
Date: 2026-10-12   Status: open
Decided by: agent (not confirmed by owner)

Context: The owner wants Power save as a choice next to Switch off. The device today never saves power by itself: `modem_sleep = false` is set in the owner's `settings.toml` (the default is true), so the policy's rule 3 never lets the modem or the processor sleep, and the energy log of 2026-10-12 shows only "awake" and "cable", about 130 mA, which empties 1500 mAh in about eleven hours. Why the setting is off is not recorded; the energy log of 2026-10-09 names it as the reason for a fast discharge.
Options: Power save switches the modem and processor sleep on until the device is woken (what `modem_sleep = true` does; reachable by SMS and call through the ring line). Real deep sleep of the processor with a wake on the ring line and the keys (lowest current; more to build and to test). Leave Power save out of 4.2.0 and first find out why `modem_sleep` was turned off.
Decision: None yet. Recommendation: the third, because with modem sleep off nothing a new button does would save power, and that fault is worth finding first.
Reason: A button that promises to save power while the setting underneath forbids it would be wrong.
Consequences: Open point 25.
Verified: not verified; the 130 mA is read from the energy log on the device

## D-14: The charge current is limited to 0.65 C and the charger's watchdog is off
Date: 2026-10-12   Status: decided
Decided by: owner (the limit must come, and he charges with a mains adapter); agent (0.65 C, the watchdog)

Context: The diagnosis of 2026-10-12 showed the SY6970 on its defaults: a charge current of 2048 mA, more than 1 C for the 1500 mAh cell, and a watchdog that nobody feeds, which sets every setting back to the default every 40 seconds. On the computer's USB port the input limit of 500 mA kept the current low; on a mains adapter it would not.
Options: Leave the defaults (cell charged above 1 C). Set the current and feed the watchdog from the main loop (a task that, if it stops, lets the chip fall back to 2048 mA). Set the current and switch the watchdog off (the value stays whatever the processor does).
Decision: At every start the firmware sets the charge current to 0.65 C of the configured capacity, rounded down to 64 mA (960 mA for 1500 mAh), and switches the watchdog off. Each register is written only on a difference and read back.
Reason: A cell that is charged too fast ages and, in the worst case, is a safety risk; with the watchdog off the limit cannot silently disappear while the firmware hangs.
Consequences: Charging from empty takes a little longer with a strong adapter (about 1.5 hours instead of about one). The register map is the BQ25896's, which the SY6970 is documented to follow; its data sheet is not in the repository, so a write that does not read back is logged and the charger keeps its own value. Open point 26 keeps the check on the device.
Verified: host-tested; on the device on 2026-10-12 the chip reads back 960 mA and the watchdog off (register 07 = 0x8D). With the cable the charger still limits the input to 500 mA (register 00 = 0x48, "unknown adapter"): the cell charged at 445 to 500 mA from the computer and from a mains adapter, so the limit of 960 mA was not reached; the fault register 0x90 reads the same before and after, so it was no watchdog fault.

## D-15: The modem firmware is not updated
Date: 2026-10-12   Status: decided
Decided by: owner

Context: The modem firmware `A7682M7_B15V01_241119` hangs in the call setup when the IMS is not registered. The driver works around it by never dialling without the IMS ([volte.md](modem/volte.md)). LilyGO answered on 2026-10-12 that a newer firmware for the A7682E exists, installed with a tool shared with the A76XX series over the modem's USB endpoints, which on the T-Deck Pro are inside the case, and that they advise against an update unless there is a specific problem.
Options: Open the case and update the modem firmware (risk to the case and to a working modem; the effect on the hang is unknown). Leave the firmware as it is and keep the driver's workaround.
Decision: The modem firmware is not updated now. The vendor's answer and the link to the instructions are recorded in volte.md.
Reason: Opening the case is not needed while calls and SMS work with the workaround, and the vendor does not recommend an update without a specific problem.
Consequences: The question to the manufacturer (open point 8) is answered and removed. The decision is looked at again if the hang returns in a way the workaround does not cover, or when the case is open for another reason.
Verified: not applicable; the vendor's statement is taken from the owner's message and has not been checked against the vendor's page

## D-16: The parents' page accepts request headers up to 4096 bytes
Date: 2026-10-12   Status: decided
Decided by: owner (it must be fixed); agent (4096 bytes)

Context: Saving a setting (modem sleep) from the parents' page in a browser answered "Request Header Fields Too Large" (431). The parser refused a head of more than 1024 bytes, and a browser's head with the Authorization header, Origin, Referer, the user agent and the cookies it holds for the address is easily larger. The USB bridge's request limit follows the parser's.
Options: Leave 1024 (saving from a browser stays unreliable). 2048 (a few cookies still pass it). 4096, a common server default.
Decision: `http::kMaxHead` is 4096; the bridge's limit (`kConsoleMaxRequest`, `MAX_REQUEST`) is 12300, the head and the largest body.
Reason: It lets ordinary browsers through and costs 3 KB of the parser's buffer (the firmware uses 14.5 % of its RAM).
Consequences: A head over 4096 bytes is still refused with 431, with a test. Verified in the browser only after flashing (open point 27).
Verified: host-tested; compiled; saving a setting from a browser over the home network worked on the device on 2026-10-12 (owner: the right message was shown).

## D-17: Reading the device and updating its firmware over Wi-Fi
Date: 2026-10-12   Status: superseded by D-18
Decided by: agent (a recommendation the owner asked for)

Context: The owner charges the device away from the computer and cannot then read the logs or the battery diagnosis, and a firmware update always needs the cable. He asked whether an update over the air (OTA) and the console over Wi-Fi make sense. What exists: the partition table (`default_8MB`) already has two application slots of 3.2 MB (`ota_0`, `ota_1`) and `otadata`, and the firmware is 1.5 MB, so no repartitioning is needed. The parents' page is plain HTTP on port 80 with a password in a Basic header (no TLS), one connection at a time, and a request body of at most 8 KB in a buffer ([web-interface.md](web/web-interface.md)).
Options: (A) Routes on the parents' page, behind the same password and the same `X-Phone-OS` header: the battery diagnosis, a longer log tail, the energy log. (B) The raw USB console on a TCP port: it has no login, and it can write files and send SMS. (C) OTA through the parents' page: a streamed upload (the parser holds a whole request in memory today), written to the other slot, a signature of the image checked against a public key in the firmware, a reboot, and a return to the old slot if the new firmware does not mark itself good; refused on a low battery, in a call and during an SOS. (D) OTA with the password only: anybody on the Wi-Fi who sees the password (HTTP, Basic) could install any firmware on a child's phone.
Decision: None yet. Recommendation: A in 4.3 (small, solves today's problem); C later as a project of its own, with the signature; not B; not D.
Reason: Reading is low risk behind the existing password. Writing firmware is the one action that must not depend on a password sent in the clear, and a device that fails to boot after an update is a child without a phone, so the return to the old slot and the signature are the work, not the upload.
Consequences: Open point 28. For C the owner decides where the private signing key lives (not in the repository). The first firmware with OTA has to be flashed by cable like every other. Nothing is built.
Verified: not applicable

## D-18: Read routes on the parents' page in 4.3; an update over the air is written down and not built
Date: 2026-10-12   Status: decided (the read routes are dropped by D-19; the rest stands)
Decided by: owner

Context: D-17 asked whether to read the device over Wi-Fi and to update its firmware over the air. The owner charges the device away from the computer and wants to read logs and the battery diagnosis without the cable.
Options: As in D-17: (A) read routes behind the password, (B) the raw console on the network, (C) a signed update over the air, (D) an update over the air with the password only.
Decision: (A) is built in 4.3. An update over the air is **not** built; it stays here, with its objections, so that it can be thought through and done later. (B) and (D) are refused.
Reason: Reading is low risk behind the existing password. The objections to an update over the air, kept for later: the parents' page is plain HTTP with a password in a Basic header, so anybody on the Wi-Fi who sees it could install any firmware on a child's phone; a device that does not boot after an update is a child without a phone, so a signature (a public key in the firmware, the private key outside the repository, the owner's to keep) and a return to the old application slot when the new firmware does not mark itself good are the work, not the upload; the parser holds a whole request in memory (8 KB), so the upload needs a streaming route of its own; an update is to be refused on a low battery, in a call and during an SOS; and the first image with an update function has to be flashed by cable like every other. The partition table already has the two slots (`ota_0`, `ota_1` of 3.2 MB; the firmware is 1.5 MB).
Consequences: Open point 28 is the read routes: the battery diagnosis, a longer log tail and the energy log, with the same password, the `X-Phone-OS` header and the lockout as every route. No raw console and no update function are built. A later decision that takes up the update over the air supersedes this one.
Verified: not applicable

## D-19: No read routes on the parents' page
Date: 2026-10-12   Status: decided
Decided by: owner

Context: D-18 decided read routes for 4.3 (the battery diagnosis, a longer log tail, the energy log over the parents' page). The owner then said that reading the battery over the network was only an idea: it comes up once, and the cable does it.
Options: Build the read routes (a small piece of work and a new surface behind the password). Do not build them and read over the cable.
Decision: The read routes are not built. The rest of D-18 stands: an update over the air is written down with its objections and not built; the raw console on the network and an update with the password only are refused.
Reason: The need was a single occasion, and every route on the parents' page is code that has to be kept safe.
Consequences: Open point 28 is removed. If the need comes back, a new entry takes up D-17 (the options) again.
Verified: not applicable

## D-20: Measure the loop and the sleep before changing them
Date: 2026-10-12   Status: decided
Decided by: owner

Context: D-8 (does anything block the main loop) and the open question of the energy (about 105 mA with the modem asleep, where a sleeping processor should draw far less) both lack numbers. The owner said: a small version 4.2.1, "we have to measure this".
Options: Guess and change the code (a task for the sensor start, another sleep policy). Add two lines to the log and read them after a day.
Decision: Version 4.2.1 writes one line every half hour to the system log with the longest pass of the loop, the number of passes and the share of the time spent asleep, and logs the three language requests of the parents' page. It changes no behaviour.
Reason: D-8 recommends measuring first, and the owner agreed; the same line answers where the 105 mA go.
Consequences: Open points 20, 25 and 30 are read from these lines after a day on the device. The half hour keeps the system log from filling (48 lines a day); the line is kept on the levels `all` and `events`. A decision about the sensor start, about the sleep policy or about the page follows from what the lines show.
Verified: host-tested; compiled; on the device the loop line is written every half hour (2026-10-13): the loop makes about 32,000 passes in 1800 s and sleeps 1 % of the time, the longest pass was 123 to 206 ms (1479 ms in the first window, the start). The motion page was not opened, so the 11 seconds of the sensor start are not in these lines yet.

## D-21: The Power tile shows the power symbol, not a word
Date: 2026-10-12   Status: decided
Decided by: owner

Context: The tile is called "Strom" in German ("Power" in English, "Energie" in French). The owner finds "Strom" a poor translation. A tile on the first page can carry a picture (phone, text, SOS, inbox, radio have one).
Options: Another word ("Aus", "Ausschalten" are too long for 68 pixels, "Power" is English). The power-off symbol, the circle with a bar, which needs no translation.
Decision: The tile shows the power symbol. The words stay in the catalogs for the page behind it (title and the buttons).
Reason: A symbol needs no translation and is known from every device.
Consequences: Open point 31. Not built; it must be looked at whether the fonts have the glyph (U+23FB) or whether it is drawn like the other symbols (`src/ui/Symbols.h`). The tile keeps its label key `home.power` for the tests and for the self-test words.
Verified: host-tested; compiled; pictures reviewed (the symbol is drawn in four sizes and centred on its tile); not verified on the device. In 4.2.2 the label key `home.power` was removed, because a tile without a word needs none (the Consequences above said it would stay).

## D-22: The self-test words stay in the device catalog
Date: 2026-10-13   Status: decided
Decided by: owner

Context: The words of the self-test page (45 keys) are in the device catalogs, although only a parent sees that page. The question was whether to move them to the page's catalogs.
Options: Keep them in the device catalog (the page runs on the device and needs them without the card). Move them (smaller catalogs, but the page needs the card).
Decision: They stay. The owner agreed on condition that they are not irritating and do not take much room.
Reason: They take 1.7 to 1.9 KB of a catalog of 14.7 to 16.4 KB (about 12 %), the device has about 8 MB of memory, and nothing outside the self-test page shows them.
Consequences: Open point 12 is removed.
Verified: not applicable

## D-23: No firmware binary is handed out
Date: 2026-10-13   Status: decided
Decided by: owner

Context: Open point 14 held back a binary until the redistribution terms of the Bosch image of the motion sensor were verified, and the Arduino core (LGPL) added a question of its own.
Options: Verify the terms and publish a binary. Hand out no binary at all.
Decision: No firmware binary is handed out, for reasons of responsibility: the device is a child's emergency phone, and a binary in other hands is a device whose behaviour nobody here answers for. The source is public; whoever builds the firmware answers for the result.
Reason: The owner's: responsibility. It also makes the licence question of the binary moot.
Consequences: Open point 14 is removed. The releases carry source, changelog and notes, and no binary. [THIRD_PARTY.md](../THIRD_PARTY.md) keeps its remark on the Bosch image for whoever builds from source.
Verified: not applicable

## D-24: The battery percentage trusts the voltage over a gauge at 0 %, with a margin on the way back
Date: 2026-10-13   Status: decided
Decided by: owner (go for the fix, 2026-10-13); agent (the numbers)

Context: Overnight the gauge said 0 % at 3579 mV and the device ran four hours more, to the switch-off at 3291 mV. The rule of 4.1.4 shows the voltage's number only when the gauge is 30 points or more below it, which the voltage gives only above about 3579 mV, so below it the screen said 0 % for hours; and at the border it flipped about once a second, in the log and on the screen. The gauge has not learned this cell (cycle count 2).
Options: Leave it. Only add a margin on the way back (no flapping, but 0 % for four hours remains). Add the margin, and also distrust a gauge that says almost nothing while the voltage is clearly above empty.
Decision: The third. A gauge at 3 % or less is not believed while the voltage gives 6 % or more; once the voltage's number is shown it stays until the gauge is within 15 points of it, or, for an empty gauge, until the voltage gives less than 4 %.
Reason: The device switches itself off at 3300 mV (D-5), so a cell above it is not empty whatever the gauge says; the margin ends the flapping.
Consequences: The bar and the percentage on the screens follow the voltage in the last hours, and the log has one line per change. The numbers (3, 6, 15, 4) are the agent's and can be changed with a new entry. Open point 32.
Verified: host-tested (five tests fail with the old rule put back); compiled; verified on the device on 2026-10-14: in the night of 2026-10-13 the log has one line at 20:19 ("the gauge says 7 % at 3624 mV; showing about 43 % from the voltage") and one at 00:55 ("the gauge agrees with the voltage again"), none in between, and the screen followed the voltage to the switch-off at 3291 mV; again on 2026-10-21 (4.2.6): one line at 01:31 ("the gauge says 7 % at 3615 mV; showing about 41 % from the voltage") and "the gauge agrees with the voltage again" at 09:33 when the cable came, 7.7 hours and about 470 mAh after the gauge's 0 %

## D-25: The loop line tells where the time of a pass goes
Date: 2026-10-13   Status: decided
Decided by: owner (go, 2026-10-13)

Context: The Loop line of 4.2.1 showed about 32,000 passes in 1800 s (56 ms each), 1 % sleep, and the longest pass 123 to 206 ms. It does not say where the 50 ms of work go or whether the power policy allows sleep at all.
Options: Guess (the touch chip, the e-ink drawing, the card). Time the parts of the pass and count the passes in which sleep is allowed.
Decision: The second: the line gives the share of passes in which sleep was allowed and the average cost per pass of the console, the keys, the modem, the application step and the housekeeping, and what is left.
Reason: A guess about 100 mA of current is worth less than one day of numbers; the cost is a few timer reads per pass.
Consequences: Open points 20 and 25 are read from the line after a day. What the numbers show decides what is changed (the sleep policy, a part of the loop, the I2C clock); nothing is changed before.
Verified: host-tested; compiled; verified on the device on 2026-10-14: 25 lines in the night of 2026-10-13, each "sleep allowed in 100 % of the passes; per pass (ms) console 0.0, keys 2.8, modem 0.1, step 52.0, housekeeping 0.2", 32,000 passes in 1800 s, asleep 1 %

## D-26: The parents' page keeps the password for the tab only
Date: 2026-10-13   Status: decided
Decided by: owner

Context: The page stored the Basic credential in `localStorage` after a login, so that a reload did not ask again. `localStorage` outlives the visit, and the page's origin is `http://192.168.4.1` on the device's own access point, the address every ESP32 access point has: a page served at that address later, by any device, could read the credential. The documentation said "in memory", which was not what the code did.
Options: Leave it (no re-login after a reload, the credential stays on the phone for good). Keep it in memory only (every reload asks again). Keep it in `sessionStorage` (a reload in the same tab does not ask, closing the tab forgets it).
Decision: `sessionStorage`.
Reason: A reload is common while editing, closing the tab is the end of the visit; nothing of the password stays on the phone afterwards.
Consequences: `scripts/gen_web.py` and the generated `web/index.html`; `test/test_web_page/test.mjs` (f2) pins that the credential is in `sessionStorage` and that the script never names `localStorage` with it. The page's language choice (`pp_lang`) stays in `localStorage`, it is not a secret.
Verified: host-tested; not verified on the device

## D-27: The licence names the owner as the copyright holder
Date: 2026-10-13   Status: decided
Decided by: owner

Context: `LICENSE` said "Copyright (c) 2026 Phone OS contributors". Most commits are by an agent under a noreply address and the rest by the owner; "contributors" without a name is nobody a third party can ask, and the review of 2026-10-13 pointed it out.
Options: Leave it. Name the owner. Name the owner and the contributors.
Decision: "Copyright (c) 2026 Paul Wilke and the Phone OS contributors"; the MIT text is unchanged.
Reason: Somebody is named who can answer for the work, and whoever contributes later is covered by the same line.
Consequences: `LICENSE`, `THIRD_PARTY.md`. In the same change the Apache-2.0 text that the operator list requires is stored (`assets/licenses/LICENSE-Apache-2.0.txt`) and SensorLib is pinned to 0.5.0, so that nothing the build fetches changes its licence unread.
Verified: not applicable

## D-28: The USB console asks for no password; the risk is written down, a restriction is open
Date: 2026-10-13   Status: decided
Decided by: owner

Context: The console on the USB port (`CONFIG READ`, `CONFIG WRITE`, `CONFIG RESTART`, `SMS`) answers any computer the device is plugged into: it can read `secrets.toml`, write the contacts and send a text in the child's name. It is the owner's tool, and the review of 2026-10-13 pointed out that a child may plug the device into a computer elsewhere. Only `CONFIG WEB` carries a password, the page's.
Options: Leave it as it is and say so. Answer only while the device is unlocked. Answer only in the first minutes after a start. Ask for the web password on the lines that write or send.
Decision: The first, today: the console stays as it is, and the documentation (device-console.md, the user's guide in three languages) says that it asks for no password. A restriction is open point 33 and comes with a version of its own.
Reason: The console is how the device is read and set up today, and a restriction changes the owner's tools; it is worth thinking through, not doing on the side.
Consequences: No code changes. Open point 33 names the choices.
Verified: not applicable

## D-29: The access point's random password stays eight digits
Date: 2026-10-13   Status: decided
Decided by: owner

Context: Without a stored password the access point gets eight random digits, shown on the screen only. Eight digits are 10^8 choices, which a recording of the WPA2 handshake can try through offline in minutes; whoever gets in reads the page's password in clear, because the page speaks HTTP.
Options: Leave it. Ten digits. Eight characters of letters and digits (harder to type from the screen).
Decision: Leave it.
Reason: The access point runs for minutes while somebody sets the device up and switches it off; the page behind it asks for its own password; a parent who wants more sets one of up to 63 characters in `secrets.toml`. The owner judges the risk as small next to the cost of typing.
Consequences: No code changes; wifi-controller.md says why.
Verified: not applicable

## D-30: The JSON bundle of 4.0 is no longer restored
Date: 2026-10-13   Status: decided
Decided by: owner

Context: 4.0 exported a JSON bundle; 4.1 made the backup a ZIP archive and kept reading the bundle "so that a backup of 4.0 can be restored once". The review of 2026-10-13 found that this path read two buffers that nothing had set when a key of the bundle was missing, and that nothing else in the code uses the JSON string helper it needed.
Options: Fix the buffers and keep the path. Remove the path.
Decision: Remove it. A restore sends a ZIP archive and nothing else; the page's file choice offers `.zip` only.
Reason: The owner's words: a leftover; the backup is the ZIP archive of the whole files now. Nobody is known to hold a bundle of 4.0, and a bundle that somebody holds is a text file whose two values can be put into the editor by hand.
Consequences: `WebApi::restoreJson`, its two buffers and `web::json::getString` are gone; a body that starts with `{` is refused as "not a ZIP file" like any other text (`test_web_api`); the format name of the bundle is no longer a name the code reads (`scripts/check_docs.py`, `test_check_docs`).
Verified: host-tested

## D-31: The core's state is not built every pass, and a battery reading is kept for a second
Date: 2026-10-14   Status: decided
Decided by: owner (go, 2026-10-14: "build everything that is possible"); agent (the second option, the second as the time)

Context: The loop lines of the night of 2026-10-13 (D-25) say that the application step costs 52 ms of every 56 ms pass, with the modem asleep and nothing to do. `Runtime::followCalls` asks `core_.state()` every pass for the call state, and `AppCore::state()` reads the battery percentage, the voltage, the minutes, the charge state and the cable, each a reading of the fuel gauge and the charger over I2C at 100 kHz: about thirty transactions, which is the 52 ms. `PowerCoordination::apply` asks the charger once more. The processor cannot save what the loop spends.
Options: Leave it and raise the I2C clock to 400 kHz (a quarter of the cost, still every pass). `followCalls` asks the modem's call state alone, and `BoardPower` keeps its reading for a second, so that five questions in one pass are one reading (the step falls to the few milliseconds of the pages). Read the chips only when a page is on or a line is due (more to get wrong).
Decision: The second: `followCalls` asks `AppCore::callState()`, and `BoardPower::setClock` lets one reading serve every question for a second (`kReadingTtlMs`).
Reason: The reading changes nothing in a second, and the status page and the energy log keep their numbers; the change is two small ones with tests.
Consequences: Built in 4.2.4 ([device-power.md](hardware/device-power.md)); the loop line of the next night is the check: the step under 5 ms. Open point 25 says what to read.
Verified: host-tested (`test_board_power`: five questions cost one reading); verified on the device on 2026-10-15: the step costs 0.4 ms of a pass (52.0 before) in every loop line of the night of 2026-10-14

## D-32: The gauge's learned full charge is left to the gauge
Date: 2026-10-14   Status: decided
Decided by: owner (2026-10-14: the size is entered once, the gauge learns the rest, "we only interfere")

Context: After the first full discharge the fuel gauge reports a learned full charge of 1372 mAh next to the design capacity of 1500 mAh, and every start logs "wrote 1500 mAh to the fuel gauge but it reports design 1500 mAh, full charge 1372 mAh": the rule of D-14 writes on a difference, the gauge learns the difference back, and the write repeats at every start. The gauge's memory wears with every write, which the rule was meant to prevent.
Options: Leave it. Write the design capacity only and compare only it; show the learned full charge in the diagnosis. Write both only while the cycle count is 0.
Decision: The second: the design capacity decides whether anything is written; when it is written, the full charge is seeded with the same size, and afterwards it is the gauge's alone.
Reason: The learned value is the gauge doing its job; the night showed about 1340 mAh from 92 % to the switch-off, near what it learned. Whether the cell is a 1500 or a 1400 mAh one, the owner enters once in `[battery] capacity_mah`.
Consequences: Built in 4.2.4: the start line says "nothing to write; it has learned a full charge of 1372 mAh", the diagnosis shows both as before (`test_fuel_gauge`).
Verified: host-tested; verified on the device on 2026-10-15: the start line names the learned 1372 mAh and writes nothing; the diagnosis shows cycle count 1; seen on 2026-10-21: the learned full charge fell to 1107 mAh after the discharge of 2026-10-19 to 20 and to 988 mAh after that of 2026-10-20 to 21, each ending at the gauge's 7 % at 3.63 V (hardware-lessons.md, open point 3, D-42)

## D-33: The sleep is measured before it is changed
Date: 2026-10-14   Status: decided
Decided by: owner (go for building everything, 2026-10-14); agent (what is measured)

Context: In the night of 2026-10-13 the power policy allowed the processor to sleep in every pass and it slept 1 % of the time (D-25). The sleep is a light sleep with a timer and three wake lines that wake on a low level (the keyboard's interrupt, the touch chip's, the modem's ring line); a line that is low before the sleep ends it at once, and nothing in the log says which.
Options: Change the wake lines to edges blind (the ring line is the suspect). Measure first: before every sleep read the three lines, after it read what ended it, and count both for the loop line.
Decision: Measure: the loop line gets "sleep: N times, ended by the timer, by a line, other; low before sleep: keyboard, touch, ring".
Reason: The same reason as D-20: one night of numbers beats a guess, and the cost is three pin reads per pass.
Consequences: `device/Sleep.h` reports, `board::LoopStats::sleep` counts (`test_loop_stats`); open point 25 says what to read. What the numbers show decides the change (an edge instead of a level, a line not used for waking, or the modem's ring line configured differently).
Verified: host-tested; compiled; verified on the device on 2026-10-15: the night of 2026-10-14 says that the keyboard's interrupt and the ring line read low before all 398,000 sleeps and that none was ended by the timer or a line; the change is D-34

## D-34: The wake lines get pull-ups and are armed with the level they do not have
Date: 2026-10-15   Status: decided
Decided by: owner (the standing go of 2026-10-14 to build what the night shows); agent (what)

Context: The measurement of D-33 says that the keyboard's interrupt line and the modem's ring line read low before every one of 398,000 sleeps in the night of 2026-10-14, that none of the sleeps was ended by the timer or a line, and that the "asleep 23 %" was the going down and coming up of a sleep that never lasted, at the same 105 mA. The keyboard chip's interrupt is open-drain and active low, and the board gives it no pull-up; the ring line idles low too. A light sleep wakes on a level, and a line that is armed for low and is low ends it at once.
Options: Arm the two lines for high instead (right only if their idle level is low by design). Pull the lines up with the processor's own pull-ups so that an open-drain line idles high, and arm each line with the level it does not have at that moment, whatever its polarity. Stop using the two lines for waking (a call would wait for the timer, up to two seconds).
Decision: The second, and a sleep that nothing ended counts as no sleep in the loop line, and the start logs the idle level of the three lines.
Reason: It needs no knowledge of the lines' polarity, costs nothing when a line is driven, and the start line and the next night show whether it holds.
Consequences: `board::powerUp` (`test_power_up`), `device/Sleep.h`, `device/main.cpp`. The checks: the start line "Wake lines: keyboard high, ...", and after a night most sleeps ended by the timer with "asleep" most of the half hour and a current well under 105 mA. If a line still idles low with the pull-up, it is driven low, and the arming for high is what wakes on it.
Verified: host-tested; compiled; verified on the device on 2026-10-16: the start line says "Wake lines: keyboard high, touch high, ring high"; in the night of 2026-10-15 every window of the loop line has 882 to 883 passes in 1800 s, asleep 97 to 98 %, all sleeps ended by the timer, no line low before a sleep, and the energy log reads 58 to 63 mA (average 61.9 mA, 105 before). A wake by a line (a key, a touch, the ring) was seen on 2026-10-23: eleven sleeps ended by a line in eight half-hour windows, the ring line low before one sleep

## D-35: The modem stays awake for its IMS, at most two minutes after the registration
Date: 2026-10-16   Status: decided
Decided by: owner (the standing go of 2026-10-14 to build what the nights show); agent (what)

Context: The log of 2026-10-16 (firmware 4.2.5, finding F3) shows the modem going to sleep 14 s after the network registration, before its IMS registered; after the wake the IMS was there at the next periodic question, 30 s later. Asleep, the driver's periodic question does not run, so the voice status stayed "IMS missing" for 40 minutes, until something woke the modem: the lock screen and the status bar said "connecting" and a call was refused as "not ready". Rule 7 of the power policy let a registered modem sleep without looking at the IMS.
Options: Keep the modem awake in the driver while the IMS is missing (the coordination would believe it asleep and let the processor sleep beside an awake modem, whose answers are lost in light sleep). A new rule of the power policy: a registered modem sleeps once the IMS is registered or the network carries SMS only, or after a bound since the registration. The same without a bound (a network without VoLTE would keep the modem awake for good).
Decision: The second, as rule 9 with a bound of 120 s (`PowerPolicy::kImsWaitSeconds`), counted by `PowerCoordination` from the pass in which it sees the modem registered and started again at every new registration. A voice status that is not known (the IMS question unanswered) waits like a missing IMS. After the bound the modem sleeps as before; a wake asks for the IMS at once, as the driver already did.
Reason: The policy decides the processor's sleep from the modem's, so the wait belongs where both are decided. 120 s are four of the driver's 30-second questions and four times what the log shows, and a network without VoLTE costs two minutes awake per registration, not more.
Consequences: `PowerPolicy` (rule 9, inputs `voiceSettled` and `registeredSeconds`) and `PowerCoordination` ([power-policy.md](core/power-policy.md), [modem-driver.md](modem/modem-driver.md)); tests in `test_power_policy`, `test_power_coordination` (POW-17, POW-18) and `test_modem_driver` (the driver on the scripted modem under the coordination). After a start, a lost network or a restart of the modem, the modem and the processor stay awake until the IMS registers, up to two minutes; on a network without VoLTE the screens say "connecting" after those two minutes until the next wake asks again. The check on the device is open point 37.
Verified: host-tested; compiled; not verified on the device

## D-36: The home network chosen by hand is not searched again, and the automatic phase lasts a minute
Date: 2026-10-16   Status: decided
Decided by: owner (the standing go of 2026-10-14 to build what the nights show); agent (what, and the numbers: 30 rounds, AT+COPS=0 without deregistering)

Context: Finding F4 of the log analysis of 2026-10-16 (firmware 4.2.5). At a start after 5 h 25 min switched off, the cold modem had not registered by itself after the 8 rounds of AT+CREG? (about 16 s); the driver chose the first network of AT+COPS=? by hand, which was the SIM's own (26201), and treated it as a foreign one. A quarter of an hour later, at the child's first key press, the search of 4.1.7 deregistered it (AT+COPS=2, AT+COPS=0): about 33 s without calls, and the IMS had to register again.
Options: Leave it. Give the automatic phase more time only (the choice by hand at home becomes rarer, but its search stays). Also treat a network chosen by hand with the SIM's operator code as the home network: no search, and either leave the modem in manual mode (it would not register abroad after a border crossing, and nothing would set it back) or set it back to automatic with AT+COPS=0 alone, without deregistering.
Decision: The automatic phase has 30 rounds of AT+CREG? (about 60 s) before the choice by hand. A network chosen by hand whose code is the SIM's operator code (the MCC+MNC that the operator list found for the APN, D-9) is the home network: one log line ("Cellular: the network chosen by hand is the home network, no new search"), no search of a quarter of an hour, and AT+COPS=0 alone at the first idle moment in Ready. If the modem refuses AT+COPS=0, the search of 4.1.7 takes over, as for any network chosen by hand. A foreign network chosen by hand, and roaming, are searched again every 15 minutes as before.
Reason: A cold modem after hours off needs more than 16 s; a minute covers it and still leaves 30 s of the 90 s registration window, which is extended for the choice by hand anyway. A foreign SIM that finds nothing automatically pays about 44 s more, once per start. A registered modem answers AT+COPS=0 at once and keeps its network, so the modem is in automatic mode again without a gap, and can follow the child across a border.
Consequences: src/drivers/A7682EDriver.{h,cpp} (Config::autoRegisterAttempts 30, Job::Automatic, homeOperator_), docs/modem/modem-driver.md, docs/modem/apn-table.md. The home network is known only when no APN is configured and the list knows the operator; otherwise every network chosen by hand is searched again as before. Checked by test_modem_driver: test_the_home_network_chosen_by_hand_is_not_searched_again (fails with the old behaviour: the home network is deregistered), test_a_foreign_network_chosen_by_hand_is_searched_again_although_the_home_network_is_known, test_a_refused_automatic_mode_leaves_the_search_of_a_quarter_hour, test_the_automatic_phase_lasts_thirty_rounds_before_the_choice_by_hand. On the device: open point 19.
Verified: host-tested; compiled; not verified on the device; seen on the device on 2026-10-22 and 2026-10-24: starts after 1 h 41 min and 2 h 56 min off registered automatically within 29 s, no choice by hand; the search abroad is still unseen

## D-37: After a read of the message log that stopped early, nothing is written to it until a whole read
Date: 2026-10-16   Status: decided
Decided by: owner (the standing go of 2026-10-16: the agent decides what the nights show); agent (the rule)

Context: Twice in the week of 2026-10-13 a start read messages.log only up to a block that could not be read (F6 of the log analysis of 2026-10-16, open point 36): the store kept what it read without the read mark (35 unread on the lock screen) and took the next number from the lines read so far, which the unread rest of the file already holds. A message written with such a number is a repeat at the next whole read and is dropped, and a deletion line for it deletes the old message.
Options: Raise the next number above a high-water mark (the store cannot know one without reading the rest). Take the next number from the file's size or its last line (the size is no safe bound, because numbers whose line was never written still count; the last line needs a read at the end that can fail like the first). Read past the failed block (changes the log's contract, hands over a piece of a line). Write nothing to the log until a restore reads it to the end (the messages of that session are not in the history after the next start).
Decision: The last: after a read that stopped early the store applies the read mark and the deletions it read, works in memory as usual, and refuses every write to the log (counted in writeErrors()) until the next restore reads it whole. The card's log reads a failed block once more before it stops, and the log says whether a read was complete, found no history, or stopped early, so that a first block that fails is not taken for an empty history.
Reason: It is the one rule of the four that can never reuse a number that is in the file, and it needs no new read; the retry makes the case rare.
Consequences: The start logs "Messages: restored N (U unread), D damaged lines" or "Messages: reading the log failed after L lines: ...", kept on every log level. Messages and marks of a session after a read that stopped early are lost from the history at the next start. A card that is missing at start and pushed in later still numbers from 1 (not part of this decision). Tests: test_message_store (test_a_read_that_stops_early_still_applies_the_read_mark_and_the_deletions_it_saw, test_after_a_read_that_stopped_early_nothing_is_written_so_no_number_is_reused), test_sd_logs (the retry), test_app_core (the start line); see message-store.md.
Verified: host-tested; compiled; not verified on the device

## D-38: The input current limit is 800 mA
Date: 2026-10-16   Status: decided
Decided by: owner (800 mA, 2026-10-16); agent (the register value and the read-back)

Context: The SY6970 takes every supply for an "unknown adapter" and limits its input current to 500 mA (register 0x00 = 0x48). On 2026-10-15 a mains adapter charged the 1500 mAh cell at 516 falling to 420 mA for 3 h 10 min; the charge current limit of 960 mA (D-14, 0.65 C) was never reached (open point 26).
Options: Leave the chip's 500 mA (slow, the cell's limit never holds). Set 800 mA at every start. Set 1 A or more (a computer's USB port, which gives 500 mA to 900 mA, would be asked for more than it gives). Detect the supply and choose per kind (the SY6970's data sheet is not in the repository).
Decision: The input current limit is set to 800 mA at every start, like the charge current limit of D-14: IINLIM (register 0x00 bits 5:0) = (800 - 100) / 50 = 14, the other bits kept, written only on a difference and read back. The value is `kInputLimitMa` in `Charger.h`, the only place.
Reason: A mains adapter gives at least 1 A, so 800 mA leaves a margin, and the cell's 960 mA charge limit stays the one that holds. A computer's USB port limits itself, or the charger's input voltage regulation lowers the current; the owner accepts that.
Consequences: `Charger::limitInput` and the start line of `BoardPower::begin` ("Battery: input current limit set to 800 mA (it was 500 mA)", "... is 800 mA already, nothing to write", "... could not be set"); tests in `test_charger` and `test_board_power`; [device-power.md](hardware/device-power.md). Not known: whether the SY6970 keeps the value when it detects a supply after the start (the BQ25896 sets the limit from its detection); open point 26 watches the next charge at the adapter.
Verified: host-tested; compiled; verified on the device on 2026-10-21: the energy log of the charge of 2026-10-20 (the device switched on with the adapter attached) reads 812 falling to 759 mA, 13 to 95 % in 70 minutes; a cable attached on 2026-10-21 while the device ran left register 0x00 at 0x48 (500 mA, "limit register in force"): the chip sets the limit from its input detection at every new supply, after the start's write, which is the question this entry left open (hardware-lessons.md)

## D-39: A call during the SOS countdown does not cancel it
Date: 2026-10-17   Status: decided
Decided by: owner (2026-10-17: not cancelled, as long as the call can be answered and the SOS page with what was sent comes back after the call); agent (how)

Context: The review of 2026-10-17 found in the simulator that a call ringing during the 40-second choice of the SOS page cancelled the emergency: the call page covered the SOS page, the page's `onLeave()` called `leaveSos()`, and `SosFlow::leave()` sends nothing, now or later. After the call the page said "No emergency is running". No decision covered it; docs/ui/screen-sos.md named only Back, home and the lock as ways out.
Options: Leave it (a call from anybody in the contact book cancels an automatic emergency). A call does not cancel the choice: the call page covers the SOS page, the countdown goes on, and the stack returns to the SOS page after the call. Hold the call until the choice is made (the parent who calls back would not get through).
Decision: The second. The SOS page does not close its choice when it is left for a call that rings (`SosScreen::onLeave`); the child can answer, the countdown goes on, and when nobody chose the emergency goes out by itself during the call. When the call is over `ScreenStack::endInterrupt` brings back the SOS page, which shows the countdown or what the ping did.
Reason: An emergency that runs out its countdown was meant to go out; a call that happens to come in is no decision by the child to cancel it, and the child must still be able to talk to whoever calls.
Consequences: `src/ui/screens/SosScreen.h`; [screen-sos.md](ui/screen-sos.md), [sos-flow.md](core/sos-flow.md). Tests: `test_runtime` (`test_a_call_during_the_sos_countdown_does_not_cancel_it_and_the_page_comes_back_after_it`) and the scenario `sos-during-a-call`, both failing with the old `onLeave()`. Whether the modem sends the text during a voice call is not known on the device; if it refuses, the text waits in the queue and goes when the call is over. The check on the device is open point 38.
Verified: host-tested; compiled; seen in part on the device on 2026-10-21 (4.2.9, the owner): during ringing and in the call the three touch keys do nothing and the call can be taken and ended; a locked device is locked again after a refused call. Not seen yet: the lock after an answered call, and a call during the SOS countdown (open point 38)

## D-40: A start without a network searches on instead of giving up
Date: 2026-10-17   Status: decided
Decided by: owner (2026-10-17: yes, search on as for a lost network); agent (how)

Context: The review of 2026-10-17 found, and reproduced on the scripted modem, that the modem driver went to `Error` when the window of the first registration (90 s, extended for the choice by hand, about 4.5 minutes in all) ran out without a network, and that nothing ever left that state: `pickNextJob()` does nothing in `Error`, a ring is ignored and `sendSms()` refuses. A device switched on in a basement, a tunnel or the underground stayed without calls, texts and SOS until a restart, even when the network came back. A network lost after the start already searches on without a window ("a device in the woods is not in error").
Options: Leave it. Search on after the window as for a lost network: no window, the registration asked every two seconds. Search on with a slower interval after a while (less traffic on the serial line, the same current, because a modem that is not registered stays awake by rule 7 of the power policy either way).
Decision: The second. At the end of the window the driver logs "Cellular: no network found, searching on" and goes on in `Registering` without a window (`A7682EDriver::searchOn`). `Error` is left for what the SIM refuses and a modem that does not answer.
Reason: An emergency phone that has been in a cellar must work when the child comes out; the lost network already behaves this way, and the start should not be stricter than the woods.
Consequences: `src/drivers/A7682EDriver.{h,cpp}`; [modem-driver.md](modem/modem-driver.md), [hardware-capabilities.md](hardware/hardware-capabilities.md). Test: `test_modem_driver` (`test_a_start_without_a_network_searches_on_after_the_window_and_takes_the_network_when_it_comes`, in the automatic phase and after the choice by hand; it fails with the old `Error`). The current is the one of the old `Error` state, in which the modem was not registered and so awake as well. The check on the device is open point 39.
Verified: host-tested; not verified on the device

## D-41: The web server's socket never waits, and the modem's receive queue is 4096 bytes
Date: 2026-10-17   Status: decided
Decided by: owner (2026-10-17: build step 3 of the review and prepare it for the USB log run of 2026-10-18); agent (how)

Context: The review of 2026-10-17 found two ways in which the main loop lost what it must not. (1) `Esp32TcpSocket::write` handed the rest of a response to `WiFiClient::write`, which waits up to a second per try for room and renews its ten tries with every bit of progress: a client on the access point or the home network that reads the 57 KB page slowly (no password needed for `GET /`), or a parent's phone on a weak link, held the main loop, and with it the SOS, the calls and the keys, for as long as it kept reading. (2) The modem's UART kept the core's receive queue of 256 bytes, 22 ms at 115200 baud, while a pass that refreshes the e-ink panel takes hundreds; a text read from the SIM could lose bytes, and a UCS2 text whose line broke was delivered empty or cut and then deleted from the SIM.
Options: (1) Leave the blocking write. A write that never waits (`send` with `MSG_DONTWAIT`) and a server that offers several chunks per pass. A task of its own for the web server (more to get wrong on a device with one loop). (2) Leave the queue. A larger queue (4096 bytes, the size the USB console got in 4.2.2) and a count of overflows in the log, so the device says whether it is enough. Hardware flow control (the pin map has no RTS or CTS line of the modem, only RXD, TXD, the ring line and DTR).
Decision: (1) The socket's write never waits; `WebServer::poll` offers up to eight chunks per pass and the rest in the next pass. (2) The modem's UART gets a receive queue of 4096 bytes; overflows of the queue or the chip's own buffer are counted (`Esp32UartPort::takeOverflows`) and logged at most once a minute ("Modem UART: receiving failed N times, ..."); a text with a line inside it that is neither hex nor a URC is marked damaged and left on the SIM, to be read again with the next read of the whole SIM.
Reason: An emergency phone must not be stopped by a slow browser, and a text from a parent must not vanish because its bytes came during a refresh of the panel. Both changes are small, and the overflow line lets the device measure what the host cannot.
Consequences: `device/Esp32TcpListener.h`, `device/Esp32UartPort.h`, `device/main.cpp`, `src/web/WebServer.cpp`, `src/drivers/AtParser.{h,cpp}`, `src/drivers/A7682EDriver.cpp`; [web-interface.md](web/web-interface.md), [modem-driver.md](modem/modem-driver.md), [device-gnss-modem-clock.md](hardware/device-gnss-modem-clock.md). Tests: `test_web_server` (`test_a_pass_sends_up_to_eight_chunks_and_leaves_the_rest_for_the_next`), `test_at_parser` (`test_cmgl_ucs2_text_that_lost_characters_on_the_line_is_damaged_and_a_whole_one_is_not`), `test_modem_sms` (`test_a_text_broken_on_the_line_stays_on_the_sim_and_arrives_when_it_is_read_whole`), each failing with its part put back. The device adapters are compiled only: whether `send(MSG_DONTWAIT)` behaves on the device as described, and whether 4096 bytes are enough, is the USB log run of open point 40.
Verified: host-tested; compiled; seen on the device on 2026-10-21 (4.2.9, the station on the home network): the start lines have no "Modem UART: receiving failed" and no "reading a text failed"; `curl --limit-rate 300` fetched the whole page (58,797 bytes in 195.7 s) while the keys answered at once and a call rang and was taken (the owner); afterwards the page loaded in 0.23 s; the loop line of that half hour (15:24 to 15:54) has a longest pass of 1404 ms, the panel's full refresh, the same as the half hour before (1403 ms), so no write waited. Not seen yet: three long texts with umlauts sent while the device was off (open point 40)

## D-42: The gauge's end-of-discharge thresholds are set for this cell, or its learned full charge is corrected
Date: 2026-10-21   Status: decided
Decided by: owner (2026-10-21: work out what the manual gives and build it in; nothing more to decide by the owner); agent (the thresholds and the sequence, from the reference manual)

Context: Four discharges (2026-10-13 to 2026-10-21) fell from about 45 % to 7 % within ten minutes at 3623 to 3637 mV and said 0 % from about 3.60 V, while the cell gave five to eight more hours to the switch-off at 3291 mV (D-5). D-24 shows the voltage's number on the screen for those hours. Each discharge that passed 3.63 V taught the gauge a smaller full charge, 1372, 1107 and 988 mAh, because the gauge learns at its own end of discharge, and its thresholds are not this cell's (hardware-lessons.md, "Two charges and a day on the table"). The percentage above the fall is counted against a full charge that shrinks, and the fall comes at a higher percentage each time.
Options: (a) Leave it: the screen is right below the fall (D-24) and roughly right above it. (b) At the start, write the design capacity over a learned full charge that fell below a bound, as D-32 did before 4.2.4 at every start: the percentage stays relative to 1500 mAh, the fall to 7 % stays where it is. (c) Set the gauge's end-of-discharge voltages to this cell, down to the switch-off, once and through the chip's configuration update mode, as the design capacity is set once (D-32): the gauge would count to the real end and learn a true full charge. (d) Leave the gauge's percentage out and compute it in the firmware from the voltage and the gauge's count.
Decision: (c), prepared from the BQ27220's reference manual (the data memory's subclass and offsets, the update sequence and its checksum), written once like the design capacity and read back; (b) as the fallback when the manual does not give usable thresholds. The owner's go of 2026-10-21 covers both; the numbers and the sequence are the agent's and go into the Consequences line of the change's own entry.
Reason: (c) is the only option that makes the gauge's own numbers right; it needs the manual, which the repository does not have, as with the charger's settings after "done" (open point 26).
Consequences: Open point 3 says what the device shows until the change is built; the change comes with its own pull request, tests against a scripted gauge, and a check on the device (a discharge past 3.63 V that no longer falls to 7 %, and a learned full charge that does not shrink).
Verified: the change is built as D-45 (4.2.9); its Verified line says what the device showed

## D-43: The input current limit is written again while a cable feeds the board
Date: 2026-10-21   Status: decided
Decided by: owner (800 mA, D-38; the standing go of 2026-10-16 to decide the charger's behaviour from what the device shows); agent (the rule)

Context: D-38 left open whether the SY6970 keeps the limit the start wrote when it detects a supply later. The logs of 2026-10-21 answer it: switched on with the adapter attached, the device charged at 812 falling to 759 mA (the start wrote 800 mA after the chip's detection); a cable attached while the device ran left register 0x00 at 0x48, 500 mA "limit register in force", "unknown adapter". The chip's input detection runs at every new supply and writes the register, over what the firmware wrote, so a plug-in while the device runs charged at 500 mA as before D-38.
Options: Leave it (the adapter charges at 800 mA only after a start with the cable attached). Write the limit once when the power-good bit appears (the detection may end after that reading and set 500 mA back; the reading comes at most once a second). Write it again at every reading while a cable feeds the board, which reads one register a second and writes only on a difference. Read the chip's detection result and write only when it says "unknown adapter" (the data sheet is not in the repository, and the other kinds were never seen).
Decision: The third: `BoardPower::read` calls `Charger::limitInput(kInputLimitMa)` whenever the charger says power good, which reads register 0x00 and writes only on a difference; a write is logged ("Battery: input current limit set to 800 mA again (it was 500 mA: the charger set its own when it detected the supply)"), a refused write once per cable.
Reason: It holds whenever the chip writes the register, however late its detection ends, and it costs one register read a second only while a cable is attached. A plug-in that the chip answers twice shows as two log lines, which is what the log is for.
Consequences: `BoardPower::keepInputLimit` ([device-power.md](hardware/device-power.md)); tests in `test_board_power` (`test_the_input_limit_is_written_again_when_the_charger_set_its_own_at_a_plug_in`, which fails without the change, `test_on_the_battery_the_input_limit_register_is_left_alone`, `test_a_refused_write_at_the_cable_is_said_once_per_cable`). On the device: open point 26 (the log line at a plug-in while running, `battery` reading 800 mA, about 800 mA in the energy log at the adapter).
Verified: host-tested; compiled; verified on the device on 2026-10-21 (4.2.8): the cable pulled and attached again while the device ran gave one line "Battery: input current limit set to 800 mA again (it was 500 mA: the charger set its own when it detected the supply)", and `battery` read "input current limit 800 mA (limit register in force)" at 837 mA from the computer's port

## D-44: A whole log is read in pieces from an offset, over the cable and over the page
Date: 2026-10-21   Status: decided
Decided by: owner (2026-10-21: read whole logs without taking the card out); agent (an offset argument on both reads rather than a larger buffer or an HTTP Range header)

Context: The system log on the card grows to hundreds of KB (219 KB after a week). Over the USB cable `CONFIG READ system.log` gives only the last 8 KB (the console's answer, `kConsoleMaxFile`), and the parents' page's log tab (`GET /api/file?path=/system.log`) only the last 12 KB (the one response buffer, `kResponseMax`), so the start lines of earlier boots could only be read by taking the card out of the device.
Options: A larger buffer (RAM on the device, and still a limit a log outgrows). An HTTP `Range` header on the file route (more parsing on the device, and nothing for the console). An offset argument on both reads, `CONFIG READ <file> <from>` and `GET /api/file?...&from=<offset>`, answered with the offset and the total (the smallest change; the tools read the pieces).
Decision: The offset argument. `ConfigStore::readAt` reads from an offset in every store; the console answers up to 8 KB from `<from>` with `from <from> of <total>`, the web route up to 12287 bytes with `X-File-From`, `X-File-Size` and `X-Truncated`; at or past the end both answer empty with the right total. Without the argument both behave as before (the end of a long log). `deviceconfig.py read <file> --all` and the log tab's "Load the whole log" read the pieces from 0 to the total and join the bytes before decoding them.
Reason: It reads a log of any size with the buffers the device already has, adds one number to two commands, and leaves every existing answer as it was.
Consequences: `src/hal/ConfigStore.h`, `src/board/SdConfigStore.{h,cpp}`, `sim/DirectoryStore.h`, `testing/fakes/FakeConfigStore.h`, `src/board/Console.{h,cpp}`, `src/web/WebApi.{h,cpp}`, `scripts/deviceconfig.py`, `scripts/gen_web.py` and `web/index.html`, `lang/web/*.toml` (`web.log.whole`); [device-console.md](hardware/device-console.md), [web-interface.md](web/web-interface.md), the three user guides. Tests: `ConfigStoreContract::a_file_read_in_pieces_from_offsets_is_the_whole_file` (fake, SD store, simulator store), `test_sd_config_store` (`test_a_piece_of_a_large_log_is_read_from_its_offset_in_blocks_and_a_failing_card_gives_none`), `test_console` (`test_read_with_an_offset_gives_the_pieces_of_the_whole_log`, `test_read_at_or_past_the_end_is_empty_and_still_says_the_total`, `test_the_offset_of_a_read_is_digits_only`), `test_web_api` (`test_a_log_is_read_in_pieces_from_an_offset_with_its_size_in_the_headers`, `test_a_small_file_read_from_0_is_whole_and_not_marked_truncated`, `test_an_offset_that_is_not_digits_is_refused`), `test_device_tools` (`read --all`), `test_web_page` (r). On the device: open point 42.
Verified: host-tested; compiled; seen on the device on 2026-10-21 (4.2.9): `deviceconfig.py read system.log --all` over the cable printed the whole log (2387 bytes) from its first line; the page's "Load the whole log" is not seen yet (open point 42)

## D-45: The gauge's end of discharge is 3300/3420/3470 mV with the compensation off
Date: 2026-10-21   Status: superseded by D-47
Decided by: owner (the go of D-42); agent (the numbers, the addresses and the sequence, from the BQ27220 reference manual SLUUBD4A)

Context: D-42 decided to set the gauge's end-of-discharge thresholds for this cell from the chip's reference manual. The manual (extracted on 2026-10-21, checked by a second agent against the PDF) says: the gauge says 0 %, 3 % and "Battery Low %" (7 %) at EDV0, EDV1 and EDV2 and learns its full charge at EDV2; with the Gauging Configuration's bit EDV_CMP (0x929B, bit 3) cleared the three are the fixed values Fixed EDV0/1/2 at 0x92B4, 0x92B7 and 0x92BA in mV (defaults 3031, 3385, 3501); the chip ships with the compensation on and a profile of another cell, which put EDV2 at about 3630 mV with this cell at 60 to 105 mA. The data memory is written by absolute address through 0x3E/0x3F, the data big-endian at 0x40, the checksum (the complement of the sum of the address and data bytes) at 0x60 and the length (the data bytes plus 4) at 0x61, inside the chip's configuration update mode (Control 0x0090, the CFGUPDATE bit of OperationStatus, exit 0x0091 with a restart or 0x0092 without, the unseal keys 0x0414/0x3672 and full access 0xFFFF/0xFFFF, seal 0x0030); the values live in RAM. SensorLib 0.5.0, which writes the design capacity (D-32), has no general data-memory write.
Options: Keep the compensation and move the fixed values only (the compensation formula is not in the manual, so the result cannot be computed). Clear the compensation and set fixed values placed on this cell's discharge curve. The fallback of D-42 (write the design capacity over a shrunken learned full charge).
Decision: The compensation is cleared (every other bit of the configuration kept as read) and the fixed thresholds are EDV0 3300 mV (the firmware's switch-off, D-5), EDV1 3420 mV and EDV2 3470 mV, where 3 % and 7 % of about 1300 mAh usable remain on the curve of 2026-10-21 at 61 mA (0.6 and 1.5 hours before the switch-off), each at the lower end of its range because a threshold reached early is the measured failure and the modem awake lowers the voltage by about 60 mV. `board::ensureEndOfDischarge` reads the four items at every start, writes only those that differ through `GaugeCapacityWriter::writeDataMemory`, reads them back and logs one line; `device/Esp32GaugeWriter.h` repeats the manual's sequence on SensorLib's public raw register calls, leaves the update mode again when the entry fails, and seals the chip again if it was sealed.
Reason: It is the only option the manual supports that makes the gauge's own numbers right for this cell, and the write-on-difference at every start restores the RAM values if the chip ever loses them. What the manual leaves open (which exit applies new values, the whole-block wording against the partial writes it also shows, the active profile) is answered by the capacity write that has worked on the device the same way, and by the device check.
Consequences: `src/board/FuelGauge.{h,cpp}` (`kEdv0Mv`..`kEdv2Mv`, `dataMemoryBlock`, `ensureEndOfDischarge`, `describeEndOfDischargeCheck`), `src/board/BoardPower.cpp` (the start line "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3420/3470 mV, compensation off (they were ...)", or "... already, nothing to write", "... could not be set ...", "... could not be read, nothing written"), `device/Esp32GaugeWriter.h`, `testing/fakes/FakeGaugeWriter.h`; [fuel-gauge-data-memory.md](hardware/fuel-gauge-data-memory.md), [device-power.md](hardware/device-power.md). Tests: `test_fuel_gauge` (`test_the_thresholds_are_set_with_the_compensation_off_and_the_other_bits_kept`, `test_only_the_items_that_differ_are_written`, `test_a_data_memory_block_has_the_manuals_bytes_checksum_and_length` and others), `test_board_power` (`test_start_sets_the_end_of_discharge_thresholds_once_and_logs_one_line_about_it`). On the device: open point 3 (the start line, `battery`, a discharge past 3.63 V that no longer falls to 7 %, a learned full charge that no longer shrinks).
Verified: host-tested; compiled; seen on the device on 2026-10-21 (4.2.9): the first start logged "the fuel gauge's end-of-discharge thresholds set to 3300/3420/3470 mV, compensation off (they were 3031/3385/3501 mV, compensation on)", the second "are 3300/3420/3470 mV, compensation off already, nothing to write"; the write with the restart (0x0091) set the gauge's cycle count from 5 to 0 and left the learned full charge at 988 mAh; whether a discharge past 3.63 V no longer falls to 7 % is not seen yet (open point 3); verified on the device on 2026-10-22 and 2026-10-24: no fall at 3.63 V, the learned full charge grew (988, 1244, 1310 mAh), the gauge held 7 % for 5 h 10 min and 55 min before EDV2 and said 0 % at 3432 and 3438 mV, about 1.5 h before the switch-off ([hardware-lessons.md](hardware/hardware-lessons.md))

## D-46: The documentation is grouped in folders, and a moved file's link path may change in an old entry
Date: 2026-10-21   Status: decided
Decided by: owner (2026-10-21: reorganise the documentation); agent (the folders)

Context: docs/ held 91 Markdown files in one flat list: the decision log and the list of open work were buried in it, the user guides stood between driver notes, and on GitHub in the browser nothing could be browsed by subject. Moving a file breaks the relative links to it, and this log and hardware-lessons.md link to many documents, while rule 7 of CLAUDE.md allowed only the status and "Verified" lines of an old entry to change.
Options: Leave the flat list and only rewrite the index (nothing to browse by folder). Group the documents in a few folders by what they describe and update every link and path that names a moved file, in old entries too. Group them and leave old entries with broken links (the log would point nowhere).
Decision: Group them in six folders, keep the names of the files, and update every link and path that names a moved file, including those in old entries of this log; nothing else of an old entry changes. Rule 7 of CLAUDE.md says so in one sentence. Documents that a script or a test reads by its path, or that a comment in the generated parents' page names, stay at the top of docs/.
Reason: A link that points nowhere is worse than a changed path; the path is a mechanical consequence of the move and changes nothing that was decided.
Consequences: docs/README.md is the index: "Start here" (decisions.md, open-points.md, the changelog, the release notes), "Manuals" (the user guides in three languages, the acceptance test, upgrading.md), then one table per folder. The layout: docs/ keeps README.md, decisions.md, open-points.md, user-guide.md, de/, fr/, release-4.0.md to release-4.2.md (scripts/check_docs.py and its test read `docs/release-<major>.<minor>.md`), upgrading.md and quiet-hours.md (named in a comment of the page that the firmware carries), app-behaviour.md (scripts/check_behaviour.py), configuration-reference.md (scripts/gen_config.py), simulator.md (test_simulator) and translating.md (test_translating); docs/hardware/ the board, its chips, the measurements and the bring-up; docs/modem/ the modem driver, its AT layer, VoLTE, the APN list and the SMS PDU; docs/core/ the application core and its logic modules; docs/ui/ the interface framework and one document per screen (scripts/check_docs.py looks for `docs/ui/screen-<name>.md`); docs/web/ the parents' page and the Wi-Fi; docs/process/ the test doubles, the simulated panel and the demo. Checked by scripts/check_docs.py (every link resolves, every document is in the index).
Verified: not applicable

## D-47: The gauge's EDV1 and EDV2 are lowered to 3390/3440 mV (supersedes the numbers of D-45)
Date: 2026-10-24   Status: decided
Decided by: owner (the standing go for the battery numbers: the agent's numbers change with a new entry when the device shows a reason); agent (the numbers)

Context: Firmware 4.2.9 set the gauge's thresholds to 3300/3420/3470 mV with the compensation off (D-45) at its first start on 2026-10-21. Two discharges followed: 2026-10-21 20:54 (cable off) to 2026-10-22 19:04, the switch-off at 3295 mV (22 h 10 min on the desk, 62 mA by the slope), and 2026-10-23 07:31 to 2026-10-24 05:04 (21 h 32 min of use by the child, 64 mA by the slope, the modem awake 0.8 % of the time). The old fault did not return: at 3.63 V the percentage fell one point per ten minutes and never from 45 to 7 % within ten minutes, and the learned full charge grew (988, 1244, 1310 mAh) instead of shrinking. Two things remained wrong. The gauge held 7 % with a frozen count for 5 h 10 min on the first discharge (3595 to 3477 mV, about 320 mAh drawn meanwhile) and for about 55 min on the second (3523 to 3497 mV): the count reached 7 % of a full charge that was still too small (988, then 1244 mAh) long before EDV2, and the gauge, as far as the logs show, holds "Battery Low %" until EDV2 fires (the count resumed at 3470 mV on the first discharge and at 3490 mV on the second); the hold shrinks as the full charge converges. And after EDV2 it counted the remaining 7 % down and said 0 % at 3432 mV (17:38) and 3438 mV (03:28), 1 h 26 min and 1 h 36 min before the switch-off: at 63 mA the cell still held about 95 mAh below 3470 mV, so EDV2 sat about 40 mV too high for this cell, and EDV1 with it ([hardware-lessons.md](hardware/hardware-lessons.md)).
Options: Keep 3420/3470 mV (the gauge says 0 % an hour and a half early and the screen shows 0 % for that time). Lower EDV2 to the measured 1.5-hour point and EDV1 to the 38-minute point, from the two energy logs. Set EDV2 even lower (the cell's knee below 3400 mV is steep, a threshold there is reached late, and the count would run past 0 % with the voltage already near the switch-off).
Decision: The second. EDV2 3440 mV: the energy log read 3432 and 3438 mV 1 h 26 min and 1 h 36 min before the switch-off, and 7 % of about 1350 mAh is 95 mAh, 1.5 h at 63 mA. EDV1 3390 mV: 3 % of 1350 mAh is 40 mAh, 38 minutes at 63 mA; the rows 36 minutes before the switch-off read 3396 mV (2026-10-22 18:28:17) and 3398 mV (2026-10-24 04:28:18), and the lower, rounded down to 10 mV, is 3390 mV. EDV0 stays 3300 mV (D-5). The sequence, the compensation bit and everything else of D-45 are unchanged.
Reason: The two upper thresholds belong where the cell really has 7 % and 3 % left at the device's load; the observed points are the best measurement there is, and rounding down errs towards a late threshold, which costs a few minutes of a 0 % that is a little late, not the shrinking full charge that an early threshold teaches. With the modem awake the voltage is about 60 mV lower, so the gauge still says 7 % and 3 % a little early then.
Consequences: `kEdv1Mv` and `kEdv2Mv` in `src/board/FuelGauge.h` with their comment; [device-power.md](hardware/device-power.md), [fuel-gauge-data-memory.md](hardware/fuel-gauge-data-memory.md). The first start of the next release finds 3300/3420/3470 mV on the chip and writes the two upper ones: the start line reads "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3390/3440 mV, compensation off (they were 3300/3420/3470 mV, compensation off)", the start after it "... are 3300/3390/3440 mV, compensation off already, nothing to write". Tests: `test_fuel_gauge` (`test_the_numbers_of_d45_on_the_chip_are_replaced_by_3390_and_3440_with_one_line`, which fails with the old constants, and the tests that pin the numbers), `test_board_power` (`test_a_start_after_4_2_9_replaces_the_numbers_of_d45_and_says_so`). A hold at 7 % lasts until EDV2 fires, so it lasts about 45 minutes longer than it would at 3470 mV (the time the cell took from 3470 to 3440 mV on both discharges) for as long as the learned full charge is below what the cell delivers; it ends by itself once the full charge has converged (988, 1244, 1310 mAh so far; the cell delivered 1330 to 1390 mAh to the switch-off). Open point 3 keeps the next discharge as the check: the gauge's 0 % within about half an hour of the switch-off, and no hold at 7 % once the full charge has converged.
Verified: host-tested; compiled; not verified on the device

## D-48: The project has a web page on GitHub Pages, with the click-through demo as its first screen
Date: 2026-10-25   Status: decided (the language's place in the address: superseded by D-51)
Decided by: owner (2026-10-25: a page that presents the project, with the simulator on its first screen); agent (confirmed by owner on 2026-10-25)

Context: The owner asked for a GitHub page that presents the project with the running simulator on its first screen. The simulator is a C++ program on the laptop (threads, sockets for the parents' page, a card on disk); a browser cannot run it as it is. What exists for a browser is the click-through demo ([demo.md](process/demo.md)): the simulator takes a picture of every state the keys reach, and a static page walks through them along the graph of the walk, in three languages.
Options: Compile the simulator to WebAssembly and run it in the page (the real behaviour in the browser, but a toolchain the project does not have, a stub for the sockets and the card, a port of the loop without threads: a project of its own, and the timing would still not be the device's). Embed the click-through demo as the page's first screen (what the keys reach, drawn by the simulator on every build; no scenes, no time, no incoming call unless a picture of it is linked). A page with pictures made by hand (against the rule that nothing of the demo is drawn by hand). Where it is published: a `gh-pages` branch with generated files committed (against "nothing generated is committed"), the `docs/` folder (which is the documentation and is checked as such), or GitHub Pages built by a workflow from `site/` and a fresh run of the demo.
Decision: The click-through demo is the page's first screen, in a frame that shows the device's keys, with the full demo page published beside it under `demo/`; the page says that its pictures were drawn by the simulator and that it is not the simulator running. The page exists in English, German and French: its words are the `site.*` keys of the host-only catalogs `tools/lang/site.<code>.toml`, as the demo page's are (no words in the markup, a test says so), and one selector switches the words and the device's pictures together. The language is the address's (`?lang=de`), English by default whatever the browser says, because the page is English and a link must open what its sender saw; the owner reported that the earlier page, which followed the browser, could not be kept in English. The page presents Phone OS as a phone operating system that is 100 % open source; the emergency phone for a child is the first device built with it (owner, 2026-10-25). Published by a workflow (`.github/workflows/pages.yml`) that builds the simulator, runs `scripts/make_demo.py` and `scripts/make_site.py` and deploys `out/site/` on every push to `main`; nothing generated is committed. The WebAssembly simulator is not built; it stays an option for the owner to ask for.
Reason: It gives the owner a page today that shows every screen the device has, drawn from the code of the commit it was built from, with the project's rules kept: no hand-drawn pictures, nothing generated in the repository, and the three words kept apart on the page itself. A simulator in the browser is the better page and a project of its own; it should not be started on the way to this one.
Consequences: `site/index.html`, `tools/lang/site.{en,de,fr}.toml`, `scripts/make_site.py`, `scripts/check_i18n.py` (`--usage` reads `site/*.html`), `test/test_make_site/test_site.py` and `test/test_site_page/test.mjs` (listed in `test_ignore` of both host environments and run by `ci.yml`), `.github/workflows/pages.yml`, [site.md](process/site.md), [i18n.md](core/i18n.md), [translating.md](translating.md), a line in the README. The demo page takes its language from the address too, before the browser's, and falls back to English (`sim/demo/page.html`, `test_demo_page`). The owner switches GitHub Pages to "GitHub Actions" in the repository's settings once (open point 43); the page is `https://paulwilke.github.io/Phone-OS/`. The decision is open until the owner has seen the page and confirmed its shape.
Verified: host-tested (`test_site.py`, the page assembled and opened in a browser from `out/site/`); published on 2026-10-25 by `.github/workflows/pages.yml` after the merge of pull request 66, and seen at https://paulwilke.github.io/Phone-OS/ in English, German and French with the keys and the taps of the device working; not applicable on the device

## D-49: The screens report their touch targets, and the walk taps them
Date: 2026-10-25   Status: decided (the rule that keeps keys and taps from changing the world, "none during a call, none on the content of the compose page": superseded by D-53)
Decided by: owner (2026-10-25: the touch screen must work on the web page); agent (confirmed by owner on 2026-10-25)

Context: The web page (D-48) showed the device with its seven keys, and the owner asked why the touch screen did not work: on the device the tiles, the rows and the soft keys are tapped, and a page that shows the device without that shows it wrong. The walk of the simulator pressed keys only, so no picture reached by a tap existed, and no state knew where a finger would hit something. A simulator start costs about 40 ms, and the walk has about 250 states.
Options: Tap a grid of points on every state blindly (300 points at 16 pixels: 75,000 starts, about an hour, and the regions found would be as coarse as the grid). Read the pressable rectangles off the picture (frames, inverted rows: a heuristic that breaks with the first new widget). Let every screen say where a tap does something, through the rectangles its `onInput` already tests a touch against, and tap the middle of each (one more start per state to ask, one per target to tap; exact rectangles; a change in the interface).
Decision: The third. `Screen::touchTargets()` returns the places that answer a tap as `TouchTarget` (a rectangle and, for a soft key, its key); `AppScreen` lists the soft keys that are shown and the screen's `targets(area)`; the widgets give theirs (`TileGrid`, `ListView`, `OptionRows`), and the screens with buttons of their own list them (SOS, compose, messages, self-test, radio, number, call). `ScreenStack::visible()` hands the visible screen out. The crawler asks every state for its targets, taps the middle of each, and records the tap's rectangle in the graph (`"tap"` edges); the rule that keeps the world as it is holds for taps too (none during a call, none on the content of the compose page, a soft key counts as its key). The demo page follows a click on the picture, and the web page lays the targets over the picture as invisible buttons.
Reason: The screens know where a finger hits something; asking them is exact and costs a few hundred starts, not tens of thousands, and the same answer makes a test possible that no heuristic allows: every target of every state of a short walk is used when its middle is tapped, and no two overlap, so a screen that reports a rectangle its handler ignores fails the tests.
Consequences: `src/ui/Screen.h` (`TouchTarget`, `touchTargets()`), `src/ui/ScreenStack.h` (`visible()`), `src/ui/AppScreen.{h,cpp}` (`touchTargets()`, `targets()`), `src/ui/TileGrid.{h,cpp}`, `src/ui/ListView.{h,cpp}`, `src/ui/OptionRows.{h,cpp}` (`targets()`), fourteen screens under `src/ui/screens/`, `sim/Crawler.h` (paths as console lines, tap edges, `targets()`), `sim/demo/page.html`, `site/index.html`; [ui-framework.md](ui/ui-framework.md), [demo.md](process/demo.md), [site.md](process/site.md). Tests: `test_widgets` (`test_the_grid_reports_the_dots_and_the_tiles_on_show_as_touch_targets`, `test_the_list_reports_the_rows_on_show_as_touch_targets`, `test_the_option_rows_report_the_rows_on_show_as_touch_targets`), `test_simulator` (`test_the_walk_taps_the_targets_of_the_screens_and_the_graph_carries_their_rectangles`, `test_every_touch_target_a_screen_reports_is_used_when_its_middle_is_tapped`, the tap cases of `worldStaysAsItIs`), `test_demo_page` (a click on the picture). Nothing changes on the device: the firmware does not call `touchTargets()`. The walk takes longer (one start per state to ask, one per target); the numbers of a run are in demo.md.
Verified: host-tested; the firmware compiles; not applicable on the device

## D-50: The web page carries the owner's legal notice, the one personal datum the repository holds on purpose
Date: 2026-10-25   Status: superseded by D-52
Decided by: owner (2026-10-25: the notice goes onto the page itself, with name, address and e-mail, no telephone number)

Context: German law (§ 5 DDG) asks a public page that is not purely private for a legal notice with the provider's name, a postal address and an e-mail address; whether a non-commercial project page needs one is a grey area, and the owner wants the page safe rather than argued about. The repository's rule is that no personal data enters it, ever ([CONTRIBUTING.md](../CONTRIBUTING.md)), and `scripts/privacy_check.py` fails on every e-mail address. The owner's own site, which could hold the notice, is not ready.
Options: No notice (the page is private, but the reading is uncertain). A link to a notice on the owner's own domain (keeps the data out of the repository; not available yet). The notice on the page itself, its lines as `site.legal.*` words of the catalogs (public in the repository and its history, which a legal notice is by nature). The data injected at build time from repository variables (out of the repository, but a page that differs between a local build and the published one, and one more hidden part).
Decision: The third. `site/index.html` gets a section `#legal`, linked from the footer, with the owner's name, street, town and country, the e-mail address as a `mailto:` link, and three sentences on privacy (no cookies, no analytics, no tracking, no form; the language is in the address only; GitHub as the host receives the IP address and handles it under its own privacy statement, which is linked). The demo page, published under the web page, links the notice too. The rule against personal data gets its one exception, written where the rule is stated, and `privacy_check.py` knows that one address by name and no other. No telephone number (owner).
Reason: A notice costs nothing and ends the question; the page is public anyway, so the address being public in the repository is the notice's purpose, not a leak. The exception is narrow and named, so the check still fails on any other address.
Consequences: `tools/lang/site.{en,de,fr}.toml` (`site.legal.*`, `site.footer.legal`), `site/index.html`, `sim/demo/page.html` and `tools/lang/demo.*.toml` (`demo.legal-notice`), `scripts/privacy_check.py` (`LEGAL_NOTICE_EMAIL`), [CONTRIBUTING.md](../CONTRIBUTING.md), the README, [site.md](process/site.md). Tests: `test_site_page` (the notice names its lines and its link by keys, no address in the markup, the footer links it), `test_demo_page`. When the owner's own domain carries a notice, a new decision may move it there.
Verified: host-tested; not published yet

## D-51: One prerendered page per language; the language is the path, not a query parameter
Date: 2026-10-25   Status: decided
Decided by: owner (2026-10-25: yes, after the trade-off was laid out); agent (the way)

Context: D-48 made the page's language the address's query parameter (`?lang=de`), switched in by the page's script: the delivered HTML held English only. A link preview of `?lang=de` therefore showed English, search engines saw one English page, and a reader without a script read English whatever the address said. The words of all three languages are already in the catalogs; only the assembly wrote one of them in.
Options: Keep the one page and the script (no change; the previews and the search engines stay English). Build one page per language from the same `site/index.html` and catalogs, the language in the path (`/`, `/de/`, `/fr/`), with `canonical` and `hreflang` links (prerendered words everywhere; the paths of a sub-folder page need a root prefix; the `?lang=` links already shared need a redirect). Serve the language by content negotiation (not possible on GitHub Pages).
Decision: The second. `scripts/make_site.py` renders the page once per catalog it finds, English at the root as the default, each with its own words written in, `<html lang>` set, `data-root` for the paths, only its own words embedded, and `canonical`/`hreflang` links built from `--url` (default `https://paulwilke.github.io/Phone-OS/`, for the custom domain to come). The selector navigates to the other language's page; `?lang=<code>` on the root page redirects to that page so that shared links keep working. The demo page keeps its own `?lang=` and links the legal notice of its language's page. This supersedes the language part of D-48; everything else of D-48 stands.
Reason: The words exist; writing them in at build time costs one loop, and it is what gives German and French readers a page that is theirs before any script runs, in previews and in search.
Consequences: `scripts/make_site.py` (`--url`, one page per language, `NEEDED` per language), `site/index.html` (`data-root`, the alternates placeholder, the script's `ROOT` and redirect), `sim/demo/page.html` (the legal link follows the language), `test/test_make_site/test_site.py`, `test/test_site_page/test.mjs`, [site.md](process/site.md). The address of a German reader is `https://paulwilke.github.io/Phone-OS/de/`; `?lang=de` still arrives there.
Verified: host-tested; not published yet

## D-52: The legal notice leaves the web page again
Date: 2026-10-26   Status: decided
Decided by: owner (2026-10-26)

Context: D-50 put the owner's legal notice (name, address, e-mail) onto the web page, as the one personal datum the repository holds on purpose, and `scripts/privacy_check.py` was given an exception for that one address. On 2026-10-26 the owner asked for the notice to be removed again, in all three languages, and the page republished.
Options: Keep the notice on the page (D-50). Remove it and keep the exception in the privacy check for later. Remove it and the exception, so that the repository's rule against personal data holds again without exception.
Decision: The third. The section, its words in the three catalogs, the footer link, the demo page's link, the exception in `privacy_check.py` and the sentences about the exception in CONTRIBUTING.md and the README are removed; the page carries no personal data. D-50 is superseded. Whether and where a notice is published later is the owner's, and a new decision when it comes.
Reason: The owner's decision. The repository's rule is simpler without an exception, and the check fails again on every e-mail address.
Consequences: `site/index.html`, `tools/lang/site.*.toml`, `sim/demo/page.html`, `tools/lang/demo.*.toml`, `scripts/make_demo.py`, `scripts/privacy_check.py`, CONTRIBUTING.md, README.md, [site.md](process/site.md), `test_site_page`, `test_demo_page`. The address stays in the history of the repository, as every commit does.
Verified: host-tested; not published yet

## D-53: The demo shows exactly English, German and French, and every key that is drawn works as on the device
Date: 2026-10-26   Status: decided
Decided by: owner (2026-10-26: the demo must show exactly English, German and French and every drawn key must work as on the device); agent (the way)

Context: Two testers of the live page found that it "hangs at some places" and that on the Language page in French only German could be chosen, and then a row "[Pseudo]" was offered. The walk of D-48 and D-49 was found once in English and replayed in German and French, but the Language page puts its cursor on the language in use, so the same keys pick different rows in each language: in French, Up and Enter land on German, English is unreachable in German and French, and a tap on the English row led to a state whose French picture still said French. The simulator's pseudo-language (a layout tool, `qps-ploc`) was a fourth row. A cap of 30 states per screen (`--per-screen`) left a key that is drawn without an edge on about 170 pairs of a state and a key (Back, Home and Inbox on the language page, after choosing a language, on the settings, clock, radio, messages, recipient and phone pages, and Back on the SOS page, which could not be left). The walk pressed no key during a call, so Hang up did nothing and "start over" was the only exit; the key test of the self-test page says "Ends in 6 s without input" and never ended on the page; a ready-made message was not chosen; after a click with the mouse on a touch target or on a key that the move disables, the keyboard stopped driving the device; and the English page's link to the full demo carried no language, so a German browser opened a German demo.
Options: Keep one graph found in English and replay it per language (the keys pick the wrong rows; this is what failed). One graph per language, each found and replayed alone (the Language page leads out of a language into the others, so each graph would hold the other two languages' states as well: three times the work for the same pictures). One graph over (language, screen, picture), started once in each language, each state's picture in its own language (each language's page begins at its own start; the Language page leads from one into the other; one walk). For the cap: a higher cap (still cuts), or none, with the depth and the number of states as fuses that fail the run (the walk is the whole graph, or it is reported as not complete). For the pseudo-language: remove it from the simulator (the layout tests and docs/translating.md use it) or leave it out of the demo only. For a call: keep the rule and explain on the page (the owner asked that Hang up work), or let the walk press the keys (Hang up is a state change that leaves the world as it was; the other keys do nothing on the device while it rings or a call is on). For the ready-made message: keep the rule and explain, or follow it (the device shows the home screen after it; a walk that carried the sent message would multiply every list by every message that could have been sent, so the walk keeps no history). For the key test: a key the walk takes as the timeout, or a timed edge that the page follows. For the focus: give it back to the device after every move, or only when the key that was clicked is gone or disabled.
Decision: The third for the graph. `program --crawl` starts the device once in each language and follows every key and every tap of every state, with no cap; `--depth` (30) and `--max-states` (20,000) are fuses, and a walk that reaches one is not complete and fails (`Crawl::complete`, exit status 1). The pseudo-language stays in the simulator (`Options::pseudoLanguage`, on by default; `--no-pseudo`) and is never in the walk. The walk presses everything: Hang up works (the Back key and the bar), the other keys of a call are recorded as doing nothing; a ready-made message is sent by Enter and by a tap, the walk follows to the home screen, and the sent message is not carried into the lists that follow (the page shows the world of the start, as the scenes do); on the screens that end by themselves (`Crawler::endsByItself`: the self-test page) the walk lets seven seconds pass and records a `wait` edge, which both pages follow when the time has passed. Choosing the language already in use does nothing on the device (`AppCore::setInterfaceLanguage` with the same code) and nothing on the page. The device keeps the keyboard focus: after a move the web page gives it back to the device unless an enabled key of the device has it. The link from every page of the web page to the full demo names the page's language, English too.
Reason: The owner's words are that on the device everything works, and the page must be what the device is. The language is part of what a picture is: a state that is not in a language has the wrong keys. A fuse is honest where a cap is not: the demo is the whole graph or the build says it is not, and a key that is drawn leads where the device would lead. The depth of the deepest state is a number to watch, not a limit that cuts. The pseudo-language is a tool for the translator and the layout tests, not a language of the device.
Consequences: `sim/Crawler.h` (a graph over (language, screen, picture); starts; `idle`, `wait`; no `worldStaysAsItIs`; `run(maxDepth, languages, maxStates)`; `each()` and `setJobs()` run the replays of one state on several processors, the graph the same whatever the number), `sim/main.cpp` (`--no-pseudo`, `--max-states`, `--jobs`, `--depth` 30; `--per-screen` is gone), `sim/Simulator.h` (`Options::pseudoLanguage`), `platformio.ini` (`-O2` for the `sim` environment: the walk is three times faster), `scripts/make_demo.py` (`check_graph`, `SOFT_KEY_IDLE_OK`), `sim/demo/page.html`, `site/index.html`, `scripts/make_site.py` (the graph's `starts`: the first picture and the preview image of a language's page are its start state), `tools/lang/demo.*.toml` (`demo.trimmed` and `demo.call-note` are gone), [demo.md](process/demo.md), [site.md](process/site.md), [simulator.md](simulator.md). The walk of this tree has 4,600 states (1,536 English, 1,532 German, 1,532 French), 68,699 edges, 4,645 pictures and 44 MiB of PNG files; one run takes about 14 minutes on one processor. Tests: `test_simulator` (`test_a_walk_that_reaches_a_limit_says_it_is_not_complete_and_nothing_else_cuts_a_state`, `test_every_key_of_every_followed_state_has_an_edge_or_a_note_and_every_language_its_start`, `test_hang_up_works_in_the_walk`, `test_a_ready_made_message_is_sent_in_the_walk_and_leads_to_the_home_screen`, `test_the_key_test_of_the_self_test_page_ends_by_itself_in_the_walk`, `test_the_language_page_lists_the_three_languages_and_each_can_be_chosen_from_each_other_by_key_and_tap`, `test_the_pseudo_language_is_a_choice_of_the_simulator_and_not_of_the_demo`), `test/test_make_demo/test_graph.py` (every rule of `check_graph`, each failing when broken, which `make_demo.py` also holds the real graph to on every build), `test_demo_page` (the start of each language, the picture in the language of the state, Hang up, the wait edge), `test_site_page` (the same, the focus, the link with `?lang=en`), `test_make_site` (the start pictures, a graph without starts). Nothing changes on the device. The sent message that the walk does not carry and the world of the start that the page always shows are the limits of a demo that is pictures and not the simulator running (D-48).
Verified: host-tested; published on 2026-10-26 by `.github/workflows/pages.yml` after the merge of pull request 74, and seen at https://paulwilke.github.io/Phone-OS/ (French to English on the Language page, Hang up); a restart from the Radio page came back in English, fixed by the pull request that follows; not applicable on the device

## D-56: The help texts of the parents' page come one tab at a time
Date: 2026-10-29   Status: decided
Decided by: owner (2026-10-29: the answer should be made smaller, not the guard larger); agent (the way)

Context: The help answer of the parents' page was one JSON object with every `config.*.help` key, 5.3 KB in English, 5.8 KB in German and 6.1 KB in French, built in the static response buffer of 12288 bytes (`kResponseMax`) that the device holds for ever; an answer that overflows it is a 500. `test_sim_web` keeps every answer under half of the buffer so that a user's own catalogue with longer sentences still fits, and the guard had to be raised from 6000 to 6200 bytes (pull request 81) because two help texts were added. The owner asked whether the answer can be made smaller instead.
Options: Shorten the texts (saves 10 to 20 %, needs a native speaker for each language, and the next two texts bring it back). Send the answer in chunks (needs `WebServer` to stream, a high effort). Load the help per tab (`/help?tab=basics`), so that each answer is 1 to 2 KB (a medium effort, and it makes room for more languages).
Decision: Per tab. `GET /api/lang/<code>/help?tab=<id>` answers the help of the fields of one tab (`basics`, `school_time`, `advanced`, `contacts`, `secrets`); the page asks for it when the tab is first shown and keeps it until the language changes. The tab of a key is generated from `config/schema.toml` (its `group`, else its file) into `config::KeyDef::tab`, so that the device and the page cannot disagree about which help belongs to which tab. Without `tab=` the route answers 400 (an old answer of all texts at once would need the 6 KB again, and nothing else asks for it), for a tab the page does not have 404.
Reason: It is the only option that makes the largest answer smaller by a factor of three without touching the texts or the server. Shortening is for a translator, not for this change, and streaming is a larger project than the problem.
Consequences: The largest help answer is French `basics` with 2165 bytes (English 1794, German 2011); `test_sim_web` keeps every tab of every shipped language under 3000 bytes, checks that the tabs together carry every help key the form uses, and keeps the words and the labels under 6000. The page makes one more request per tab opened, and the log gains one line for each. Pull request 81 adds two help texts to `basics`; they fit. A new tab or a new group needs nothing but its keys in the schema. Not verified on the device: the timing of one request per tab on the Wi-Fi of the device.
Verified: host-tested (`test_sim_web`, `test_web_page`); compiled for the device; not verified on the device
