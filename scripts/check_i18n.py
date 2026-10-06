#!/usr/bin/env python3
"""Checks the language catalogs in lang/ against the English reference. (allow-non-english: holds the GSM alphabet, which has umlauts.)

    scripts/check_i18n.py            # from the repository root
    scripts/check_i18n.py --usage    # the keys of en.toml against the code that uses them

Two sets of catalogs: lang/<code>.toml are shipped to the device; tools/lang/<set>.<code>.toml hold
words that only host tools show (the demo page) and never go to the device, so they do not weigh on
its limit for a catalog (ConfigLoader::kMaxCatalogBytes). The host sets are checked like the others
(same keys in every language, same placeholders, plural forms), except that their text goes to a
browser and not to the device font; a key must be in one set only. The size of every shipped
catalog is printed against the limit; lang/en.toml, the file a translator starts from, must fit.

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


HOST_DIR = os.path.join(ROOT, "tools", "lang")
LIMIT_FILE = os.path.join(ROOT, "src", "config", "ConfigLoader.h")


def catalog_limit():
    """The size a catalog on the card may have, from the one place it is written down."""
    with open(LIMIT_FILE, encoding="utf-8") as f:
        m = re.search(r"kMaxCatalogBytes\s*=\s*(\d+)", f.read())
    return int(m.group(1)) if m else None


def host_sets():
    """{set name: {code: (path, meta, strings)}} from tools/lang/<set>.<code>.toml."""
    sets = {}
    for path in sorted(glob.glob(os.path.join(HOST_DIR, "*.toml"))):
        base = os.path.basename(path)[:-5]
        name, _, code = base.rpartition(".")
        meta, strings = load(path)
        sets.setdefault(name, {})[code] = (path, meta, strings)
    return sets


KEY_LITERAL = re.compile(r'"([a-z][a-z0-9-]*(?:\.[a-z0-9-]+)+)"')
# scripts/ is also searched so that gen_web.py's WEB_KEYS list covers the web.* catalog keys.
CODE_DIRS = ("src", "testing", "test", "scripts", "sim")
CODE_SUFFIXES = (".h", ".cpp", ".py")


def _schema_catalog_keys():
    """Return a dict of catalog keys referenced in config/schema.toml → source description."""
    schema_path = os.path.join(ROOT, "config", "schema.toml")
    if not os.path.exists(schema_path):
        return {}
    with open(schema_path, "rb") as f:
        raw = tomllib.load(f)
    keys = {}
    for entry in raw.get("file", []):
        for field in ("label",):
            k = entry.get(field)
            if k:
                keys[k] = "config/schema.toml"
    for entry in raw.get("section", []):
        for field in ("label", "help"):
            k = entry.get(field)
            if k:
                keys[k] = "config/schema.toml"
    for entry in raw.get("key", []):
        for field in ("label", "help"):
            k = entry.get(field)
            if k:
                keys[k] = "config/schema.toml"
    return keys


def runs_on_device(rel):
    """Does the file belong to what the device is, or tests what it shows? The firmware (src/), the
    tests and the test helpers (the texts of a screen are laid out there before the screen exists),
    the schema, and the generator of the web page the device serves. Everything else (the simulator,
    the demo generator, other scripts) is host code, which the device does not need words for."""
    return rel.split(os.sep)[0] in ("src", "testing", "test", "config") or rel == os.path.join("scripts", "gen_web.py")
# A file name is no catalog key, even when its first part is also the name of a section ("settings.toml").
FILE_SUFFIXES = (".toml", ".txt", ".json", ".csv")


def usage():
    """Compares the keys of the catalogs with the key-like string literals of the code."""
    _, ref = load(os.path.join(ROOT, "lang", "en.toml"))
    host = {}
    for name, langs in host_sets().items():
        if "en" in langs:
            host.update({key: name for key in langs["en"][2]})
    sections = {key.split(".")[0] for key in ref} | {key.split(".")[0] for key in host}
    literals = {}  # literal -> [(file:line, on the device?)]
    GENERATED_MARKER = "Generated by scripts/gen_config.py"
    for top in CODE_DIRS:
        for folder, _, names in os.walk(os.path.join(ROOT, top)):
            for name in names:
                if not name.endswith(CODE_SUFFIXES):
                    continue
                path = os.path.join(folder, name)
                with open(path, encoding="utf-8") as f:
                    text = f.read()
                # Skip files that were generated from the schema; their string literals
                # include Profile field paths that look like catalog keys but are not.
                if GENERATED_MARKER in text[:200]:
                    continue
                # Skip src/config/ — it uses config key-path strings as diagnostic identifiers,
                # not catalog keys. These share namespace prefixes with the catalogs but are not
                # catalog keys.
                rel_path = os.path.relpath(path, ROOT)
                if rel_path.startswith(os.path.join("src", "config") + os.sep):
                    continue
                for number, line in enumerate(text.split("\n"), 1):
                    for literal in KEY_LITERAL.findall(line):
                        if literal.endswith(FILE_SUFFIXES):
                            continue
                        literals.setdefault(literal, []).append((f"{rel_path}:{number}", runs_on_device(rel_path)))
    # config/schema.toml is a user of its label/help catalog keys
    for k, src in _schema_catalog_keys().items():
        literals.setdefault(k, []).append((src, True))
    problems = []
    for key in sorted(ref):
        users = literals.get(key, [])
        if not users:
            problems.append(f"`{key}` is in en.toml but no code or test mentions it")
        elif not any(on_device for _, on_device in users):
            problems.append(f"`{key}` is shipped to the device in lang/ but only host code uses it ({users[0][0]}): "
                            f"move it to tools/lang/")
    for key, name in sorted(host.items()):
        if key not in literals:
            problems.append(f"`{key}` is in the host set `{name}` (tools/lang/) but no code or test mentions it")
    known = set(ref) | set(host)
    problems += [f"{users[0][0]}: \"{literal}\" looks like a catalog key but is not in en.toml or a host set (tools/lang/)"
                 for literal, users in sorted(literals.items())
                 if literal.split(".")[0] in sections and literal not in known]
    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} problems with the use of the keys.")
        return 1
    print(f"check_i18n --usage: {len(ref)} keys of the device and {len(host)} of the host sets, all used, no unknown keys in the code")
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

    loaded = {}  # name → strings (for the schema-key check at the end)
    for path in files:
        name = os.path.basename(path)
        meta, strings = load(path)
        loaded[name] = strings
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
                if not key.startswith("web."):  # web.* strings go to the browser, not the device font
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

    # What a translator copies must fit what the device reads: lang/en.toml is the file to start from.
    limit = catalog_limit()
    sizes = []
    if limit is None:
        problems.append("cannot find kMaxCatalogBytes in src/config/ConfigLoader.h")
    else:
        for path in files:
            name, size = os.path.basename(path), os.path.getsize(path)
            sizes.append(f"{name} {size}" + (f" (over by {size - limit})" if size > limit else ""))
            if size > limit and name == "en.toml":
                problems.append(f"en.toml is {size} bytes, over the {limit} that a catalog on the card may have: "
                                f"it is the file a translator starts from")

    # Host-only sets: the same rules, without the device's font, and no key in both places.
    for set_name, langs in sorted(host_sets().items()):
        label = f"tools/lang/{set_name}"
        if "en" not in langs:
            problems.append(f"{label}.en.toml is missing: English is the reference of the set")
            continue
        _, set_ref = langs["en"][1], langs["en"][2]
        for code in sorted({os.path.basename(p)[:-5] for p in files} - set(langs)):
            problems.append(f"{label}.{code}.toml is missing")
        for code, (path, meta, strings) in sorted(langs.items()):
            name = f"{label}.{code}.toml"
            for field in ("code", "name", "native", "plural"):
                if not meta.get(field):
                    problems.append(f"{name}: [meta] lacks `{field}`")
            if meta.get("plural") not in PLURAL_FORMS:
                problems.append(f"{name}: unknown plural rule {meta.get('plural')!r}")
                continue
            if meta.get("code") != code or not os.path.exists(os.path.join(ROOT, "lang", code + ".toml")):
                problems.append(f"{name}: code {meta.get('code')!r} is not the language of a catalog in lang/")
            for key, value in strings.items():
                if key in ref:
                    problems.append(f"{name}: `{key}` is in lang/en.toml too: a key is in one place only")
                if len(forms(value)) not in (1, PLURAL_FORMS[meta["plural"]]):
                    problems.append(f"{name}: `{key}` has {len(forms(value))} forms, the rule needs {PLURAL_FORMS[meta['plural']]}")
            if code == "en":
                continue
            for key in sorted(set(set_ref) - set(strings)):
                problems.append(f"{name}: missing `{key}`")
            for key in sorted(set(strings) - set(set_ref)):
                problems.append(f"{name}: `{key}` is not in the English file of the set")
            for key in sorted(set(set_ref) & set(strings)):
                want_ph = {p for form in forms(set_ref[key]) for p in PLACEHOLDER.findall(form)}
                for form in forms(strings[key]):
                    if set(PLACEHOLDER.findall(form)) != want_ph:
                        problems.append(f"{name}: `{key}` has different {{placeholders}} than English")
                        break

    # Every catalog key named in config/schema.toml must exist in every catalog.
    schema_keys = _schema_catalog_keys()
    for key in sorted(schema_keys):
        for name, strings in loaded.items():
            if key not in strings:
                problems.append(f"{name}: `{key}` is named in config/schema.toml but missing")

    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} problems in the catalogs.")
        return 1
    print(f"check_i18n: {len(files)} catalogs, {len(ref)} messages, no problems")
    if sizes:
        print(f"sizes against the {limit} bytes a catalog on the card may have: " + ", ".join(sizes))
    return 0


if __name__ == "__main__":
    sys.exit(main())
