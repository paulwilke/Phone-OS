"""Reads assets/fonts/charset.txt, the one definition of the characters the interface can draw.

Used by make_font.py, check_i18n.py and (through the file itself) the host tests.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHARSET_FILE = os.path.join(ROOT, "assets", "fonts", "charset.txt")


class CharsetError(Exception):
    pass


def load(path=CHARSET_FILE):
    """Returns (codepoints, symbols): the sorted list of all code points, and {name: code point}."""
    codepoints = set()
    symbols = {}
    with open(path, encoding="utf-8") as f:
        for number, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].split()
            if not line:
                continue
            try:
                if line[0] == "range" and len(line) == 3:
                    first, last = int(line[1], 16), int(line[2], 16)
                    if first > last:
                        raise ValueError
                    codepoints.update(range(first, last + 1))
                elif line[0] == "char" and len(line) == 2:
                    codepoints.add(int(line[1], 16))
                elif line[0] == "symbol" and len(line) == 3:
                    code = int(line[1], 16)
                    if line[2] in symbols:
                        raise CharsetError(f"{path}:{number}: symbol {line[2]} twice")
                    symbols[line[2]] = code
                    codepoints.add(code)
                else:
                    raise ValueError
            except ValueError:
                raise CharsetError(f"{path}:{number}: cannot read: {raw.strip()}")
    return sorted(codepoints), symbols


def allowed_text(text, codepoints):
    """The characters of `text` that are not in the set (sorted, without repeats)."""
    allowed = set(codepoints)
    return sorted({c for c in text if ord(c) not in allowed and c not in "\n"})
