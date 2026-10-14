# Decision log

The running log of decisions: what was chosen, what was refused, why, and how it is checked. It exists so that a decision made in a conversation does not live only in that conversation, and so that the owner and the agent both read and update it. The way of working is in [CLAUDE.md](../CLAUDE.md); open work is in [open-points.md](open-points.md), lessons measured on the device are in [hardware-lessons.md](hardware-lessons.md).

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
Consequences: The radio page stays on the fake until the driver exists. Open point 9 keeps the build for the later release; [device.md](device.md), [messaging.md](messaging.md) and [screen-radio.md](screen-radio.md) describe what exists.
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
Consequences: Not to be weakened by the agent on its own initiative. To be looked at again with real memory numbers from the device (open point 11); [translating.md](translating.md) and [i18n.md](i18n.md) describe the limit.
Verified: host-tested. Memory numbers from the device (2026-10-12): about 8.3 MB of heap free; loading the three web catalogs, about 42 KB of text together, took about 106 KB of heap. A catalog of 32,768 bytes costs well under 100 KB, so the limit is far from the memory; it stays as the owner set it (a security limit).

## D-4: The APN stays explicit in the configuration
Date: 2026-10-11 (recorded afterwards)   Status: superseded by D-9
Decided by: agent (not confirmed by owner)

Context: The modem needs an APN to register the IMS. A built-in table by operator (MCC and MNC) or an empty APN with an `IPV4V6` context could remove the setting.
Options: A table: silently wrong for operators that are not in it. An empty APN with an `IPV4V6` context: not tried on a modem that was never configured. The explicit APN in the configuration.
Decision: Neither alternative is built; the explicit APN stays. This is the recommendation in the open points, not yet the owner's decision.
Reason: A wrong table entry fails silently, and a missing APN fails visibly.
Consequences: Waits for the owner (open point 10); what an empty APN does is open point 5. See [volte.md](volte.md).
Verified: not verified

## D-5: The firmware switches off before the cell is damaged
Date: 2026-10-12   Status: decided
Decided by: owner (the direction); agent (the voltage, which the owner left to the agent on 2026-10-12)

Context: On 2026-10-09 the device ran on down to 3426 mV while the fuel gauge had said 0 % for hours. The owner assumed that a protection circuit guards the cell and does not want to open the case to look. The maker's page for the T-Deck MAX names the cell (3.7 V, 1500 mAh, so lithium-polymer, full at 4.2 V) and the chips (SY6970 charger, BQ27220 gauge), and says nothing about a protection circuit or a cut-off voltage. Neither charger nor gauge protects the cell ([battery.md](battery.md)).
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
Consequences: Open point 22. Texts that need a stand-in are shown unchanged in the history; only the message on the air is folded ([modem-driver.md](modem-driver.md)).
Verified: host-tested; not verified on the device

## D-7: The home screen has a Lock button and no arrows
Date: 2026-10-11 (recorded afterwards)   Status: decided
Decided by: agent (confirmed by owner on 2026-10-12)

Context: Since 4.1.0 the arrows are gone from the home screen on purpose, and the Back key went with them, so nothing said how to lock the device.
Options: Bring the arrows back (the choice of 4.1.0 is undone). A visible Lock button in the bottom bar.
Decision: A Lock button in the bottom bar ("Lock", "Sperren", "Verrouiller"); its word never changes with the selection, so the bar costs no redraw. The arrows stay away.
Reason: The device must say how to lock it without redrawing the bar.
Consequences: Released in 4.1.6; [screen-home.md](screen-home.md). The owner confirmed that it works. A real switch-off is a different thing and is not this button: see open point 23.
Verified: host-tested in all three languages; the owner used it on the device (2026-10-12)

## D-8: What to do about operations that block the main loop
Date: 2026-10-12   Status: open
Decided by: agent (not confirmed by owner)

