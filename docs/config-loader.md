# Configuration loader and writer

`ConfigLoader` reads the three configuration files from the SD card into a `Profile`.
`ConfigWriter` writes a profile back to the card atomically.

## Files on the card

| File | Contains |
|---|---|
| `/config/settings.toml` | device behaviour: child name, features, school times, call targets, radio, timezone, log level, Wi-Fi idle time |
| `/config/contacts.toml` | who may call and write, and which contacts are parents |
| `/config/secrets.toml` | SIM PIN, web password, Wi-Fi access point and home networks |

The layout of each file is described in [docs/configuration-reference.md](configuration-reference.md).

## Loading

```cpp
ConfigLoader loader(store);
ConfigLoader::Result r = loader.load();
// r.profile        — all device settings
// r.simPin         — secret; not stored in Profile
// r.webPassword    — secret; not stored in Profile
// r.diagnostics    — problems found while loading
```

A missing file means defaults and one `MissingFile` diagnostic for that file. A file that
is present but broken still gives defaults for that file; the other files load normally.

### Size limit

Files larger than `ConfigLoader::kMaxFileBytes` (4096 bytes) are skipped with an
`OversizedFile` diagnostic. This matches the limit the web interface enforces on incoming
files (rule 6 of the security rules in the brief).

### Previous firmware layout

The loader accepts `[mesh]` as an alias for `[radio]` so that a card from a device running
the previous firmware loads without a manual edit. A `MeshAlias` diagnostic is added when
the alias is used; `[radio]` is used on every write.

### Non-call tiles

`[[tiles]]` entries with `action` other than `call` are accepted but ignored with a
`NonCallTile` diagnostic. The home screen currently supports call targets only; other
actions from the previous firmware (compose, light, status, sos, messages) are not ported.

## Diagnostics

Each diagnostic has a `code` and a `args` vector. The host logs them in English; the web
status page words them from the catalogs in PR 3.

| Code | args | Meaning |
|---|---|---|
| `MissingFile` | file | File not on card; defaults apply. |
| `OversizedFile` | file | File larger than 4 KB; defaults apply for that file. |
| `SyntaxError` | file, line, error-code | TOML parse error; the line number and error code name identify the problem. |
| `UnknownKey` | file, key-path | A key not in the schema (likely a typo). |
| `WrongType` | file, key-path, expected | A value of the wrong TOML type. |
| `OutOfRange` | file, key-path | An integer or string length outside the allowed range. |
| `BadEnum` | file, key-path, value | A string not in the allowed set. |
| `BadCatalogCode` | file, key-path, code | The interface or SMS language is a code no catalog has (`ConfigLoader::checkLanguages`, called by the host once shipped and user catalogs are together; the interface then stays in English). |
| `ContactNoNumber` | entry-index | A `[[contact]]` entry without a `number` field. Skipped. |
| `UnknownRole` | role | A contact role other than `parent` or `family`. Treated as `family`. |
| `BadQuietHours` | result-name | A `[[quiet_hours]]` entry refused by `QuietHours::add`. |
| `NonCallTile` | action | A `[[tiles]]` entry with an action other than `call`. Ignored. |
| `TileNoTarget` | — | A `[[tiles]]` `call` entry without a `target`. Skipped. |
| `MeshAlias` | — | `[mesh]` section accepted as `[radio]` alias. |
| `UserCatalogError` | file, first-error | A user catalog from `/lang/` was rejected. |

## User catalogs

```cpp
std::vector<config::Diagnostic> diags;
ConfigLoader::loadUserCatalogs(store, translator, diags);
```

`loadUserCatalogs` lists `/lang/` on the card and calls `Translator::addUser()` for each
`.toml` file found there. A catalog that fails validation is skipped with a
`UserCatalogError` diagnostic; the rest are loaded. Call this before `load()` so that
the language codes the config sets can be selected from the Translator afterwards.

`ConfigLoader::checkLanguages(profile, translator, diags)` then compares the profile's interface and SMS
language with the catalogs that are installed, shipped and user's together, and adds a `BadCatalogCode`
diagnostic for each code nobody has. It returns whether the interface language is installed; the host selects
it only then ([runtime.md](runtime.md)). The loader cannot do this itself: it does not know the catalogs.

`loadUserWebCatalogs(store, translator, diags)` does the same for `/lang/web/*.toml` with `Translator::addUserWeb()`, under the same limit for each file. It is not called at start: the runtime gives it to the translator as the source of the page's user catalogs, so it runs when the page's catalogs are loaded, and a rejected file is logged in the same words ([i18n.md](i18n.md), [runtime.md](runtime.md)).

## Writing

```cpp
ConfigWriter writer(store);
writer.writeSettings(profile);
writer.writeContacts(profile);
writer.writeSecrets(profile, simPin, webPassword);
```

Each method renders a fresh TOML file from the given data and writes it atomically through
the store. A failed write (full or missing card) returns `false` and leaves the old file
intact.

### Settings that change at run time

The child may change two settings on the device without a restart: message vibration and
remote-query visibility. `AppCore::setMessageVibration()` and `AppCore::setCommandNotice()`
set a `settingsNeedWrite()` flag. The host checks this flag on each main-loop pass and, when
it is set, calls `ConfigWriter::writeSettings(core.profile())` then `core.clearSettingsWrite()`.

```cpp
if (core.settingsNeedWrite()) {
    writer.writeSettings(core.profile());
    core.clearSettingsWrite();
}
```
