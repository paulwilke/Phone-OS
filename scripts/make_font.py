#!/usr/bin/env python3
"""Generates the display fonts in src/ui/fonts/ from the sources in assets/fonts/.

    scripts/make_font.py --all      # write every font
    scripts/make_font.py --check    # fail if a generated file is not what --all would write

Sources:
  * Lucida Sans bitmap fonts by Bigelow & Holmes (BDF, hand-pixelled, 100 dpi). The glyphs are
    taken over pixel by pixel and are not changed, which the Lucida licence requires
    (assets/fonts/LICENSE-Lucida-BH.txt); only where a glyph sits relative to the pen is
    corrected when its ink would lie outside its cell (see read_bdf). Every font has the glyph of every character in
    assets/fonts/charset.txt, except the interface symbols.
  * assets/fonts/symbols.txt: the interface symbols, our own pixel art, one set per font size.
  * Lucida's own box glyph (code 0 in the BDF files) is the replacement glyph that is drawn for
    a character the font does not have.

The generated files hold nothing but data. They are named after their role (FontSmall, ...), not
after Lucida, because the licence allows the Lucida name only on unmodified fonts and ours also
hold our own symbols. Dependency-free on purpose: the build must not need anything installed.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import charset  # noqa: E402

ROOT = charset.ROOT
FONT_DIR = os.path.join(ROOT, "assets", "fonts")
OUT_DIR = os.path.join(ROOT, "src", "ui", "fonts")
SYMBOL_FILE = os.path.join(FONT_DIR, "symbols.txt")

# (name used in symbols.txt, generated C++ name, BDF source)
FONTS = [
    ("small", "FontSmall", "luBS10.bdf"),
    ("text", "FontText", "luRS14.bdf"),
    ("tile", "FontTile", "luBS18.bdf"),
    ("large", "FontLarge", "luBS24.bdf"),
]
# Blank columns after a symbol: the Lucida letters carry their spacing in their advance.
SYMBOL_GAP = {"small": 1, "text": 1, "tile": 2, "large": 2}

NOTICE = """\
// (c) Copyright Bigelow & Holmes 1986, 1985. Lucida is a registered trademark of Bigelow &
// Holmes. Permission to use the Lucida trademark is hereby granted only in association with the
// images and fonts described in the licence file. (c) Copyright 1989 Sun Microsystems, Inc. Sun
// design patents pending in the U.S. and foreign countries. OPEN LOOK is a trademark of AT&T.
// Used by written permission of the owners.
// Licence: assets/fonts/LICENSE-Lucida-BH.txt. The letter glyphs are unchanged; the interface
// symbols are our own (assets/fonts/symbols.txt) and no part of Lucida."""


class FontError(Exception):
    pass


def read_bdf(path):
    """Returns ({code: (rows, xoff, yoff_from_baseline_top, advance)}, ascent, descent, cap)."""
    glyphs = {}
    ascent = descent = cap = None
    with open(path, encoding="latin-1") as f:
        lines = iter(f.read().splitlines())
    for line in lines:
        key = line.split(" ", 1)[0]
        if key == "FONT_ASCENT":
            ascent = int(line.split()[1])
        elif key == "FONT_DESCENT":
            descent = int(line.split()[1])
        elif key == "CAP_HEIGHT":
            cap = int(line.split()[1])
        elif key == "STARTCHAR":
            code = advance = None
            w = h = xo = yo = 0
            rows = []
            for line in lines:
                if line.startswith("ENCODING"):
                    code = int(line.split()[1])
                elif line.startswith("DWIDTH"):
                    advance = int(line.split()[1])
                elif line.startswith("BBX"):
                    w, h, xo, yo = (int(v) for v in line.split()[1:5])
                elif line == "BITMAP":
                    for _ in range(h):
                        digits = next(lines).strip()
                        bits = bin(int(digits, 16))[2:].zfill(len(digits) * 4)
                        rows.append([int(b) for b in bits[:w]])
                elif line == "ENDCHAR":
                    break
            if code is not None:
                # BDF counts the bottom edge up from the baseline; we count the top edge down
                # from the top of the line.
                # Placement only, never pixels: some glyphs have ink left of the pen (the
                # superscript digits sit up to 8 px to its left in the source) or past their
                # advance (Q). The text engine promises that a string's width is the width of
                # its ink, so the ink is moved into the cell and the advance widened to hold it.
                xo = max(xo, 0)
                advance = max(advance, xo + w)
                glyphs[code] = (rows, xo, ascent - (yo + h), advance)
    if None in (ascent, descent, cap):
        raise FontError(f"{path}: FONT_ASCENT, FONT_DESCENT or CAP_HEIGHT missing")
    return glyphs, ascent, descent, cap


