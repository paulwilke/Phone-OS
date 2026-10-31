# Documentation

The documents describe what exists in this repository; each one is written from the code it covers and changes with it. This page is the index: it links every document, once.

**Reading it on GitHub.** Open the folder `docs/` of the repository on github.com: GitHub shows the list of files and folders and, below it, this page. Every link here opens the document, rendered, in the browser; the links inside the documents work the same way, and the browser's Back button returns. A folder opened on GitHub lists its documents; this page says what each one is for.

## Start here

| Document | What it is for |
|---|---|
| [decisions.md](decisions.md) | the running log of decisions: what was chosen or refused, by whom, why, and how it is checked; the "Verified" line of each says what was seen on the device and when |
| [open-points.md](open-points.md) | the one list of open work, with the owner of every item |
| [security.md](security.md) | what the device does against strangers and, plainly, what it does not protect against: forged sender numbers, the position query, the card in plain text, the USB console, the web page, the chip and modem, the network |
| [CHANGELOG.md](../CHANGELOG.md) | what changed in each version for the people who use the device |
| [release-4.2.md](release-4.2.md) | the current release notes: what 4.2 changed for the parents and the child, what was checked, what the owner tries on the device |

## Manuals

For the parents and the owner of the device. The three guides have the same headings.

| Document | What it is for |
|---|---|
| [user-guide.md](user-guide.md) | the guide for the parents: the device, school time, SOS, the parents' page, settings, battery, what to do without network or SIM or with a blank screen |
| [de/benutzerhandbuch.md](de/benutzerhandbuch.md) | the same guide in German, with the same headings |
| [fr/guide-utilisateur.md](fr/guide-utilisateur.md) | the same guide in French, with the same headings |
| [de/abnahmetest.md](de/abnahmetest.md) | the acceptance test the owner goes through on the device after every update, in German, with a column for the date and the result |
| [upgrading.md](upgrading.md) | from a device that runs 3.2: what to save, what to look at in the files, how to flash, what happens at the first start, how to go back |

## Earlier release notes

| Document | What it is for |
|---|---|
| [release-4.1.md](release-4.1.md) | what release 4.1 changed for the parents and the child, what was checked, what the owner tries on the device |
| [release-4.0.md](release-4.0.md) | what release 4.0 is and is not, what was checked and where, the owner's checklist state, the decisions that belong to the owner |

## The board: its chips, their drivers, the measurements on the device and the bring-up (`hardware/`)

| Document | What it is for |
|---|---|
| [hardware/device.md](hardware/device.md) | the firmware's platform code: where what lives, the pin map, the seams and their fakes, the start-up order, how to build |
| [hardware/hardware-capabilities.md](hardware/hardware-capabilities.md) | the interfaces to the hardware (capabilities), their contracts, vibration patterns |
| [hardware/hardware-drivers.md](hardware/hardware-drivers.md) | every driver: what it does, from what, what is tested on the host, what is not verified; the rules of the lessons and their tests |
| [hardware/hardware-lessons.md](hardware/hardware-lessons.md) | what went wrong on the real device, why, and the rule that prevents it (fuel gauge capacity, IMS context, charger, USB) |
| [hardware/bring-up.md](hardware/bring-up.md) | the owner's checklist for the first start, with empty date columns |
| [hardware/device-console.md](hardware/device-console.md) | the firmware's wiring and loop, the USB console and the two host tools |
| [hardware/device-display.md](hardware/device-display.md) | the e-ink pipeline (frame buffer, dirty rectangle, refresh sequences) and what is not built: the panel's own values |
| [hardware/device-input.md](hardware/device-input.md) | keyboard, touch and the three keys under the screen, vibration and the motion sensor on the board: the decoding, the layout, what is host-tested |
| [hardware/device-gnss-modem-clock.md](hardware/device-gnss-modem-clock.md) | the NMEA reader and its rules, the receiver and the modem's lines on the board, the clock and which time to believe |
| [hardware/device-storage-network.md](hardware/device-storage-network.md) | the card (mounting, the safe write, power loss, the logs), Wi-Fi, the TCP server and the page in the firmware |
| [hardware/device-power.md](hardware/device-power.md) | the Power capability on the board: rails, the fuel gauge capacity check and its end-of-discharge thresholds, the plausibility rule, the charger revisions, the diagnosis |
| [hardware/battery.md](hardware/battery.md) | what the battery numbers mean, when the gauge is not believed, and what is known and not known about deep discharge |
| [hardware/fuel-gauge-data-memory.md](hardware/fuel-gauge-data-memory.md) | the fuel gauge's data memory: the end-of-discharge items the firmware writes, where they come from in the manual, the write sequence, what is not known |
| [hardware/energy-log.md](hardware/energy-log.md) | the battery readings every ten minutes in /energy.log, their format, rotation and what to read from them |