Context: The loop in `device/main.cpp` is cooperative and single-threaded. The owner sees the screen blocked while the motion page starts the accelerometer, which takes about 11 seconds by the documentation and about 10 seconds by the owner's feel ([screen-motion.md](screen-motion.md)). An outside proposal suggested moving the modem, the GNSS parsing and the card onto core 0 with command and event queues; the modem and the receiver are already non-blocking state machines, so that proposal did not address what was observed.
Options: Leave it: the page says what it does, but an SOS during the 11 seconds waits. Take only the sensor start off the main loop (a one-shot task polled through a flag): a small piece of concurrency. A general two-core split with queues: breaks the single-threaded, host-tested core, shares core 0 with the Wi-Fi stack, and has no measurement behind it.
Decision: None yet. Measure the longest loop pass and the key-to-reaction time on the device first; then choose.
Reason: No measurement shows that the loop as a whole is too slow, only that one known operation blocks.
Consequences: Open point 20. The general split is not started. Nothing changes in the code until the owner chooses.
Verified: not verified

## D-9: The APN is chosen automatically from a built-in list by operator
Date: 2026-10-12   Status: decided
Decided by: owner

Context: The network does not tell the device which APN to use; the device brings one ([volte.md](volte.md): the APN differs from operator to operator). Today it must be typed into `settings.toml`, and ordinary users do not know their APN and should not have to. Phones solve this with a list keyed by the SIM's MCC and MNC.
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

Context: The modem firmware `A7682M7_B15V01_241119` hangs in the call setup when the IMS is not registered. The driver works around it by never dialling without the IMS ([volte.md](volte.md)). LilyGO answered on 2026-10-12 that a newer firmware for the A7682E exists, installed with a tool shared with the A76XX series over the modem's USB endpoints, which on the T-Deck Pro are inside the case, and that they advise against an update unless there is a specific problem.
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

Context: The owner charges the device away from the computer and cannot then read the logs or the battery diagnosis, and a firmware update always needs the cable. He asked whether an update over the air (OTA) and the console over Wi-Fi make sense. What exists: the partition table (`default_8MB`) already has two application slots of 3.2 MB (`ota_0`, `ota_1`) and `otadata`, and the firmware is 1.5 MB, so no repartitioning is needed. The parents' page is plain HTTP on port 80 with a password in a Basic header (no TLS), one connection at a time, and a request body of at most 8 KB in a buffer ([web-interface.md](web-interface.md)).
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
Verified: host-tested (five tests fail with the old rule put back); compiled; verified on the device on 2026-10-14: in the night of 2026-10-13 the log has one line at 20:19 ("the gauge says 7 % at 3624 mV; showing about 43 % from the voltage") and one at 00:55 ("the gauge agrees with the voltage again"), none in between, and the screen followed the voltage to the switch-off at 3291 mV

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
Consequences: Built in 4.2.4 ([device-power.md](device-power.md)); the loop line of the next night is the check: the step under 5 ms. Open point 25 says what to read.
Verified: host-tested (`test_board_power`: five questions cost one reading); not verified on the device

## D-32: The gauge's learned full charge is left to the gauge
Date: 2026-10-14   Status: open
Decided by: agent (not confirmed by owner)

Context: After the first full discharge the fuel gauge reports a learned full charge of 1372 mAh next to the design capacity of 1500 mAh, and every start logs "wrote 1500 mAh to the fuel gauge but it reports design 1500 mAh, full charge 1372 mAh": the rule of D-14 writes on a difference, the gauge learns the difference back, and the write repeats at every start. The gauge's memory wears with every write, which the rule was meant to prevent.
Options: Leave it. Write the design capacity only and compare only it; show the learned full charge in the diagnosis. Write both only while the cycle count is 0.
Decision: None yet. Recommendation: the second.
Reason: The learned value is the gauge doing its job; the night showed about 1340 mAh from 92 % to the switch-off, near what it learned.
Consequences: If confirmed, the start line says "design 1500 mAh, nothing to write; the gauge has learned 1372 mAh" and the diagnosis shows both. Open point 35.
Verified: not verified