def read_symbols(path):
    """Returns {font: (cap, {name: (rows, top)})} from symbols.txt."""
    fonts = {}
    current = None
    glyph = None
    with open(path, encoding="utf-8") as f:
        for number, raw in enumerate(f, 1):
            line = raw.rstrip("\n")
            where = f"{path}:{number}"
            words = line.split()
            if glyph is not None and line and set(line) <= {"#", "."}:
                glyph[0].append([1 if c == "#" else 0 for c in line])  # before comments: a row of # is art
            elif line.startswith("#") or (not line.strip() and glyph is None):
                continue
            elif not line.strip():
                glyph = None
            elif words[0] == "font" and len(words) == 4 and words[2] == "cap":
                current = fonts.setdefault(words[1], (int(words[3]), {}))
                glyph = None
            elif words[0] == "glyph" and len(words) == 4 and words[2] == "top" and current:
                if words[1] in current[1]:
                    raise FontError(f"{where}: glyph {words[1]} twice")
                glyph = ([], int(words[3]))
                current[1][words[1]] = glyph
            else:
                raise FontError(f"{where}: cannot read: {line}")
    return fonts


def pack(rows, width):
    """Rows of 0/1 to bytes: each row starts on a byte boundary, leftmost pixel in the top bit."""
    out = bytearray()
    for row in rows:
        for start in range(0, width, 8):
            byte = 0
            for bit in range(8):
                x = start + bit
                byte = (byte << 1) | (row[x] if x < width else 0)
            out.append(byte)
    return out


