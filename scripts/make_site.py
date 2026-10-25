#!/usr/bin/env python3
"""Assembles the project's web page (GitHub Pages) from its parts.

    pio run -e sim
    python3 scripts/make_demo.py            # the pictures and the click-through demo, out/demo/
    python3 scripts/make_site.py            # site/ + tools/lang/site.*.toml + out/demo/ + VERSION -> out/site/ (not committed)

The page in site/ has no pictures and no words of its own. The device on it is the click-through demo, whose
pictures the simulator draws on every build (docs/process/demo.md). Its words are the `site.*` keys of the host-only
catalogs tools/lang/site.<code>.toml: every element that says something names its key (data-i18n="site.hero.title",
or data-i18n-alt, data-i18n-aria-label, data-i18n-content for an attribute), and the script names the others. The
assembly writes the English words into the page, so that a reader without a script and a link preview see the page
in English, its default, and embeds the words of every language for the selector.

The result is checked so that a run that "built" a page without its pictures or its words fails: the exit status is 1
when the demo is missing, when a language lacks a word the page uses (the key is named), or when a file the page
needs is not there.
"""
import argparse
import html
import json
import os
import re
import shutil
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# What the page fetches at run time. A missing one is a broken page, not a warning.
NEEDED = ["index.html", "version.txt", "demo/index.html", "demo/graph.json", "demo/en/n0.png", "demo/de/n0.png", "demo/fr/n0.png"]
# Where the words of all languages go; the page holds it once, in its data script.
DATA = "__DATA__"
DEFAULT = "en"

# An element whose words are its text: it is empty in site/, and its key names what goes in.
TEXT_KEY = re.compile(r'(<([a-zA-Z][a-zA-Z0-9]*)\b[^>]*?\sdata-i18n="([^"]+)"[^>]*>)(</\2>)')
# An attribute with words in it: data-i18n-alt="key" becomes alt="...".
ATTRIBUTE_KEY = re.compile(r'\sdata-i18n-([a-z-]+)="([^"]+)"')
# A language's own name, for the links to the guide in the other languages.
NATIVE = re.compile(r'(<a\b[^>]*?\sdata-native="([^"]+)"[^>]*>)(</a>)')
# A number, written the English way ("2500+" -> "2,500+"); the script writes it the way of the language in use.
NUMBER = re.compile(r'(<([a-z]+)\b[^>]*?\sdata-number="(\d+)([^"]*)"[^>]*>)(</\2>)')
# The keys the script asks for by name (the screen names, the keys of the device, the status line).
SCRIPT_KEY = re.compile(r'"(site\.[a-z0-9.-]+)"')


def flatten(table, prefix=""):
    out = {}
    for key, value in table.items():
        if isinstance(value, dict):
            out.update(flatten(value, f"{prefix}{key}."))
        else:
            out[f"{prefix}{key}"] = value
    return out


def catalogs(folder):
    """[(code, native, words)] of tools/lang/site.<code>.toml, English first: it is the page's default."""
    found = []
    for name in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
        if not (name.startswith("site.") and name.endswith(".toml")):
            continue
        with open(os.path.join(folder, name), "rb") as f:
            data = tomllib.load(f)
        found.append((data["meta"]["code"], data["meta"]["native"], flatten(data.get("strings", {}))))
    return sorted(found, key=lambda entry: (entry[0] != DEFAULT, entry[0]))


def used_keys(page):
    """Every key the page uses: in the markup's attributes and by name in its script."""
    return sorted(set(re.findall(r'\sdata-i18n(?:-[a-z-]+)?="([^"]+)"', page)) | set(SCRIPT_KEY.findall(page)))


def render(page, languages):
    """The page with the English words written in and every language embedded."""
    words = languages[0][2]
    natives = {code: native for code, native, _ in languages}
    page = TEXT_KEY.sub(lambda m: m.group(1) + html.escape(words[m.group(3)], quote=False) + m.group(4), page)
    page = ATTRIBUTE_KEY.sub(lambda m: f'{m.group(0)} {m.group(1)}="{html.escape(words[m.group(2)])}"', page)
    page = NATIVE.sub(lambda m: m.group(1) + html.escape(natives.get(m.group(2), m.group(2))) + m.group(3), page)
    page = NUMBER.sub(lambda m: m.group(1) + f"{int(m.group(3)):,}" + html.escape(m.group(4)) + m.group(5), page)
    payload = {
        "languages": [{"code": code, "native": native} for code, native, _ in languages],
        "words": {code: {k: v for k, v in table.items() if k.startswith("site.")} for code, _, table in languages},
    }
    # "</" cannot end the script element early inside JSON text.
    return page.replace(DATA, json.dumps(payload, ensure_ascii=False).replace("</", "<\\/"))


def assemble(site, demo, version, out, lang):
    if not os.path.isfile(os.path.join(demo, "graph.json")):
        return f"make_site: no demo at {demo}; build it with `python3 scripts/make_demo.py`"
    with open(os.path.join(site, "index.html"), encoding="utf-8") as f:
        page = f.read()
    languages = catalogs(lang)
    if not languages or languages[0][0] != DEFAULT:
        return f"make_site: no {os.path.join(lang, 'site.en.toml')}: English is the page's default and the reference of its words"
    if page.count(DATA) != 1:
        return f"make_site: {os.path.join(site, 'index.html')} must hold {DATA} once, for the words of every language"
    missing = [f"site.{code}.toml lacks {key}" for code, _, words in languages for key in used_keys(page) if key not in words]
    if missing:
        return "make_site: " + "; ".join(missing)
    if os.path.isdir(out):
        shutil.rmtree(out)
    shutil.copytree(site, out)
    with open(os.path.join(out, "index.html"), "w", encoding="utf-8") as f:
        f.write(render(page, languages))
    shutil.copytree(demo, os.path.join(out, "demo"))
    with open(version, encoding="utf-8") as f:
        text = f.read().strip()
    with open(os.path.join(out, "version.txt"), "w", encoding="utf-8") as f:
        f.write(text + "\n")
    missing = [name for name in NEEDED if not os.path.isfile(os.path.join(out, name))]
    if missing:
        return "make_site: missing in the result: " + ", ".join(missing)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--site", default=os.path.join(ROOT, "site"))
    parser.add_argument("--demo", default=os.path.join(ROOT, "out", "demo"))
    parser.add_argument("--version", default=os.path.join(ROOT, "VERSION"))
    parser.add_argument("--lang", default=os.path.join(ROOT, "tools", "lang"))
    parser.add_argument("--out", default=os.path.join(ROOT, "out", "site"))
    args = parser.parse_args()
    error = assemble(args.site, args.demo, args.version, args.out, args.lang)
    if error:
        print(error, file=sys.stderr)
        return 1
    print(f"make_site: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
