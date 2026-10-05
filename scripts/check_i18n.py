#!/usr/bin/env python3
"""Checks the language catalogs in lang/ against the English reference. (allow-non-english: holds the GSM alphabet, which has umlauts.)

    scripts/check_i18n.py            # from the repository root
    scripts/check_i18n.py --usage    # the keys of en.toml against the code that uses them

For every lang/*.toml:
  * the [meta] header is complete and the plural rule is known;
  * every message has the number of plural forms the rule needs (or one form);
  * the keys are exactly the keys of en.toml (nothing missing, no leftovers);
  * every message uses the same {placeholders} as the English one;
  * only characters the display fonts can draw are used (assets/fonts/charset.txt);
  * messages whose key starts with `sms.` use only the GSM 7-bit alphabet, so that an SMS stays
    one part.

The runtime parser (src/i18n/Catalog.cpp) enforces the same format rules on catalogs
users put on the SD card; this script adds the cross-language checks for the catalogs
that ship with the firmware. Exits with 1 on any problem.

`--usage` checks the other direction: every key of en.toml appears as a string literal in the
code or in a test (a key nothing asks for is dead weight, or the screen that should use it
has lost it), and every string literal that looks like a key of a catalog section ("status.gps")
is a key of en.toml (a misspelled key shows the key itself on the child's screen). The code is
src/, testing/ and test/; a key used only by a test is used, because the tests are where a
screen's texts are laid out in every language before the screen exists.
"""
import glob
import os
import re
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import charset  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODEPOINTS = charset.load()[0]
PLURAL_FORMS = {"single": 1, "one-other": 2, "zero-one-other": 2}
# Characters of the GSM 7-bit alphabet (basic table and extension table). A text sent
# by SMS that contains anything else is sent as UCS-2, which fits 70 characters per
# part instead of 160 and splits an emergency message into several parts.
GSM7 = set("@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞÆæßÉ !\"#¤%&'()*+,-./0123456789:;<=>?¡"
           "ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§¿abcdefghijklmnopqrstuvwxyzäöñüà"
           "^{}\\[~]|€")
PLACEHOLDER = re.compile(r"(?<!\{)\{([a-z0-9_-]+)\}(?!\})")


def flatten(table, prefix=""):
    out = {}
    for key, value in table.items():
        full = f"{prefix}{key}"
        if isinstance(value, dict):
            out.update(flatten(value, full + "."))
        else:
            out[full] = value
    return out


def forms(value):
    return value if isinstance(value, list) else [value]


def load(path):
    with open(path, "rb") as f:
        data = tomllib.load(f)
    return data.get("meta", {}), flatten(data.get("strings", {}))


KEY_LITERAL = re.compile(r'"([a-z][a-z0-9-]*(?:\.[a-z0-9-]+)+)"')
CODE_DIRS = ("src", "testing", "test")
CODE_SUFFIXES = (".h", ".cpp")
# A file name is no catalog key, even when its first part is also the name of a section ("settings.toml").
FILE_SUFFIXES = (".toml", ".txt", ".json", ".csv")


def usage():
    """Compares the keys of en.toml with the key-like string literals of the code."""
    _, ref = load(os.path.join(ROOT, "lang", "en.toml"))
    sections = {key.split(".")[0] for key in ref}
    literals = {}
    for top in CODE_DIRS:
        for folder, _, names in os.walk(os.path.join(ROOT, top)):
            for name in names:
                if not name.endswith(CODE_SUFFIXES):
                    continue
                path = os.path.join(folder, name)
                with open(path, encoding="utf-8") as f:
                    for number, line in enumerate(f, 1):
                        for literal in KEY_LITERAL.findall(line):
                            literals.setdefault(literal, f"{os.path.relpath(path, ROOT)}:{number}")
    problems = [f"`{key}` is in en.toml but no code or test mentions it" for key in sorted(ref) if key not in literals]
    problems += [f"{where}: \"{literal}\" looks like a catalog key but is not in en.toml"
                 for literal, where in sorted(literals.items())
                 if literal.split(".")[0] in sections and literal not in ref and not literal.endswith(FILE_SUFFIXES)]
    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} problems with the use of the keys.")
        return 1
    print(f"check_i18n --usage: {len(ref)} keys, all used, no unknown keys in the code")
    return 0


def main():
    if "--usage" in sys.argv[1:]:
        return usage()
    problems = []
    files = sorted(glob.glob(os.path.join(ROOT, "lang", "*.toml")))
    if not files:
        print("no catalogs in lang/")
        return 1
    ref_path = os.path.join(ROOT, "lang", "en.toml")
    ref_meta, ref = load(ref_path)

    for path in files:
        name = os.path.basename(path)
        meta, strings = load(path)
        for field in ("code", "name", "native", "plural"):
            if not meta.get(field):
                problems.append(f"{name}: [meta] lacks `{field}`")
        if meta.get("plural") not in PLURAL_FORMS:
            problems.append(f"{name}: unknown plural rule {meta.get('plural')!r}")
            continue
        if meta.get("code") != name[:-5]:
            problems.append(f"{name}: code {meta.get('code')!r} does not match the file name")
        want = PLURAL_FORMS[meta["plural"]]
        for key, value in strings.items():
            f = forms(value)
            if len(f) not in (1, want):
                problems.append(f"{name}: `{key}` has {len(f)} forms, the rule needs {want}")
            for form in f:
                if key.startswith("sms."):
                    outside = sorted({c for c in PLACEHOLDER.sub("", form) if c not in GSM7})
                    if outside:
                        problems.append(f"{name}: `{key}` is sent by SMS but uses characters outside the GSM alphabet: {''.join(outside)}")
                bad = charset.allowed_text(form, CODEPOINTS)
                if bad:
                    problems.append(f"{name}: `{key}` uses characters outside assets/fonts/charset.txt: {''.join(bad)}")
        if name == "en.toml":
            continue
        for key in sorted(set(ref) - set(strings)):
            problems.append(f"{name}: missing `{key}`")
        for key in sorted(set(strings) - set(ref)):
            problems.append(f"{name}: `{key}` is not in en.toml (renamed or removed?)")
        for key in sorted(set(ref) & set(strings)):
            want_ph = {p for form in forms(ref[key]) for p in PLACEHOLDER.findall(form)}
            for form in forms(strings[key]):
                if set(PLACEHOLDER.findall(form)) != want_ph:
                    problems.append(f"{name}: `{key}` has different {{placeholders}} than English")
                    break

    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} problems in the catalogs.")
        return 1
    print(f"check_i18n: {len(files)} catalogs, {len(ref)} messages, no problems")
    return 0


if __name__ == "__main__":
    sys.exit(main())