## The modem: its driver, the AT layer, calls over LTE, the APN list and the text format (`modem/`)

| Document | What it is for |
|---|---|
| [modem/modem-driver.md](modem/modem-driver.md) | the modem driver: start-up, SIM and PIN, the data bearer and the IMS, registration, restarts |
| [modem/at-layer.md](modem/at-layer.md) | the modem driver's AT layer: the transport seam, lines, the answers the driver reads, the SMS answers, UCS2, the scripted modem |
| [modem/volte.md](modem/volte.md) | voice over LTE: what the measurements prove, what the driver does about it, what is not proven or not built, and the owner's checklist on the device |
| [modem/apn-table.md](modem/apn-table.md) | which APN a SIM gets: the operator list from the Android project, the lookup by the SIM's operator code, and what happens for an unknown operator |
| [modem/sms-pdu.md](modem/sms-pdu.md) | the PDU of an outgoing SMS: 7-bit with umlauts, UCS2, one concatenated message for a long text |

## The application core and the logic modules it is built from (`core/`)

A document marked "top" lives at the top of `docs/`, not in the folder, because a script or a test reads it by that path.

| Document | What it is for |
|---|---|
| [core/app-core.md](core/app-core.md) | the facade: the host's calls, the order of one pass, the read-only state, the settings that change at run time |
| [core/runtime.md](core/runtime.md) | the one composition of core, screens and web service that the firmware and the simulator both run: boot, one pass, calls, input, language |
| [app-behaviour.md](app-behaviour.md) | the inventory of the application core's behaviour: every behaviour, its owner and its test, and every test of the previous code base (top) |
| [core/profile.md](core/profile.md) | the settings the behaviour depends on, as one plain struct; who may be called, whom SOS reaches; the core's log |
| [core/config-loader.md](core/config-loader.md) | loading the three config files into a Profile with diagnostics; writing them back; user catalogs; the settings-changed flag |
| [core/toml.md](core/toml.md) | the TOML subset of the configuration files: parser, writer, errors |
| [core/safe-path.md](core/safe-path.md) | validation of file paths that arrive from outside |
| [core/contacts.md](core/contacts.md) | phone number normalisation, the whitelist and its file format |
| [quiet-hours.md](quiet-hours.md) | school-time windows, midnight crossing, behaviour without a clock (top) |
| [core/school-time.md](core/school-time.md) | school time on the clock and the emergency lift |
| [core/time-zones.md](core/time-zones.md) | named time zones, summer-time rules, calendar arithmetic |
| [core/calls.md](core/calls.md) | dialling rules, every call rejected in school time, ringing, why a call failed, volume |
| [core/messaging.md](core/messaging.md) | messages in and out: remote queries first, cellular else radio else the queue, delivery reports, send failures |
| [core/message-store.md](core/message-store.md) | the message history, its append-only log format, restoring and deleting |
| [core/sms-command.md](core/sms-command.md) | the remote location query by SMS: trigger words, admin check, outcomes |
| [core/emergency.md](core/emergency.md) | the SOS button and the reply to a location query as one search at a time; the receiver, school time, the outcome, the wording |
| [core/sos-flow.md](core/sos-flow.md) | the escalation in front of the ping: choice, 40-second countdown, five taps in school time |
| [core/sos-ping.md](core/sos-ping.md) | the position-sending state machine: best fix, waiting, timeout, message wording as input |
| [core/positioning.md](core/positioning.md) | the reasons the satellite receiver is on, the start-up fix, the last known fix |
| [core/map-link.md](core/map-link.md) | the map address in position messages, built from a template |
| [core/shake-detector.md](core/shake-detector.md) | recognising shaking from acceleration samples |
| [core/connectivity.md](core/connectivity.md) | cellular, the radio request, Wi-Fi with its rules, the network label as data |
| [core/operator-label.md](core/operator-label.md) | the short operator name for the status line; the UTF-8 text helper |
| [core/power-policy.md](core/power-policy.md) | what may sleep when: the eight rules and their order |
| [core/power-coordination.md](core/power-coordination.md) | the power policy applied to modem, radio chip and processor; the front light |
| [core/notices.md](core/notices.md) | notices and system entries as codes: the kinds, their catalog keys, the stored form |
| [core/log-level.md](core/log-level.md) | which log lines are written on which level, and which are never dropped |
| [core/i18n.md](core/i18n.md) | translation library, catalog format, plural rules, user catalogs |

