<!-- allow-non-english: quotes the words of the German and French catalogs. -->
# Open points

The one list of open work. Every other document says what exists and what the host tests prove; where it says "not verified", the item is here with the person who has to act. Nothing is listed here that a later document describes as done: when an item is finished, it is removed in the commit that finishes it.

The owner is the person who owns the device and the repository. Software work that needs no device is not listed: it is done before it is merged.

## On the device

| # | Open point | What exactly | Owner | Described in |
|---|---|---|---|---|
| 1 | Smoke test of the firmware | Flash and go through the 28 steps with the date and result columns. The first picture on the panel (steps 2 and 24) decides whether the display profile is right; nothing else in the display chain has been seen on this panel from this firmware. | owner | [bring-up.md](bring-up.md), [device-display.md](device-display.md) |
| 2 | The keys for Up and Down | The `$` key and the microphone key stand in for Up and Down. Try them while typing and on the pages; report whether other keys are better. | owner | [bring-up.md](bring-up.md) step 3, [device-input.md](device-input.md) |
| 3 | Battery gauge and charger (second discharge run done 2026-10-14; left: what to do with what it showed) | Seen on the device: capacity 1500 mAh, the percentage agrees with the voltage, "charge done" on the cable. The second discharge run (radio off, modem asleep, 2026-10-13 to 14): 12.8 hours at about 105 mA from 4099 mV to the switch-off at 3291 mV, about 1340 mAh; the gauge fell from 45 % to 7 % in ten minutes at 3.62 V and said 0 % from 3596 mV for the last five hours, so it takes the cell for empty at about 3.6 V; after the charge it reports a learned full charge of 1372 mAh and the firmware writes 1500 over it at every start (open point 35). | owner | [hardware-lessons.md](hardware-lessons.md), [battery.md](battery.md), [energy-log.md](energy-log.md) |
| 4 | Calls over VoLTE | Reproduce the broken state, flash, dial, cold-start the modem and dial again; the IMS must register with the context set up by the driver. | owner | [bring-up.md](bring-up.md) steps 25 to 28, [volte.md](volte.md) |
| 6 | The vibration patterns | The five patterns come from the data sheet; whether they can be told apart in a pocket has not been felt. | owner | [bring-up.md](bring-up.md) step 6 |
| 7 | The simulator window | Look at the window version of the simulator once (`pio run -e sim-sdl`, in a terminal): the build needs SDL2 and CI does not make it. | owner | [simulator.md](simulator.md) |
| 19 | Changes of 4.1.3 to 4.1.7 | Seen on the device: the energy log (it is written and read), the Lock button (works). Host-tested and not verified on the device: the battery shown from the voltage when the gauge fails (the gauge works now, so it has not been needed), the position of the GPS note, and the 15-minute search for the home network (needs a trip abroad or a roaming SIM). Each entry of the changelog says what to look for. | owner | [../CHANGELOG.md](../CHANGELOG.md), [energy-log.md](energy-log.md), [battery.md](battery.md), [modem-driver.md](modem-driver.md) |

## Decisions

