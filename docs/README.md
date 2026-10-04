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
| [ui-layout-testing.md](ui-layout-testing.md) | testing layouts in every language: `languages::forEach` and the pseudo-language |
| [ui-text.md](ui-text.md) | the four fonts, the character set and the symbols, the text engine: UTF-8, width, fit, ellipsize, wrap, draw |

The rules for contributing are in [CONTRIBUTING.md](../CONTRIBUTING.md); third-party material is listed in [THIRD_PARTY.md](../THIRD_PARTY.md).
