<!-- allow-non-english: quotes the words of the German and French catalogs. -->
# Open points

The one list of open work. Every other document says what exists and what the host tests prove; where it says "not verified", the item is here with the person who has to act. Nothing is listed here that a later document describes as done: when an item is finished, it is removed in the commit that finishes it.

The owner is the person who owns the device and the repository. Software work that needs no device is not listed: it is done before it is merged.

## On the device

| # | Open point | What exactly | Owner | Described in |
|---|---|---|---|---|
| 1 | Smoke test of the firmware | Flash and go through the 28 steps with the date and result columns. The first picture on the panel (steps 2 and 24) decides whether the display profile is right; nothing else in the display chain has been seen on this panel from this firmware. | owner | [bring-up.md](bring-up.md), [device-display.md](device-display.md) |
| 2 | The keys for Up and Down | The `$` key and the microphone key stand in for Up and Down. Try them while typing and on the pages; report whether other keys are better. | owner | [bring-up.md](bring-up.md) step 3, [device-input.md](device-input.md) |
| 3 | Battery gauge and charger | After the first start the gauge's capacity must read 1500 mAh, the percentage must agree with the cell voltage, and a full battery on the cable must read "charge done". Then a second discharge run with the radio switched off, to see whether the gauge is right on its own. | owner | [bring-up.md](bring-up.md) steps 7 to 10, [hardware-lessons.md](hardware-lessons.md), [device-power.md](device-power.md) |
| 4 | Calls over VoLTE | Reproduce the broken state, flash, dial, cold-start the modem and dial again; the IMS must register with the context set up by the driver. | owner | [bring-up.md](bring-up.md) steps 25 to 28, [volte.md](volte.md) |
| 5 | The empty APN | With an empty `apn` the driver leaves the modem's own contexts alone. Whether a modem that was never configured then registers the IMS has not been tried. | owner | [volte.md](volte.md), [modem-driver.md](modem-driver.md) |
| 6 | The vibration patterns | The five patterns come from the data sheet; whether they can be told apart in a pocket has not been felt. | owner | [bring-up.md](bring-up.md) step 6 |
| 7 | The simulator window | Look at the window version of the simulator once (`pio run -e sim-sdl`, in a terminal): the build needs SDL2 and CI does not make it. | owner | [simulator.md](simulator.md) |
| 8 | Question to the manufacturer | The modem firmware `A7682M7_B15V01_241119` hangs in the call setup when the IMS is not registered, and no update package is published. | owner | [volte.md](volte.md) |
| 19 | Changes of 4.1.3 to 4.1.7 | Host-tested and not verified on the device: the energy log, the battery shown from the voltage when the gauge fails, the SOS text sent in ASCII to stay one message, the Lock button and the position of the GPS note, and the 15-minute search for the home network (not verified on a real network). Each entry of the changelog says what to look for. | owner | [../CHANGELOG.md](../CHANGELOG.md), [energy-log.md](energy-log.md), [battery.md](battery.md), [modem-driver.md](modem-driver.md) |

## Decisions

| # | Decision | The question | Recommendation | Owner | Described in |
|---|---|---|---|---|---|
| 9 | The LoRa radio | The interface, the radio page, the settings and the message path exist and are tested against a fake. There is no implementation for the chip: it needs a driver (RadioLib) and a protocol of our own, because MeshCore is not used. Decided by the owner: not in 4.0, in a later release. Save mode (radio only, cellular and Wi-Fi off) was removed from 4.0 and comes back with the radio project, where an emergency radio mode switches battery saving on implicitly. | Build it for a later release: driver, protocol, then the trial on the device. | owner | [device.md](device.md), [messaging.md](messaging.md), [screen-radio.md](screen-radio.md) |
| 10 | A table of APNs by operator | A built-in table by MCC and MNC, or an empty APN with an `IPV4V6` context, would let the modem register the IMS without a configured APN. Both are not built. A table is silently wrong for operators not in it. | Keep the explicit APN in the configuration. | owner | [volte.md](volte.md) |
| 11 | The size limit of a catalog on the card | 32,768 bytes for each file. It is a security limit and was set by the owner; look at it again with real memory numbers from the device. | Keep. | owner | [translating.md](translating.md), [i18n.md](i18n.md) |
| 12 | The self-test words | The words of the self-test page are in the device catalog although only a parent sees that page. | Keep them there: the page runs on the device and needs them without the card. | owner | [screen-selftest.md](screen-selftest.md) |
| 17 | School-time windows of a 3.2 card | 3.2 accepted German day names (`mo-fr`, `taeglich`) and three windows on one day; 4.0 refuses both, reports them, and the school time is then shorter than the card says ([upgrading.md](upgrading.md), [quiet-hours.md](quiet-hours.md)). Decided by the owner: both refusals stay. | Write the school-time windows anew in the format of 4.0 (`mon-fri`, at most two windows a day) before the first start; the status page counts what is refused. | owner | [upgrading.md](upgrading.md), [quiet-hours.md](quiet-hours.md) |
| 18 | Protection against deep discharge | Nothing in the firmware or in this repository shows that the cell is protected: no protection circuit is documented, and the firmware does not switch off at a low voltage. On 2026-10-09 the device ran on down to 3426 mV, where the gauge had said 0 % for hours. Find out from the cell's data sheet or by looking whether it has a protection circuit and at what voltage it cuts off; decide, as the owner, whether the firmware should switch the device off at a voltage of your choice (a child's emergency phone that switches off earlier is a trade-off, and the limit is yours to set). | owner | [battery.md](battery.md) |

## Licences and release

| # | Open point | What exactly | Owner | Described in |
|---|---|---|---|---|
| 14 | Handing out a firmware binary | The Bosch image of the motion sensor comes inside SensorLib and carries no licence text: until Bosch's redistribution terms are verified, a binary is for the owner's own device and is not published. The binary also contains LGPL code (the Arduino core) and Bosch's BSD-3-Clause drivers; what that asks of a published binary has not been assessed. | owner | [THIRD_PARTY.md](../THIRD_PARTY.md), [hardware-drivers.md](hardware-drivers.md) |
| 15 | A native speaker for German and French | The catalogs and guides were read for wording by the author, not by a native speaker. The places where an opinion is wanted are listed below. | owner | [translating.md](translating.md) |
| 16 | Releases and tags | The repository is public and carries the tag `v4.1.2`; versions 4.1.3 to 4.1.7 are merged into `main` and have no tag. Still the owner's: the tags, and what happens to the old repositories. Nothing is archived or deleted without the owner's decision. | owner | [release-4.1.md](release-4.1.md), [../CHANGELOG.md](../CHANGELOG.md) |

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
