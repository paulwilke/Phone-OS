# Documentation

The documents describe what exists in this repository — each one is written from the code it covers and changes with it.

| Document | Covers |
|---|---|
| [user-guide.md](user-guide.md) | the guide for the parents: the device, school time, SOS, the parents' page, settings, battery, what to do without network or SIM or with a blank screen |
| [de/benutzerhandbuch.md](de/benutzerhandbuch.md) | the same guide in German, with the same headings |
| [fr/guide-utilisateur.md](fr/guide-utilisateur.md) | the same guide in French, with the same headings |
| [open-points.md](open-points.md) | the one list of open work, with the owner of every item |
| [i18n.md](i18n.md) | translation library, catalog format, plural rules, user catalogs |
| [contacts.md](contacts.md) | phone number normalisation, the whitelist and its file format |
| [quiet-hours.md](quiet-hours.md) | school-time windows, midnight crossing, behaviour without a clock |
| [time-zones.md](time-zones.md) | named time zones, summer-time rules, calendar arithmetic |
| [log-level.md](log-level.md) | which log lines are written on which level, and which are never dropped |
| [map-link.md](map-link.md) | the map address in position messages, built from a template |
| [shake-detector.md](shake-detector.md) | recognising shaking from acceleration samples |
| [message-store.md](message-store.md) | the message history, its append-only log format, restoring and deleting |
| [toml.md](toml.md) | the TOML subset of the configuration files: parser, writer, errors |
| [safe-path.md](safe-path.md) | validation of file paths that arrive from outside |
| [operator-label.md](operator-label.md) | the short operator name for the status line; the UTF-8 text helper |
| [sos-ping.md](sos-ping.md) | the position-sending state machine: best fix, waiting, timeout, message wording as input |
| [sos-flow.md](sos-flow.md) | the escalation in front of the ping: choice, 40-second countdown, five taps in school time |
| [sms-command.md](sms-command.md) | the remote location query by SMS: trigger words, admin check, outcomes |
| [power-policy.md](power-policy.md) | what may sleep when: the eight rules and their order |
| [hardware-capabilities.md](hardware-capabilities.md) | the interfaces to the hardware (capabilities), their contracts, vibration patterns |
| [device.md](device.md) | the firmware's platform code: where what lives, the pin map, the seams and their fakes, the start-up order, how to build |
| [device-gnss-modem-clock.md](device-gnss-modem-clock.md) | the NMEA reader and its rules, the receiver and the modem's lines on the board, the clock and which time to believe |
| [device-storage-network.md](device-storage-network.md) | the card (mounting, the safe write, power loss, the logs), Wi-Fi, the TCP server and the page in the firmware |
| [hardware-drivers.md](hardware-drivers.md) | every driver: what it does, from what, what is tested on the host, what is not verified; the rules of the lessons and their tests |
| [release-4.0.md](release-4.0.md) | what release 4.0 is and is not, what was checked and where, the owner's checklist state, the decisions that belong to the owner |
| [upgrading.md](upgrading.md) | from a device that runs 3.2: what to save, what to look at in the files, how to flash, what happens at the first start, how to go back |
| [bring-up.md](bring-up.md) | the owner's checklist for the first start, with empty date columns |
| [device-console.md](device-console.md) | the firmware's wiring and loop, the USB console and the two host tools |
| [device-display.md](device-display.md) | the e-ink pipeline (frame buffer, dirty rectangle, refresh sequences) and what is not built: the panel's own values |
| [device-input.md](device-input.md) | keyboard, touch and the three keys under the screen, vibration and the motion sensor on the board: the decoding, the layout, what is host-tested |
| [device-power.md](device-power.md) | the Power capability on the board: rails, the fuel gauge capacity check, the plausibility rule, the charger revisions, the diagnosis |
| [hardware-lessons.md](hardware-lessons.md) | what went wrong on the real device, why, and the rule that prevents it (fuel gauge capacity, IMS context, charger, USB) |
| [modem-driver.md](modem-driver.md) | the modem driver: start-up, SIM and PIN, the data bearer and the IMS, registration, restarts |
| [volte.md](volte.md) | voice over LTE: what the measurements prove, what the driver does about it, what is not proven or not built, and the owner's checklist on the device |
| [at-layer.md](at-layer.md) | the modem driver's AT layer: the transport seam, lines, the answers the driver reads, the SMS answers, UCS2, the scripted modem |
| [test-doubles.md](test-doubles.md) | the scriptable fake of every capability and what each can be made to do |
| [ui-display.md](ui-display.md) | the 1-bit display interface, the frame buffer with clip detection, golden files |
| [app-behaviour.md](app-behaviour.md) | the inventory of the application core's behaviour: every behaviour, its owner and its test, and every test of the previous code base |
| [profile.md](profile.md) | the settings the behaviour depends on, as one plain struct; who may be called, whom SOS reaches; the core's log |
| [positioning.md](positioning.md) | the reasons the satellite receiver is on, the start-up fix, the last known fix |
| [messaging.md](messaging.md) | messages in and out: remote queries first, cellular else radio else the queue, delivery reports, send failures |
| [school-time.md](school-time.md) | school time on the clock and the emergency lift |
| [calls.md](calls.md) | dialling rules, every call rejected in school time, ringing, why a call failed, volume |
| [emergency.md](emergency.md) | the SOS button and the reply to a location query as one search at a time; the receiver, school time, the outcome, the wording |
| [connectivity.md](connectivity.md) | cellular, save mode and the cell probe, the radio request, Wi-Fi with its rules, the network label as data |
| [wifi-controller.md](wifi-controller.md) | Wi-Fi settings and modes, the access point password, the idle switch-off |
| [power-coordination.md](power-coordination.md) | the power policy applied to modem, radio chip and processor; the front light |
| [app-core.md](app-core.md) | the facade: the host's calls, the order of one pass, the read-only state, the settings that change at run time |
| [notices.md](notices.md) | notices and system entries as codes: the kinds, their catalog keys, the stored form |
| [screen-sos.md](screen-sos.md) | the SOS page: choice, school-time taps, the ping step by step, leaving releases the search |
| [screen-settings.md](screen-settings.md) | the options page: volume, message vibration, save mode, Wi-Fi, language |
| [screen-wifi.md](screen-wifi.md) | the Wi-Fi page: three buttons, the mark, what to join with, why it did not work |
| [screen-net.md](screen-net.md) | the network page: the lines, SIM problems and their explanations |
| [screen-status.md](screen-status.md) | the status page: battery, configuration problems, queries, firmware, switching off |
| [screen-language.md](screen-language.md) | the language page: native names, choosing at once through the core |
| [screen-gnss.md](screen-gnss.md) | the position page: the receiver held while it is open, four values, the last known position with its age |
| [screen-radio.md](screen-radio.md) | the radio page: head line, controls per state, contacts with way and age, writing to one |
| [screen-motion.md](screen-motion.md) | the motion page: the sensor started after the first frame, the pinned ball mapping, shakes |
| [screen-selftest.md](screen-selftest.md) | the self-test page: eight checks, the key test without a soft key, the receiver given back |
| [ui-framework.md](ui-framework.md) | input model, screen stack and its enter/leave rules, the widgets and what each shortens |
| [ui-screens.md](ui-screens.md) | what a screen may know, the frame every screen shares (status bar, notice line, soft keys), the wording of the core's codes |
| [screen-home.md](screen-home.md) | the home screen: which tiles exist for which profile, their order, what each does, the badge, school time |
| [screen-lock.md](screen-lock.md) | the lock: two presses of Enter, no clock, what it says instead |
| [screen-clock.md](screen-clock.md) | the clock page: time, weekday and date from the wall clock, no time |
| [screen-phone.md](screen-phone.md) | the phone page: who is on it, what choosing does, the core's refusals |
| [screen-call.md](screen-call.md) | the call page: dialling, ringing, connected, volume, why a call failed |
| [screen-number.md](screen-number.md) | the typed number for a message: limits, canonical form, the profile switch |
| [screen-messages.md](screen-messages.md) | the history: lists, marks, words from codes, reading, reply and call back, delete |
| [screen-compose.md](screen-compose.md) | writing a message: the character set and limits, ready-made messages, sending |
| [screen-recipient.md](screen-recipient.md) | whom to write to, and when a new number is possible |
| [ui-layout-testing.md](ui-layout-testing.md) | testing layouts in every language: `languages::forEach` and the pseudo-language |
| [ui-refresh.md](ui-refresh.md) | refresh policy, frame tracker, refresh controller and the frame-budget tests |
| [ui-text.md](ui-text.md) | the four fonts, the character set and the symbols, the text engine: UTF-8, width, fit, ellipsize, wrap, draw |
| [runtime.md](runtime.md) | the one composition of core, screens and web service that the firmware and the simulator both run: boot, one pass, calls, input, language |
| [simulator.md](simulator.md) | the application on the fakes with a console: build, run, the card, commands, scripts and assertions, the web page, what it cannot prove |
| [demo.md](demo.md) | the generated pictures and the click-through demo: the walk, the scenes, the page, what the generator checks |
| [translating.md](translating.md) | writing a catalog, the size limit, trying it in the simulator |
| [simulated-panel.md](simulated-panel.md) | the e-ink panel without a window: the real refresh policy, the visible delay, a PNG of what is shown |
| [configuration-reference.md](configuration-reference.md) | every configuration key, generated from the schema: type, default, limits, meaning |
| [web-interface.md](web-interface.md) | the parents' page and its API: the HTTP layer, the routes, the security model, the page, the service |
| [config-loader.md](config-loader.md) | loading the three config files into a Profile with diagnostics; writing them back; user catalogs; the settings-changed flag |

What is not verified on the device is stated where the code is described, and what has to be done about it, with the person who has to do it, is only in [open-points.md](open-points.md).

The rules for contributing are in [CONTRIBUTING.md](../CONTRIBUTING.md); third-party material is listed in [THIRD_PARTY.md](../THIRD_PARTY.md).
