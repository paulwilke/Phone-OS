# TOML subset

Code: `src/app/Toml.{h,cpp}`, tests in `test/test_toml/`. Pure C++17 with no JSON library: the parser produces a plain document tree (`toml::Value`) and the writer turns one back into text. The catalogs of the interface use their own, stricter parser (see [i18n.md](i18n.md)).

The configuration of the device is written in TOML because it is made for people: comments, sections, no commas and braces as in JSON. Only a subset is read, since a complete parser would be bigger than everything it has to read and a variant that never occurs cannot go wrong.

## What is read

| Form | Meaning |
|---|---|
| `# comment` | on its own line or after a value |
| `key = "text"` | basic string; escapes `\"` `\\` `\n` `\t` `\r` `\uXXXX` |
| `key = 'text'` | literal string, no escapes |
| `key = 42`, `-7`, `+7`, `0` | integers (64 bit); no leading zeros |
| `key = true`, `false` | booleans |
| `key = ["a", "b"]` | arrays of plain values, also across lines, a trailing comma is fine |
| `[table]`, `[a.b]` | tables, nested |
| `[[list]]`, `[[a.b]]` | lists of tables; a later `[list.sub]` belongs to the last element |
| `"quoted key" = 1` | keys with other characters in basic or literal quotes |

A byte order mark at the start of the file is skipped. Lines may end in `\n` or `\r\n`; the last line needs no line break. A table may be defined after one of its own sub-tables (`[a.b]` then `[a]`), once.

## What is refused

Every problem gives a `toml::Code` and the 1-based line; `codeName()` is a stable identifier for logs and catalog keys (the interface words it). The whole document is rejected, and the output is an empty table, so a half-read configuration can never be used.

| Code | When |
|---|---|
| `ExpectedKey`, `ExpectedEquals`, `ExpectedValue` | a missing key, `=` or value |
| `DottedKey` | `a.b = 1`: use a section |
| `UnreadableValue` | floats, dates, `007`, `1_000`, numbers beyond 64 bit, `TRUE` |
| `UnterminatedString`, `MultilineString` | a string that ends at the line, or `"""`/`'''` |
| `UnknownEscape` | a backslash escape other than the six above (`\b`, `\f`, `\U` are not read) |
| `InvalidUnicode` | `\u` without exactly four hex digits (a sign or blank is not accepted), `\u0000` (would end a C string early), or a surrogate half |
| `ControlCharacter` | a raw control character other than tab inside a string |
| `InlineTable`, `NestedArray` | `{ }`, an array inside an array |
| `ExpectedCommaOrEnd`, `UnterminatedArray` | problems inside `[ ]` |
| `UnterminatedHeader` | `[table` or `[[list]` |
| `DuplicateKey`, `DuplicateTable` | a key twice in a table, a `[table]` written twice |
| `NameClash` | one name used as a value and as a table, or as an array and a list of tables, or as a table and a list |
| `TooDeep` | more than `kMaxDepth` (8) levels. The writer recurses along the nesting and a file must not be able to exhaust the stack |
| `TrailingGarbage` | anything after a value or header on the same line |

The parser keeps the whole document in memory: whoever hands over a file limits its size first.

## The document tree

`Value` is a string, integer, boolean, array of plain values, table (named entries in the order they were written) or list of tables. `set()` and `push()` return null instead of misbehaving when a value does not fit (a table into an array, an entry into a list).

## Writing

`emit()` writes a table as TOML: first the plain values and arrays, then the sub-tables, then the lists of tables, each group in the order of its entries. Comments are lost on a save from the web page; the structure and the values are not. Keys that are not made of letters, digits, `_` and `-` are quoted. Control characters and DEL in strings are written as `\uXXXX`.

What has no spelling: an **empty list of tables** is not written (an empty table or array is), and a string holding a zero byte is written as `\u0000`, which the parser refuses. A table nested deeper than `kMaxDepth` is not written. Because the groups are written one after the other, the order of entries of different kinds can change on a round trip; the content does not.
