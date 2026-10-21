# Operator label

Code: `src/app/OperatorLabel.{h,cpp}`, tests in `test/test_operator_label/` (the text helper it uses is `src/app/Utf8.{h,cpp}`, tested in `test/test_utf8/`).

The status line shows the mobile operator's name. `oplabel::shortName(name, out, size)` shortens what the network reports:

| Network says | Label | Rule |
|---|---|---|
| `Provider.de` | `Provider` | the name ends at the first blank or dot |
| `F-Bouygues Telecom` | `Bouygues` | a country prefix of one or two letters and a hyphen is removed |
| `T-Mobile`, `E-Plus` | unchanged | the letter is part of the brand, so these two prefixes stay |
| `OPERATOR.DE` | `Operator` | a name written entirely in capitals is capitalised |
| `SFR`, `EE` | unchanged | names of up to three characters are acronyms |
| `Abcdefghijklmnop` | `Abcdefghij` | at most ten characters |
| `Telefónica-Moviles` | `Telefónica` | the cut counts characters and never splits one |
| `  Provider` | `Provider` | leading blanks are dropped |

A null name or one that leaves nothing gives an empty label; the caller decides what to show then. A small buffer cuts on a character boundary as well.

Any prefix is removed for names of the shape `X-` or `XX-` unless it is one of the two brand exceptions; a further brand with a one-letter prefix has to be added to the list in the code.

## UTF-8 helper

`utf8::fit`, `utf8::copy`, `utf8::count` and `utf8::prefixBytes` cut and count UTF-8 text on character boundaries. Fixed-size text fields in several modules (contact names, message texts, labels) go through them: a byte limit inside a character leaves a broken sequence that the display draws as garbage.
