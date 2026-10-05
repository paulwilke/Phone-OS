# Documentation

The documents describe what exists in this repository — each one is written from the code it covers and changes with it.

| Document | Covers |
|---|---|
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
| [ui-framework.md](ui-framework.md) | input model, screen stack and its enter/leave rules, the widgets and what each shortens |
| [ui-screens.md](ui-screens.md) | what a screen may know, the frame every screen shares (status bar, notice line, soft keys), the wording of the core's codes |
| [screen-phone.md](screen-phone.md) | the phone page: who is on it, what choosing does, the core's refusals |
| [screen-call.md](screen-call.md) | the call page: dialling, ringing, connected, volume, why a call failed |
| [screen-number.md](screen-number.md) | the typed number for a message: limits, canonical form, the profile switch |
| [ui-layout-testing.md](ui-layout-testing.md) | testing layouts in every language: `languages::forEach` and the pseudo-language |
| [ui-refresh.md](ui-refresh.md) | refresh policy, frame tracker, refresh controller and the frame-budget tests |
| [ui-text.md](ui-text.md) | the four fonts, the character set and the symbols, the text engine: UTF-8, width, fit, ellipsize, wrap, draw |

The rules for contributing are in [CONTRIBUTING.md](../CONTRIBUTING.md); third-party material is listed in [THIRD_PARTY.md](../THIRD_PARTY.md).
