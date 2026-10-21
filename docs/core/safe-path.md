# Safe paths

Code: `src/app/SafePath.{h,cpp}`, tests in `test/test_safe_path/`.

A path that arrives from outside (the parents' web page, the USB console) must not leave the SD card and must not address anything odd. The check is in one place, used by every storage implementation, instead of each one remembering `..`, `//` and control characters for itself.

`safepath::check(path)` returns `Ok` or the reason; `valid()` is `check() == Ok`.

| Rule | Reason code |
|---|---|
| absolute: starts with `/` (null and empty are refused) | `NotAbsolute` |
| at most 64 bytes in all | `TooLong` |
| no empty name: `//` is refused; only the end may be empty (`/dir/`) | `EmptySegment` |
| no name of dots only: `.`, `..`, `...` | `DotSegment` |
| letters, digits and `. _ -` only. No blank, backslash, colon, percent sign, control character or non-ASCII byte | `BadCharacter` |
| each name at most 40 bytes | `SegmentTooLong` |
| the last name does not end in `.tmp` | `Reserved` |

The root `/` is valid. Dots inside a name are fine (`/a..b`, `/.hidden`).

**Case.** The card is FAT, which does not tell upper from lower case, so every comparison ignores case: `x.TMP` is as reserved as `x.tmp`, and `/CONFIG/x` is in the configuration directory for `inConfigDir()` just as it is for the card. A check that compares exactly could be walked around by changing the case of a letter.

`.tmp` (`kTempSuffix`) is the name of the scratch file a safe write goes through; it belongs to the storage code, not to whoever sends a path. `inConfigDir(path)` is true below `/config/`; `endsWith(s, suffix)` compares file endings; `sameFile(a, b)` says whether two paths name the same file. Every rule about one file (the secrets file cannot be deleted, a configuration file is validated before it is written) goes through `sameFile`, never through an exact comparison: `DELETE /api/file?path=/CONFIG/SECRETS.TOML` once passed the exact one (found 2026-10-13, `test_web_api`). What the card does with the short names of FAT (`SECRET~1.TOM`) is not known and not verified on the device; the path rules refuse `~`, so such a name cannot be sent.