def build(kind, cpp_name, bdf_name, symbol_sets):
    """The text of src/ui/fonts/<cpp_name>.h."""
    codepoints, symbols = charset.load()
    symbol_codes = {code: name for name, code in symbols.items()}
    bdf, ascent, descent, cap = read_bdf(os.path.join(FONT_DIR, bdf_name))
    if kind not in symbol_sets:
        raise FontError(f"{SYMBOL_FILE}: no section for font {kind}")
    symbol_cap, art = symbol_sets[kind]
    if symbol_cap != cap:
        raise FontError(f"{SYMBOL_FILE}: font {kind} says cap {symbol_cap}, {bdf_name} has {cap}")

    # Glyph 0 is the replacement glyph; the others follow in code point order.
    entries = []  # (code or None, rows, xoff, yoff, advance)
    if 0 not in bdf:
        raise FontError(f"{bdf_name}: lacks the replacement glyph (code 0)")
    rows, xo, yo, adv = bdf[0]
    entries.append((None, rows, xo, yo, adv))
    for code in codepoints:
        if code in symbol_codes:
            name = symbol_codes[code]
            if name not in art:
                raise FontError(f"{SYMBOL_FILE}: font {kind} lacks the symbol {name}")
            srows, top = art[name]
            if not srows or len({len(r) for r in srows}) != 1:
                raise FontError(f"{SYMBOL_FILE}: {kind} {name}: empty or ragged rows")
            y = ascent - cap + top
            if top < 0 or y + len(srows) > ascent + descent:
                raise FontError(f"{SYMBOL_FILE}: {kind} {name}: does not fit into the line")
            entries.append((code, srows, 0, y, len(srows[0]) + SYMBOL_GAP[kind]))
        else:
            if code not in bdf:
                raise FontError(f"{bdf_name}: lacks U+{code:04X}")
            rows, xo, yo, adv = bdf[code]
            entries.append((code, rows, xo, yo, adv))
    unused = set(art) - set(symbols)
    if unused:
        raise FontError(f"{SYMBOL_FILE}: font {kind} draws symbols charset.txt does not know: {sorted(unused)}")

    # Some Lucida accents reach above the font's ascent or below its descent; the line grows
    # to hold them, so that nothing a font draws is outside its line.
    lift = max(0, -min(e[3] for e in entries))
    ascent += lift
    entries = [(c, r, xo, yo + lift, adv) for c, r, xo, yo, adv in entries]
    descent = max(descent, max(e[3] + len(e[1]) for e in entries) - ascent)

    bitmap = bytearray()
    glyph_lines = []
    for code, rows, xo, yo, adv in entries:
        w = len(rows[0]) if rows else 0
        offset = len(bitmap)
        bitmap += pack(rows, w)
        if offset > 0xFFFF or not (-128 <= xo < 128 and -128 <= yo < 128 and adv < 256 and w < 256):
            raise FontError(f"{cpp_name}: a value does not fit the glyph table")
        label = "replacement" if code is None else f"U+{code:04X}"
        glyph_lines.append(f"    {{{offset}, {w}, {len(rows)}, {xo}, {yo}, {adv}}},  // {label}")

    ranges = []  # (first, last, index of first)
    for index, (code, *_rest) in enumerate(entries):
        if code is None:
            continue
        if ranges and ranges[-1][1] + 1 == code and ranges[-1][2] + (code - ranges[-1][0]) == index:
            ranges[-1] = (ranges[-1][0], code, ranges[-1][2])
        else:
            ranges.append((code, code, index))

    def hexlines(data):
        return "\n".join("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ","
                         for i in range(0, len(data), 16))

    text = f"""\
#pragma once

// Generated by scripts/make_font.py from assets/fonts/{bdf_name} and assets/fonts/symbols.txt;
// do not edit by hand. Include this file only from src/ui/Fonts.cpp.
{NOTICE}

#include "ui/Font.h"

namespace ui {{
namespace fonts {{

constexpr uint8_t k{cpp_name[4:]}Bitmaps[] = {{
{hexlines(bitmap)}
}};

constexpr Glyph k{cpp_name[4:]}Glyphs[] = {{
{chr(10).join(glyph_lines)}
}};

constexpr CodeRange k{cpp_name[4:]}Ranges[] = {{
{chr(10).join(f"    {{0x{a:X}, 0x{b:X}, {i}}}," for a, b, i in ranges)}
}};

constexpr Font k{cpp_name} = {{
    k{cpp_name[4:]}Bitmaps, k{cpp_name[4:]}Glyphs, k{cpp_name[4:]}Ranges,
    {len(ranges)}, {ascent}, {descent}, {cap},
}};

}}  // namespace fonts
}}  // namespace ui
"""
    return text


def generate():
    symbol_sets = read_symbols(SYMBOL_FILE)
    return {os.path.join(OUT_DIR, f"{cpp}.h"): build(kind, cpp, bdf, symbol_sets) for kind, cpp, bdf in FONTS}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--all", action="store_true", help="write every generated font")
    group.add_argument("--check", action="store_true", help="fail if a generated file is out of date")
    args = parser.parse_args()
    try:
        files = generate()
    except (FontError, charset.CharsetError, OSError) as e:
        print(f"make_font: {e}", file=sys.stderr)
        return 1
    stale = []
    for path, text in files.items():
        current = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if args.all:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            if current != text:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
                print(f"wrote {os.path.relpath(path, ROOT)}")
        elif current != text:
            stale.append(os.path.relpath(path, ROOT))
    if stale:
        print("out of date (run scripts/make_font.py --all): " + ", ".join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