| # | Decision | The question | Recommendation | Owner | Described in |
|---|---|---|---|---|---|
| 9 | The LoRa radio | The interface, the radio page, the settings and the message path exist and are tested against a fake. There is no implementation for the chip: it needs a driver (RadioLib) and a protocol of our own, because MeshCore is not used. Decided by the owner: not in 4.0, in a later release. Save mode (radio only, cellular and Wi-Fi off) was removed from 4.0 and comes back with the radio project, where an emergency radio mode switches battery saving on implicitly. | Build it for a later release: driver, protocol, then the trial on the device. | owner | [device.md](device.md), [messaging.md](messaging.md), [screen-radio.md](screen-radio.md); decision [D-1](decisions.md) |
| 25 | The processor does not sleep: where the 105 mA go | Two nights measured ([D-25](decisions.md), [D-33](decisions.md)). The step was the battery chips, read five times a pass: fixed in 4.2.4 ([D-31](decisions.md), 52 ms to 0.4 ms, seen on the device). The sleep ended at once because the keyboard's interrupt line and the modem's ring line idle low and were armed to wake on low: 4.2.5 gives the three wake lines pull-ups and arms each with the level it does not have ([D-34](decisions.md)). To read after a night with 4.2.5: the start line "Wake lines: ...", the sleep part of the loop line (most sleeps ended by the timer, "asleep" most of the half hour) and the energy log's current. | owner | [power-policy.md](power-policy.md), [device-console.md](device-console.md), [hardware-lessons.md](hardware-lessons.md); decisions [D-31](decisions.md), [D-33](decisions.md), [D-34](decisions.md) |
| 26 | The charge current at a mains adapter | Seen on the device: 960 mA and the watchdog off. With the cable the charger limits its input to 500 mA ("unknown adapter", register 00 = 0x48), so the cell charged at 445 to 500 mA from the computer and from a mains adapter; the limit of 960 mA is not reached, and the charging is gentle (about 0.3 C) and slow (about 3.5 hours). Whether the input limit should be raised for a real adapter is a choice for later: it would make charging faster and the cell's limit (D-14) the one that holds. | Nothing to do; say if charging is too slow. | owner | [device-power.md](device-power.md), [hardware-lessons.md](hardware-lessons.md); decision [D-14](decisions.md) |
| 30 | Labels missing on the parents' page | On 2026-10-12 (17:45) the page on the device showed no labels while the APN was cleared; the device log had no line about the catalogs in that session, so the page's language requests did not arrive or were not logged. The same page in the simulator showed every tab and label (all three answers 200, under 6 KB in three languages); the web code is the same as in 4.1.7 apart from the APN hint. Not a regression of the code that the checks can find. Version 4.2.1 logs the three requests. Next time: reload the page with the browser's cache emptied, open its developer tools (Network) and look at the status of `/api/lang/<code>`, `/labels` and `/help`, and tell the agent. | Reproduce once with the developer tools open. | owner | [web-interface.md](web-interface.md) |
| 33 | The USB console answers any computer | The console asks for no password (D-28); the documentation says so. Choices for a later version: answer only while the device is unlocked, only in the first minutes after a start, or ask for the web password on the lines that write or send. | Answer only while the device is unlocked: the parents unlock it when they set it up, a child's device in a pocket is locked. | owner | [device-console.md](device-console.md); decision [D-28](decisions.md) |
| 36 | The lock screen once showed 35 unread messages | Seen by the owner on the evening of 2026-10-13: the lock screen (and the Inbox tile) said 35 unread messages; on 2026-10-14 the count was right again. The store rebuilds the list from `messages.log` at every start and applies the read mark afterwards ([message-store.md](message-store.md)); a start whose read mark was missing or damaged would count every stored message as unread. Not reproduced; to watch after a restart, and to read `messages.log` over the cable if it happens again (`damagedLines()` is not in the log yet). | owner | [message-store.md](message-store.md), [screen-lock.md](screen-lock.md) |
| 20 | Operations that block the main loop | Measured on the device (2026-10-13, [D-20](decisions.md)): about 32,000 passes in 1800 s, the longest pass 123 to 206 ms apart from the start (1479 ms). Nothing blocks for seconds in normal running. Not yet seen: the motion page's sensor start (about 11 s). Open the motion page once, and read the next `Loop:` line. | Open the motion page once, then have the log read. | owner | [device-console.md](device-console.md), [screen-motion.md](screen-motion.md); decisions [D-8](decisions.md), [D-20](decisions.md) |

## Licences and release

| # | Open point | What exactly | Owner | Described in |
|---|---|---|---|---|
| 15 | A native speaker for German and French | The catalogs and guides were read for wording by the author, not by a native speaker. The owner judges the words on the device while using it, because the right word depends on the context (the Power tile showed that: "Strom" was wrong and became a symbol, [D-21](decisions.md)). Places where an opinion is wanted are listed below. | owner | [translating.md](translating.md) |
| 16 | Releases and tags | The repository is public and carries the tag `v4.1.2`; versions 4.1.3 to 4.2.0 are merged or on their way to `main` into `main` and have no tag. Still the owner's: the tags, and what happens to the old repositories. Nothing is archived or deleted without the owner's decision. | owner | [release-4.2.md](release-4.2.md), [../CHANGELOG.md](../CHANGELOG.md) |

## Wording for a native speaker

Where a word was chosen because a better one does not fit, or where two good ones compete. A tile on the home screen has room for 68 pixels of text, a soft key for about a word.

| Language | Place | Choice | Alternative |
|---|---|---|---|
| German | tile `home.settings` | "Option" (singular), chosen when "Optionen" did not fit | "Optionen": 65 pixels, it fits since the display fonts are Helvetica |
| German | `messages.call-back` (soft key) | "Rückruf" | "Zurückrufen" does not fit |
| German | the words for the access point and the parents' tools | "Zugangspunkt", "Elternwerkzeuge", "Zugangsdaten" for the tab of `secrets.toml` | "Hotspot", "Elternfunktionen" |
| German | `notice.message-sent` | "Nachricht ist raus" (colloquial) | "Nachricht gesendet" |
| German | address of the child | "Du" in the history, bare infinitives in hints | |
| French | tile `home.inbox` | "Courrier"; "Messages" is 70 pixels | "SMS" |
| French | tile `home.motion` and `motion.reset` | "Mouv.", "Réinit." | |
| French | `home.settings`, `wifi.home-network` | "Réseau maison" | "Réseau domestique" |
| French | `selftest.on`, `selftest.off` | "oui" and "non" ("Lumière : oui") | "activé" (does not agree in gender with both lines) |
| French | school time | "En classe" on the status bar, "heures de classe" in the page and the guide | "heures scolaires" |
| French | address | the child's texts say "tu" ("Peux-tu m'appeler ?"), the hints on the device say "vous" ("Appuyez", "Tapez") | one form throughout |
| French | page | "Téléverser" for upload, "Outils des parents", "Accès" for the tab of `secrets.toml` | "Envoyer", "Secrets" |
