# Release 4.0

Version 4.0.0 of PhonePlus, the emergency phone for a child on the LilyGO T-Deck Pro MAX. What changed against 3.2 is in [../CHANGELOG.md](../CHANGELOG.md), and every open question is in [open-points.md](open-points.md). This page says what the release is, what it is not, and where the owner's checklist stands.

## What the release is

The firmware of `pio run -e device`, built from this repository at the commit that carries the tag `v4.0.0`, together with the simulator, the host tests, the tools for the USB cable and the user's guide in English, German and French. The version is the file `VERSION`; the firmware reads it at build time and shows it on the status page ("Firmware: 4.0.0") and in the first log line of every start.

## What the release is not

- **It has no radio.** The LoRa radio is not part of 4.0. The interface and the pages exist and are tested against a fake, but nothing drives the chip; the feature is off and a request to start the radio is answered with "not available". The decision to leave it out is the owner's and is open point 9.
- **It is not verified on the device.** It was compiled, tested on the host and run in the simulator. The checklist below is empty because the owner has not been through it, not because it passed.
- **It is not a published binary.** The firmware contains a sensor image whose redistribution terms are not verified and LGPL code whose conditions have not been assessed, so a binary is for the owner's own device ([../THIRD_PARTY.md](../THIRD_PARTY.md), open points 13 and 14).

## What was checked, and where

| Kind | What | Where |
|---|---|---|
| Tested on the host | the application, the drivers against scripted fakes, the configuration, the parents' page, the USB tools, the scripts that check this repository | `pio test -e native`, the Python and Node tests of [../CONTRIBUTING.md](../CONTRIBUTING.md) |
| Run in the simulator | every scenario of `sim/scripts/`, and each one fails when the behaviour it pins is put back | `pio run -e sim`, `scripts/counter_check_scenarios.py` |
| Compiled | the firmware for the board | `pio run -e device` |
| Verified on the device | nothing | [bring-up.md](bring-up.md) |

## The owner's checklist

The steps are those of [bring-up.md](bring-up.md), where the date and the result of each are written, by the owner. The state at the time of this release:

| Steps | Subject | Done |
|---|---|---|
| 1 to 2 | first start and the first picture on the panel | no |
| 3 to 6 | keyboard, the three keys, touch, haptic patterns | no |
| 7 to 10 | battery gauge, its capacity, the charger, no sleep on the cable | no |
| 11 to 13 | satellite fix, texts, a call out and a call in | no |
| 14 to 17 | the card: reading, the safe write, the log, no card | no |
| 18 to 19 | Wi-Fi, the parents' page, the page over the cable | no |
| 20 to 22 | sleep and wake, a restart of the modem, switching off | no |
| 23 to 24 | the motion page, refreshing the panel | no |
| 25 to 28 | voice over LTE: the broken state, the fix, dialling, a cold start | no |

## Decisions that belong to the owner

Each one is in [open-points.md](open-points.md) with the question and a recommendation. Those that stand between this repository and a release:

| Item | Decision | Recommendation |
|---|---|---|
| 9 | whether 4.0 has a radio | leave it out; this release is written that way |
| 13 | Meck's licence file for the copied board description | compare it and add it, or replace the board file |
| 14 | handing out a firmware binary | do not, until the sensor image's terms are verified |
| 16 | the tag, the repository's visibility, what happens to the old repositories | the owner's, after the checklist |

## Going back to 3.2

Flash the 3.2 firmware again and put back the card files saved before the upgrade: the old firmware needs the old files.
