# Fonts and the text engine

Everything the interface writes goes through `src/ui/Text.h`, which draws UTF-8 text with one of four bitmap fonts (`src/ui/Fonts.h`). The same code runs on the device, in the host tests and in the simulator, so what a test measures is what the panel shows.

## The fonts

| `FontSize` | Role | Source | Pixel size | Line height | Capital height |
|---|---|---|---|---|---|
| `Small` | status bar, soft keys | `luBS10.bdf`, bold | 14 | 16 | 10 |
| `Text` | lists, messages | `luRS14.bdf` | 20 | 23 | 14 |
| `Tile` | tiles of the home screen | `luBS18.bdf`, bold | 25 | 29 | 18 |
| `Large` | clock, caller's name | `luBS24.bdf`, bold | 34 | 41 | 25 |

The letters are the hand-pixelled Lucida Sans bitmap fonts by Bigelow & Holmes (100 dpi, taken from the `tools/font/bdf/` folder of the [u8g2](https://github.com/olikraus/u8g2) project). They are hand-drawn for pixels, which is what a panel without grey levels needs. The interface symbols are our own drawings.

`scripts/make_font.py` turns the sources in `assets/fonts/` into the data files `src/ui/fonts/Font*.h`; the files are checked in so that the firmware build needs no Python. After a change to a source run `python3 scripts/make_font.py --all`; CI runs `--check` and fails when a generated file is stale. The data format is `ui::Font` in `src/ui/Font.h`: packed bitmaps in the layout of `Display::drawBitmap`, a glyph table, and sorted code-point ranges that are searched by bisection.

### What the generator changes

The pixels of the Lucida letters are never touched. Only where a glyph sits is corrected, so that the text engine can promise that the width of a string is also the width of its ink:

- A glyph whose ink starts left of the pen (the superscript digits `¹ ² ³` do in the source files, by up to eight pixels) is moved to the pen, and a glyph whose ink reaches past its advance (the capital Q in the small font) gets a wider advance.
- A few accents of the capital letters reach above the font's ascent; the line grows by the missing rows (the large font's line is two rows taller than the font file says), so that nothing a font draws lies outside its line.

## The character set

`assets/fonts/charset.txt` is the one definition of what the interface can draw: printable Basic Latin (U+0020–U+007E), Latin-1 (U+00A0–U+00FF), `Œ œ Ÿ €`, and the ten interface symbols. The font generator, `scripts/check_i18n.py` (catalogs may use nothing else) and the host tests all read that file. A character is added to it, drawn in the fonts, and only then used in a catalog.

The Lucida files already contain `Œ œ Ÿ €`; only the symbols are drawn by us, in `assets/fonts/symbols.txt`, one set per font size. Each is a block of `#` and `.` under a `glyph <name> top <rows>` line, where `top` is the number of blank rows between the top of the capitals and the top of the symbol.

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

A catalog writes the symbol as that character. A character outside the set — or a byte sequence that is not UTF-8 — is drawn as Lucida's own box glyph, one per malformed sequence, so the fault is visible and the rest of the text is still there.

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

(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents pending in the U.S. and foreign countries. OPEN LOOK is a trademark of AT&T. Used by written permission of the owners.

(c) Copyright Bigelow & Holmes 1986, 1985. Lucida is a registered trademark of Bigelow & Holmes. Permission to use the Lucida trademark is hereby granted only in association with the images and fonts described in this file.

The Lucida licence is in `assets/fonts/LICENSE-Lucida-BH.txt`. Because the fonts here also carry our own symbols, the generated fonts are called `FontSmall`, `FontText`, `FontTile` and `FontLarge`, never Lucida.
