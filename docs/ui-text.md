# Fonts and the text engine

Everything the interface writes goes through `src/ui/Text.h`, which draws UTF-8 text with one of four bitmap fonts (`src/ui/Fonts.h`). The same code runs on the device, in the host tests and in the simulator, so what a test measures is what the panel shows.

## The fonts

| `FontSize` | Role | Source | Pixel size | Line height | Capital height |
|---|---|---|---|---|---|
| `Small` | status bar, soft keys | `helvB10.bdf`, bold | 14 | 17 | 11 |
| `Text` | lists, messages | `helvR14.bdf` | 20 | 22 | 14 |
| `Tile` | tiles of the home screen | `helvB18.bdf`, bold | 25 | 29 | 19 |
| `Large` | clock, caller's name | `helvB24.bdf`, bold | 34 | 38 | 25 |

The letters are Adobe's hand-pixelled Helvetica bitmap fonts (100 dpi, from X.org's `font-adobe-100dpi` with Markus Kuhn's ISO 10646 extension, taken from the `tools/font/bdf/` folder of the [u8g2](https://github.com/olikraus/u8g2) project). They are hand-drawn for pixels, which is what a panel without grey levels needs. The interface symbols are our own drawings.

`scripts/make_font.py` turns the sources in `assets/fonts/` into the data files `src/ui/fonts/Font*.h`; the files are checked in so that the firmware build needs no Python. After a change to a source run `python3 scripts/make_font.py --all`; CI runs `--check` and fails when a generated file is stale. The data format is `ui::Font` in `src/ui/Font.h`: packed bitmaps in the layout of `Display::drawBitmap`, a glyph table, and sorted code-point ranges that are searched by bisection.

### What the generator changes

The pixels of the letters are never touched. Only where a glyph sits is corrected, so that the text engine can promise that the width of a string is also the width of its ink:

- A glyph whose ink starts left of the pen (`j`, `Ð`, `î` and `€` do in some sizes, by one or two pixels) is moved to the pen, and a glyph whose ink reaches past its advance (`f` and `Î` in the small font, `©` and `®` in the large one) gets a wider advance.
- The accents of the capital letters reach above the font's ascent; the line grows by the missing rows (one in the small font, two in the text and tile fonts, three in the large one), so that nothing a font draws lies outside its line.

## The character set

`assets/fonts/charset.txt` is the one definition of what the interface can draw: printable Basic Latin (U+0020–U+007E), Latin-1 (U+00A0–U+00FF), `Œ œ Ÿ €`, and the ten interface symbols. The font generator, `scripts/check_i18n.py` (catalogs may use nothing else) and the host tests all read that file. A character is added to it, drawn in the fonts, and only then used in a catalog.

The Helvetica files already contain `Œ œ Ÿ €`; only the symbols are drawn by us, in `assets/fonts/symbols.txt`, one set per font size. Each is a block of `#` and `.` under a `glyph <name> top <rows>` line, where `top` is the number of blank rows between the top of the capitals and the top of the symbol.

| Symbol | Code point | Name in `charset.txt` |
|---|---|---|
| ↵ | U+21B5 | `enter` |
| ⌫ | U+232B | `backspace` |
| ▲ ▼ | U+25B2, U+25BC | `up`, `down` |
| ☎ | U+260E | `telephone` |
| ✉ | U+2709 | `envelope` |
| ✈ | U+2708 | `aeroplane` |
| 💬 | U+1F4AC | `bubble` (speech bubble) |
| 📡 | U+1F4E1 | `radio` |
| 📍 | U+1F4CD | `pin` |

A catalog writes the symbol as that character. A character outside the set — or a byte sequence that is not UTF-8 — is drawn as the font's own box glyph, one per malformed sequence, so the fault is visible and the rest of the text is still there.

## The text engine

All functions take a `ui::Font` (`ui::font(FontSize::Text)`) and UTF-8 text in a `std::string_view`; `y` is the top of the line, not the baseline.

| Function | What it does |
|---|---|
| `decode` | the next code point; malformed input gives U+FFFD and consumes the longest prefix that could still have become valid (the WHATWG rule), so decoding always makes progress |
| `width` | width in pixels: the sum of the advances |
| `fit` | the byte length of the longest prefix, ending between two characters, that fits a width |
| `ellipsize` | the text if it fits; otherwise its start plus `...`, with no space before the dots; empty when not even the dots fit |
| `wrap` | lines of at most a width: breaks at `\n` always, otherwise at spaces, inside a word only when the word alone is wider than a line |
| `draw`, `drawCentered`, `drawRight` | draw the text and return its width |

**Fixed labels are never shortened.** `ellipsize` is for text that comes from the user — names, message bodies, contact labels — and a widget documents where it uses it. A fixed label that does not fit its place is a layout bug, found by the layout tests in every language, and the cure is a shorter word in the catalog or more room, not dots.

**Nothing clips by itself.** `draw` puts the pixels where the text belongs. What runs over the right edge of its region is the caller's mistake, and the frame buffer used in tests reports it as a clip (see [ui-display.md](ui-display.md)). The tests establish the other half of the promise: for every character of the set, in every font, the ink lies inside its cell — from the pen to the pen plus the advance, and inside the line — so text that was measured with `width`, `fit`, `ellipsize` or `wrap` never paints outside the width it measured.

`wrap` returns views into the text it was given, which must outlive them. An empty text is one empty line, like splitting `""` at the newlines; a blank line between paragraphs is kept; spaces at the break are dropped, spaces inside a line are kept. A single character wider than the line gets a line of its own, so wrapping always ends.

## Tests

`test/test_text` checks UTF-8 decoding on every valid length class and the malformed cases (overlong forms, surrogates, values beyond U+10FFFF, truncated sequences, all byte pairs), that every font has exactly the characters of `charset.txt`, the ink-in-cell promise for every glyph, the line metrics, the exact boundaries of `fit`, `ellipsize` and `wrap`, the position of capitals and descenders relative to the line, centred and right-aligned drawing, clip detection through the frame buffer, and golden pictures of Latin-1 text and of the symbols in `test/golden/text_*.txt`.

## Notices

Copyright 1984-1989, 1994 Adobe Systems Incorporated. Copyright 1988, 1994 Digital Equipment Corporation. Helvetica is a trademark of Linotype-Hell AG and/or its subsidiaries.

The fonts' permission notice is in `assets/fonts/LICENSE-Adobe-Helvetica.txt`: use, copying, change and distribution are allowed for any purpose, with the copyright notices and the permission notice kept. Because the fonts here also carry our own symbols, the generated fonts are called `FontSmall`, `FontText`, `FontTile` and `FontLarge`, never Helvetica.
