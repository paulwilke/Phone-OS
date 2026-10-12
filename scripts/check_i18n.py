#!/usr/bin/env python3
"""Checks the language catalogs in lang/ against the English reference. (allow-non-english: holds the GSM alphabet, which has umlauts.)

    scripts/check_i18n.py            # from the repository root
    scripts/check_i18n.py --usage    # the keys of the English files against the code that uses them

Two catalogs per language: lang/<code>.toml is what the device's own screens, notices, SMS bodies and
system entries say; lang/web/<code>.toml is what the parents' page and the configuration form say
(web.*, config.*). The device loads the first at start and the second only when the page needs it. A key
lives in one of the two, never in both. Each set is checked on its own against its English file: the
same keys in every language, the same {placeholders}, the plural forms. Words that only host tools show
(the demo page) are a third kind, tools/lang/<set>.<code>.toml; they never go to the device, so they
do not weigh on its limit for a catalog (ConfigLoader::kMaxCatalogBytes), and are checked like the
others (their text goes to a browser and not to the device font). The size of every shipped file is
printed against the limit, which applies to each file; lang/en.toml and lang/web/en.toml, the files a
translator starts from, must fit.

For every file of the two sets:
  * the [meta] header is complete and the plural rule is known;
  * every message has the number of plural forms the rule needs (or one form);
  * the keys are exactly the keys of the English file of the set (nothing missing, no leftovers);
  * every message uses the same {placeholders} as the English one;
  * (device set) only characters the display fonts can draw are used (assets/fonts/charset.txt); the web
    set goes to a browser and has no such limit;
  * (device set) messages whose key starts with `sms.` use only the GSM 7-bit alphabet, so that an SMS
    stays one part.

The runtime parser (src/i18n/Catalog.cpp) enforces the same format rules on catalogs
users put on the SD card; this script adds the cross-language checks for the catalogs
that ship with the firmware. Exits with 1 on any problem.

`--usage` checks the other direction: every key of the English files appears as a string literal in the
code or in a test (a key nothing asks for is dead weight, or the screen that should use it has lost it),
and every string literal that looks like a key of a catalog section ("status.gps") is a key of one of
them (a misspelled key shows the key itself on the child's screen), and no file of the interface (src/ui/)
holds a sentence as a literal: two words with a space between them are text the child would read in one
language only, so it belongs in a catalog. Which code uses which set: the device
code (src/ without src/web/) uses the device set; scripts/gen_web.py, config/schema.toml and the web
service code (src/web/) use the web set. A key of the device set that only web code uses, and a key of the
web set that only device code uses, is reported: it is in the wrong file. The code is src/, testing/ and
test/; a key used only by a test is used, because the tests are where a screen's texts are laid out in
every language before the screen exists.
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

DEVICE_DIR = os.path.join(ROOT, "lang")
WEB_DIR = os.path.join(ROOT, "lang", "web")


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


def shipped(directory):
    """{code: (path, meta, strings)} of the *.toml files of one directory (not of its sub-directories)."""
    out = {}
    for path in sorted(glob.glob(os.path.join(directory, "*.toml"))):
        meta, strings = load(path)
        out[os.path.basename(path)[:-5]] = (path, meta, strings)
    return out


def shown(path):
    return os.path.relpath(path, ROOT)


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
        # The words of an enum's values (same formula as enum_option_label_key in gen_config.py).
        if entry.get("type") == "enum" and entry.get("label", "").endswith(".label"):
            stem = entry["label"][:-len(".label")]
            for v in entry.get("enums", "").split("|"):
                keys[f"{stem}.{v}.label"] = "config/schema.toml"
    return keys


def kind_of(rel):
    """Who is the file, for the question which set a key belongs to?
      web    - the parents' page: the web service (src/web/), its generator, the schema of the form;
      device - the firmware (src/, the rest of it);
      test   - the tests and the test helpers (the texts of a screen are laid out there before the screen
               exists): they count as a user of either set;
      host   - everything else (the simulator, the demo generator, other scripts), which the device does
               not need words for."""
    top = rel.split(os.sep)
    if rel == os.path.join("scripts", "gen_web.py") or rel == os.path.join("config", "schema.toml") or rel.startswith(os.path.join("src", "web") + os.sep):
        return "web"
    if top[0] == "src":
        return "device"
    if top[0] in ("testing", "test"):
        return "test"
    return "host"


# A file name is no catalog key, even when its first part is also the name of a section ("settings.toml").
FILE_SUFFIXES = (".toml", ".txt", ".json", ".csv")


# Two words with a space between them, in a string literal: a sentence. Assertion messages are for developers.
SENTENCE = re.compile(r'"(?:[^"\\\n]|\\.)*[A-Za-z]{2,} [A-Za-z]{2,}(?:[^"\\\n]|\\.)*"')


def sentence_problems():
    """The files of the interface hold no sentence as a literal: user text lives in the catalogs."""
    found = []
    for folder, _, names in os.walk(os.path.join(ROOT, "src", "ui")):
        for name in sorted(names):
            if not name.endswith((".h", ".cpp")):
                continue
            path = os.path.join(folder, name)
            with open(path, encoding="utf-8") as f:
                lines = f.read().split("\n")
            for number, line in enumerate(lines, 1):
                code = re.sub(r"//.*$", "", line)  # a comment may quote a sentence; the code may not
                if "static_assert" in code or code.lstrip().startswith("#include"):
                    continue
                if SENTENCE.search(code):
                    found.append(f"{os.path.relpath(path, ROOT)}:{number}: a sentence in a string literal: user text belongs in a catalog (lang/)")
    return found


def usage():
    """Compares the keys of the catalogs with the key-like string literals of the code."""
    device = shipped(DEVICE_DIR).get("en")
    web = shipped(WEB_DIR).get("en")
    if not device or not web:
        print("lang/en.toml or lang/web/en.toml is missing")
        return 1
    ref, web_ref = device[2], web[2]
    host = {}
    for name, langs in host_sets().items():
        if "en" in langs:
            host.update({key: name for key in langs["en"][2]})
    sections = {key.split(".")[0] for key in ref} | {key.split(".")[0] for key in web_ref} | {key.split(".")[0] for key in host}
    literals = {}  # literal -> [(file:line, kind)]
    GENERATED_MARKERS = ("Generated by scripts/gen_config.py", "Generated by scripts/gen_apn_table.py")
    for top in CODE_DIRS:
        for folder, _, names in os.walk(os.path.join(ROOT, top)):
            for name in names:
                if not name.endswith(CODE_SUFFIXES):
                    continue
                path = os.path.join(folder, name)
                with open(path, encoding="utf-8") as f:
                    text = f.read()
                # Skip files that were generated from the schema (their string literals include Profile
                # field paths) or from the operator list (access point names that begin like a catalog section): they look like
                # catalog keys but are not.
                if any(marker in text[:200] for marker in GENERATED_MARKERS):
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
                        literals.setdefault(literal, []).append((f"{rel_path}:{number}", kind_of(rel_path)))
    # config/schema.toml is a user of its label/help catalog keys
    for k, src in _schema_catalog_keys().items():
        literals.setdefault(k, []).append((src, "web"))
    problems = []

    def judge(key, set_name, own, other, other_name):
        """`own` is the kind that belongs to the set, `other` the kind that does not."""
        users = literals.get(key, [])
        kinds = {kind for _, kind in users}
        if not users:
            problems.append(f"`{key}` is in the {set_name} set but no code or test mentions it")
        elif own not in kinds and "test" not in kinds:
            where = next(place for place, kind in users if kind == other) if other in kinds else users[0][0]
            if other in kinds:
                problems.append(f"`{key}` is in the {set_name} set ({'lang/web/' if set_name == 'web' else 'lang/'}) but only {other} code uses it ({where}): "
                                f"move it to the {other_name} set")
            else:
                problems.append(f"`{key}` is shipped in the {set_name} set but only host code uses it ({where}): move it to tools/lang/")

    for key in sorted(ref):
        judge(key, "device", "device", "web", "web")
    for key in sorted(web_ref):
        judge(key, "web", "web", "device", "device")
    for key, name in sorted(host.items()):
        if key not in literals:
            problems.append(f"`{key}` is in the host set `{name}` (tools/lang/) but no code or test mentions it")
    known = set(ref) | set(web_ref) | set(host)
    problems += [f"{users[0][0]}: \"{literal}\" looks like a catalog key but is not in a catalog (lang/, lang/web/) or a host set (tools/lang/)"
                 for literal, users in sorted(literals.items())
                 if literal.split(".")[0] in sections and literal not in known]
    problems += sentence_problems()
    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} problems with the use of the keys.")
        return 1
    print(f"check_i18n --usage: {len(ref)} keys of the device set, {len(web_ref)} of the web set and {len(host)} of the host sets, "
          f"all used by the code that belongs to their set, no unknown keys in the code")
    return 0


def check_set(label, langs, ref_code, problems, device):
    """One set of catalogs (a dict code -> (path, meta, strings)) against its English file."""
    if ref_code not in langs:
        problems.append(f"{label}/{ref_code}.toml is missing: English is the reference of the set")
        return
    ref = langs[ref_code][2]
    for code, (path, meta, strings) in sorted(langs.items()):
        name = f"{label}/{code}.toml"
        for field in ("code", "name", "native", "plural"):
            if not meta.get(field):
                problems.append(f"{name}: [meta] lacks `{field}`")
        if meta.get("plural") not in PLURAL_FORMS:
            problems.append(f"{name}: unknown plural rule {meta.get('plural')!r}")
            continue
        if meta.get("code") != code:
            problems.append(f"{name}: code {meta.get('code')!r} does not match the file name")
        want = PLURAL_FORMS[meta["plural"]]
        for key, value in strings.items():
            f = forms(value)
            if len(f) not in (1, want):
                problems.append(f"{name}: `{key}` has {len(f)} forms, the rule needs {want}")
            if not device:
                continue
            for form in f:
                if key.startswith("sms."):
                    outside = sorted({c for c in PLACEHOLDER.sub("", form) if c not in GSM7})
                    if outside:
                        problems.append(f"{name}: `{key}` is sent by SMS but uses characters outside the GSM alphabet: {''.join(outside)}")
                bad = charset.allowed_text(form, CODEPOINTS)
                if bad:
                    problems.append(f"{name}: `{key}` uses characters outside assets/fonts/charset.txt: {''.join(bad)}")
        if code == ref_code:
            continue
        for key in sorted(set(ref) - set(strings)):
            problems.append(f"{name}: missing `{key}`")
        for key in sorted(set(strings) - set(ref)):
            problems.append(f"{name}: `{key}` is not in {ref_code}.toml of the set (renamed or removed?)")
        for key in sorted(set(ref) & set(strings)):
            want_ph = {p for form in forms(ref[key]) for p in PLACEHOLDER.findall(form)}
            for form in forms(strings[key]):
                if set(PLACEHOLDER.findall(form)) != want_ph:
                    problems.append(f"{name}: `{key}` has different {{placeholders}} than English")
                    break


def accent_share(strings):
    """The share of the letters of a catalog that are not ASCII: 0 for a language written without accents."""
    letters = [c for value in strings.values() for form in forms(value) for c in PLACEHOLDER.sub("", form) if c.isalpha()]
    return sum(ord(c) > 127 for c in letters) / len(letters) if letters else 0.0


def transliteration_problems(device, web):
    """The page of a language that has accents on the device is written with them too. A catalog in ASCII
    transliteration ("Geraet", "Parametres") reads wrongly to the parents, and the page is a browser's and has
    no font limit that would excuse it."""
    problems = []
    for code in sorted(set(device) & set(web)):
        on_device, on_page = accent_share(device[code][2]), accent_share(web[code][2])
        if on_device >= 0.005 and on_page < on_device / 4:
            problems.append(f"lang/web/{code}.toml: only {on_page:.1%} of its letters carry an accent against {on_device:.1%} in lang/{code}.toml: "
                            f"is it written in ASCII transliteration? The page is shown in a browser and may use every letter")
    return problems


def main():
    if "--usage" in sys.argv[1:]:
        return usage()
    problems = []
    device = shipped(DEVICE_DIR)
    web = shipped(WEB_DIR)
    if not device:
        print("no catalogs in lang/")
        return 1
    if not web:
        print("no catalogs in lang/web/")
        return 1

    check_set("lang", device, "en", problems, True)
    check_set("lang/web", web, "en", problems, False)
    # Both sets name the same languages: a language with a device catalog and no web catalog would show
    # the page in English, which is what the device does for a language it lacks, but not what ships.
    for code in sorted(set(device) - set(web)):
        problems.append(f"lang/web/{code}.toml is missing: the device has lang/{code}.toml")
    for code in sorted(set(web) - set(device)):
        problems.append(f"lang/{code}.toml is missing: the page has lang/web/{code}.toml")

    problems += transliteration_problems(device, web)

    device_ref = device.get("en", (None, None, {}))[2]
    web_ref = web.get("en", (None, None, {}))[2]
    for key in sorted(set(device_ref) & set(web_ref)):
        problems.append(f"`{key}` is in lang/en.toml and in lang/web/en.toml: a key is in one of the two sets only")

    # What a translator copies must fit what the device reads: the English files are the ones to start from.
    limit = catalog_limit()
    sizes = []
    if limit is None:
        problems.append("cannot find kMaxCatalogBytes in src/config/ConfigLoader.h")
    else:
        for langs, prefix in ((device, "lang/"), (web, "lang/web/")):
            for code, (path, _, _) in sorted(langs.items()):
                size = os.path.getsize(path)
                sizes.append(f"{prefix}{code}.toml {size}" + (f" (over by {size - limit})" if size > limit else ""))
                if size > limit and code == "en":
                    problems.append(f"{prefix}en.toml is {size} bytes, over the {limit} that a catalog on the card may have: "
                                    f"it is the file a translator starts from")

    # Host-only sets: the same rules, without the device's font, and no key in the shipped ones.
    for set_name, langs in sorted(host_sets().items()):
        label = f"tools/lang/{set_name}"
        if "en" not in langs:
            problems.append(f"{label}.en.toml is missing: English is the reference of the set")
            continue
        set_ref = langs["en"][2]
        for code in sorted(set(device) - set(langs)):
            problems.append(f"{label}.{code}.toml is missing")
        for code, (path, meta, strings) in sorted(langs.items()):
            name = f"{label}.{code}.toml"
            for field in ("code", "name", "native", "plural"):
                if not meta.get(field):
                    problems.append(f"{name}: [meta] lacks `{field}`")
            if meta.get("plural") not in PLURAL_FORMS:
                problems.append(f"{name}: unknown plural rule {meta.get('plural')!r}")
                continue
            if meta.get("code") != code or code not in device:
                problems.append(f"{name}: code {meta.get('code')!r} is not the language of a catalog in lang/")
            for key, value in strings.items():
                if key in device_ref or key in web_ref:
                    problems.append(f"{name}: `{key}` is in a shipped catalog too: a key is in one place only")
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

    # Every catalog key named in config/schema.toml is a word of the form: it must exist in every web catalog.
    for key in sorted(_schema_catalog_keys()):
        for code, (_, _, strings) in sorted(web.items()):
            if key not in strings:
                problems.append(f"lang/web/{code}.toml: `{key}` is named in config/schema.toml but missing")

    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} problems in the catalogs.")
        return 1
    print(f"check_i18n: {len(device)} device and {len(web)} web catalogs, {len(device_ref)} + {len(web_ref)} messages, no problems")
    if sizes:
        print(f"sizes against the {limit} bytes a catalog on the card may have: " + ", ".join(sizes))
    return 0


if __name__ == "__main__":
    sys.exit(main())
