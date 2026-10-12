# Decision log

The running log of decisions: what was chosen, what was refused, why, and how it is checked. It exists so that a decision made in a conversation does not live only in that conversation, and so that the owner and the agent both read and update it. The way of working is in [CLAUDE.md](../CLAUDE.md); open work is in [open-points.md](open-points.md), lessons measured on the device are in [hardware-lessons.md](hardware-lessons.md).

Entries are added at the end and are never rewritten or deleted. To change a decision, add a new entry that supersedes the old one; the only edit to an old entry is its status line.

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
Verified: host-tested; memory numbers from the device not taken

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
Consequences: Open point 18 (to be built). `kBatteryEmptyMv` (3430) and the voltage table in `BatteryPlausibility` change with it; the table was measured down to 3426 mV only, so below that it extends a measurement. The number is the agent's: if the device shows a dip that switches it off too early, a new entry changes it. The cell size of 1500 mAh in the firmware agrees with the maker's page for this model.
Verified: not verified; the discharge to 3426 mV was measured on the device on 2026-10-09

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
Verified: not verified; not built

## D-10: Texts go out in PDU mode, in the GSM alphabet, as one concatenated message
Date: 2026-10-12   Status: decided
Decided by: agent (the owner left the choice to the agent on 2026-10-12 and wants to test it)

Context: The driver sends every text that is not plain ASCII as UCS2, which holds 70 characters, and cuts a longer text into separate messages because it has no concatenation header (that needs PDU mode). A German umlaut therefore halves the length and may split the message, although the umlauts, the sharp s and the common accented letters are in the GSM alphabet and cost nothing there. The fix of 4.1.5 (D-6) hides this for some texts by writing "oe" for the umlaut. The owner does not want a text shortened or changed because it has an umlaut.
Options: Keep the folding to ASCII (changes the child's words). Rewrite single wordings (names still bring umlauts). PDU mode with the GSM 7-bit alphabet where every character is in it, UCS2 where not, and a concatenation header for long texts (a larger piece of work in the driver). Remove the folding and accept the split.
Decision: PDU mode. A text whose characters are all in the GSM alphabet goes out as 7-bit (160 characters, 153 per part when long), any other as UCS2 (70, or 67 per part), and long texts as one concatenated message that the receiving phone shows as one. The folding of D-6 is removed when this works.
Reason: It removes the cause instead of working around it, and the words of an SOS stay as written.
Consequences: Open point 22 with its "done". The folding of 4.1.5 stays until PDU mode is built and tested, so that nothing is worse in between. Receiving stays as it is. The A7682 must accept PDU sending with `AT+CMGF=0`; that is the first thing to try on the device.
Verified: not verified; not built