## The interface: the framework, the text and the refresh, and one document per screen (`ui/`)

| Document | What it is for |
|---|---|
| [ui/ui-framework.md](ui/ui-framework.md) | input model, screen stack and its enter/leave rules, the widgets and what each shortens |
| [ui/ui-screens.md](ui/ui-screens.md) | what a screen may know, the frame every screen shares (status bar, notice line, soft keys), the wording of the core's codes |
| [ui/ui-display.md](ui/ui-display.md) | the 1-bit display interface, the frame buffer with clip detection, golden files |
| [ui/ui-text.md](ui/ui-text.md) | the four fonts, the character set and the symbols, the text engine: UTF-8, width, fit, ellipsize, wrap, draw |
| [ui/ui-refresh.md](ui/ui-refresh.md) | refresh policy, frame tracker, refresh controller and the frame-budget tests |
| [ui/ui-layout-testing.md](ui/ui-layout-testing.md) | testing layouts in every language: `languages::forEach` and the pseudo-language |
| [ui/screen-call.md](ui/screen-call.md) | the call page: dialling, ringing, connected, volume, why a call failed |
| [ui/screen-clock.md](ui/screen-clock.md) | the clock page: time, weekday and date from the wall clock, no time |
| [ui/screen-compose.md](ui/screen-compose.md) | writing a message: the character set and limits, ready-made messages, sending |
| [ui/screen-gnss.md](ui/screen-gnss.md) | the position page: the receiver held while it is open, four values, the last known position with its age |
| [ui/screen-home.md](ui/screen-home.md) | the home screen: which tiles exist for which profile, their order, what each does, the badge, school time |
| [ui/screen-language.md](ui/screen-language.md) | the language page: native names, choosing at once through the core |
| [ui/screen-lock.md](ui/screen-lock.md) | the lock: two presses of Enter, no clock, what it says instead |
| [ui/screen-messages.md](ui/screen-messages.md) | the history: lists, marks, words from codes, reading, reply and call back, delete |
| [ui/screen-motion.md](ui/screen-motion.md) | the motion page: the sensor started after the first frame, the pinned ball mapping, shakes |
| [ui/screen-net.md](ui/screen-net.md) | the network page: the lines, SIM problems and their explanations |
| [ui/screen-number.md](ui/screen-number.md) | the typed number for a message: limits, canonical form, the profile switch |
| [ui/screen-phone.md](ui/screen-phone.md) | the phone page: who is on it, what choosing does, the core's refusals |
| [ui/screen-power.md](ui/screen-power.md) | the power page: switching off after a question |
| [ui/screen-radio.md](ui/screen-radio.md) | the radio page: head line, controls per state, contacts with way and age, writing to one |
| [ui/screen-recipient.md](ui/screen-recipient.md) | whom to write to, and when a new number is possible |
| [ui/screen-selftest.md](ui/screen-selftest.md) | the self-test page: eight checks, the key test without a soft key, the receiver given back |
| [ui/screen-settings.md](ui/screen-settings.md) | the options page: volume, message vibration, Wi-Fi, language |
| [ui/screen-sos.md](ui/screen-sos.md) | the SOS page: choice, school-time taps, the ping step by step, leaving releases the search |
| [ui/screen-status.md](ui/screen-status.md) | the status page: battery, configuration problems, queries, firmware, switching off |
| [ui/screen-wifi.md](ui/screen-wifi.md) | the Wi-Fi page: three buttons, the mark, what to join with, why it did not work |

