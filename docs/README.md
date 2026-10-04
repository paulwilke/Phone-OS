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

The rules for contributing are in [CONTRIBUTING.md](../CONTRIBUTING.md); third-party material is listed in [THIRD_PARTY.md](../THIRD_PARTY.md).