## The parents' page, its API, the configuration keys and the Wi-Fi (`web/`)

A document marked "top" lives at the top of `docs/`, not in the folder, because a script or a test reads it by that path.

| Document | What it is for |
|---|---|
| [web/web-interface.md](web/web-interface.md) | the parents' page and its API: the HTTP layer, the routes, the security model, the page, the service |
| [configuration-reference.md](configuration-reference.md) | every configuration key, generated from the schema: type, default, limits, meaning (top) |
| [web/wifi-controller.md](web/wifi-controller.md) | Wi-Fi settings and modes, the access point password, the idle switch-off |

## Testing: the screens automatically, and the device by hand (`testing/`)

| Document | What it is for |
|---|---|
| [testing/ui-testing.md](testing/ui-testing.md) | the concept of automated UI tests: what a scenario, an expectation and a golden frame are, what `sim/scripts` can do and what is missing, how a test fails, how it runs in CI, the three languages, what stays out, the owner's decisions |
| [testing/ui-testing-framework-requirements.md](testing/ui-testing-framework-requirements.md) | the numbered requirements for the UI testing framework built in its own repository: scenario format, the interface to the simulator, outputs, golden frames, the configuration file `ui-tests/ui-tests.toml`, performance, portability, licensing, non-goals |
| [testing/device-check.md](testing/device-check.md) | the guide the owner walks through at the device in one go: preparation, the steps on the desk, the log read at the cable, the optional discharge test; each step with its purpose, hand steps, expected result and the screen or log line that confirms it |

## Working without the device: the simulator, the test doubles, the demo, translating (`process/`)

A document marked "top" lives at the top of `docs/`, not in the folder, because a script or a test reads it by that path.

| Document | What it is for |
|---|---|
| [simulator.md](simulator.md) | the application on the fakes with a console: build, run, the card, commands, scripts and assertions, the web page, what it cannot prove (top) |
| [process/simulated-panel.md](process/simulated-panel.md) | the e-ink panel without a window: the real refresh policy, the visible delay, a PNG of what is shown |
| [process/test-doubles.md](process/test-doubles.md) | the scriptable fake of every capability and what each can be made to do |
| [process/demo.md](process/demo.md) | the generated pictures and the click-through demo: the walk, the scenes, the page, what the generator checks |
| [process/site.md](process/site.md) | the project's web page on GitHub Pages: the click-through demo as its first screen, how it is assembled and published, the rules it keeps |
| [translating.md](translating.md) | writing a catalog, the size limit, trying it in the simulator (top) |

What is not verified on the device is stated where the code is described, and what has to be done about it, with the person who has to do it, is only in [open-points.md](open-points.md).

The rules for contributing are in [CONTRIBUTING.md](../CONTRIBUTING.md); third-party material is listed in [THIRD_PARTY.md](../THIRD_PARTY.md).
