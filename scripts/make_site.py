#!/usr/bin/env python3
"""Assembles the project's web page (GitHub Pages) from its parts.

    pio run -e sim
    python3 scripts/make_demo.py            # the pictures and the click-through demo, out/demo/
    python3 scripts/make_site.py            # site/ + tools/lang/site.*.toml + out/demo/ + VERSION -> out/site/ (not committed)
    python3 scripts/make_site.py --url https://example.org/   # the address the pages are known by (default: the GitHub Pages one)

The page in site/ has no pictures and no words of its own. The device on it is the click-through demo, whose
pictures the simulator draws on every build (docs/process/demo.md). Its words are the `site.*` keys of the host-only
catalogs tools/lang/site.<code>.toml: every element that says something names its key (data-i18n="site.hero.title",
or data-i18n-alt, data-i18n-aria-label, data-i18n-content for an attribute), and the script names the others.

One page is rendered per language, each with its own words written in: English at out/site/index.html (the default,
at the root of the address), and one folder per other catalog, out/site/de/index.html, out/site/fr/index.html and so
on. A reader without a script and a link preview see the page in the language of its address; every page names its
canonical address and an hreflang link to each language's page, with x-default for the English one.

The result is checked so that a run that "built" a page without its pictures or its words fails: the exit status is 1
when the demo is missing, when a language lacks a word the page uses (the key is named), when the page lacks one of
its markers, or when a file the page needs is not there.
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

DEFAULT = "en"
DEFAULT_URL = "https://paulwilke.github.io/Phone-OS/"
# Where the words of the page's language go; the page holds it once, in its data script.
DATA = "__DATA__"
# Where the links to the page's own address and to its other languages go, once, in the head.
ALTERNATES = '<meta name="site-alternates">'
# The page's language and its root: "" for the English page at the root, "../" in a language's folder.
HTML_TAG = '<html lang="en" data-root="">'

# An element whose words are its text: it is empty in site/, and its key names what goes in.
TEXT_KEY = re.compile(r'(<([a-zA-Z][a-zA-Z0-9]*)\b[^>]*?\sdata-i18n="([^"]+)"[^>]*>)(</\2>)')
# An attribute with words in it: data-i18n-alt="key" becomes alt="...".
ATTRIBUTE_KEY = re.compile(r'\sdata-i18n-([a-z-]+)="([^"]+)"')
# A language's own name, for the links to the guide in the other languages.
NATIVE = re.compile(r'(<a\b[^>]*?\sdata-native="([^"]+)"[^>]*>)(</a>)')
# A number, written the way of the page's language ("2500+" -> "2,500+" in English, "2.500+" in German, "2 500+" in
# French with a narrow no-break space); the script writes the same with toLocaleString.
NUMBER = re.compile(r'(<([a-z]+)\b[^>]*?\sdata-number="(\d+)([^"]*)"[^>]*>)(</\2>)')
GROUPING = {"de": ".", "fr": "\u202f"}
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


def needed(languages):
    """What the result must hold: the page and its version, the demo the page fetches, and a page per language."""
    codes = [code for code, _, _ in languages]
    files = ["index.html", "version.txt", "demo/index.html", "demo/graph.json"]
    files += [f"demo/{code}/n0.png" for code in codes]
    files += [f"{code}/index.html" for code in codes if code != DEFAULT]
    return files


def address(base, code):
    """The address of a language's page: the root for the default, a folder for each other language."""
    return base if code == DEFAULT else base + code + "/"


def head(base, languages, code):
    """The links of a page's head: its canonical address, every language's page, x-default, and the picture a preview shows."""
    lines = [f'<link rel="canonical" href="{html.escape(address(base, code))}">']
    lines += [f'<link rel="alternate" hreflang="{other}" href="{html.escape(address(base, other))}">' for other, _, _ in languages]
    lines.append(f'<link rel="alternate" hreflang="x-default" href="{html.escape(address(base, DEFAULT))}">')
    lines.append(f'<meta property="og:image" content="{html.escape(base)}demo/{code}/n0.png">')
    return "\n".join(lines)


def render(page, languages, code, base):
    """The page of one language: its words written in, its language and root, its links, and its words embedded."""
    words = next(table for other, _, table in languages if other == code)
    natives = {other: native for other, native, _ in languages}
    root = "" if code == DEFAULT else "../"
    # The pictures written into the markup are the English ones; a language's page shows its own before any script runs.
    page = page.replace(f'src="demo/{DEFAULT}/', f'src="demo/{code}/')
    if code != DEFAULT:
        # The full demo opens in the page's language (the script does the same), and the pictures and the demo sit in the
        # root's folder: a page one folder down reaches them with ../.
        page = page.replace('href="demo/index.html"', f'href="demo/index.html?lang={code}"')
        page = page.replace('src="demo/', 'src="../demo/').replace('href="demo/', 'href="../demo/')
    page = page.replace(HTML_TAG, f'<html lang="{code}" data-root="{root}">')
    page = page.replace(ALTERNATES, head(base, languages, code))
    page = TEXT_KEY.sub(lambda m: m.group(1) + html.escape(words[m.group(3)], quote=False) + m.group(4), page)
    page = ATTRIBUTE_KEY.sub(lambda m: f'{m.group(0)} {m.group(1)}="{html.escape(words[m.group(2)])}"', page)
    page = NATIVE.sub(lambda m: m.group(1) + html.escape(natives.get(m.group(2), m.group(2))) + m.group(3), page)
    page = NUMBER.sub(lambda m: m.group(1) + f"{int(m.group(3)):,}".replace(",", GROUPING.get(code, ",")) + html.escape(m.group(4)) + m.group(5), page)
    payload = {
        "languages": [{"code": other, "native": native} for other, native, _ in languages],
        "words": {code: {k: v for k, v in words.items() if k.startswith("site.")}},
    }
    # "</" cannot end the script element early inside JSON text.
    return page.replace(DATA, json.dumps(payload, ensure_ascii=False).replace("</", "<\\/"))


def assemble(site, demo, version, out, lang, url):
    if not os.path.isfile(os.path.join(demo, "graph.json")):
        return f"make_site: no demo at {demo}; build it with `python3 scripts/make_demo.py`"
    if not url.startswith(("https://", "http://")):
        return f"make_site: --url must start with https:// (got {url!r}): the links of the pages are absolute"
    base = url if url.endswith("/") else url + "/"
    with open(os.path.join(site, "index.html"), encoding="utf-8") as f:
        page = f.read()
    languages = catalogs(lang)
    if not languages or languages[0][0] != DEFAULT:
        return f"make_site: no {os.path.join(lang, 'site.en.toml')}: English is the page's default and the reference of its words"
    for marker, what in ((DATA, f"{DATA}, for the words of its language"),
                         (ALTERNATES, f"{ALTERNATES}, where its links to its other languages go"),
                         (HTML_TAG, f"{HTML_TAG}, so that each language's page sets its own language and root")):
        if page.count(marker) != 1:
            return f"make_site: {os.path.join(site, 'index.html')} must hold {what}"
    missing = [f"site.{code}.toml lacks {key}" for code, _, words in languages for key in used_keys(page) if key not in words]
    if missing:
        return "make_site: " + "; ".join(missing)
    if os.path.isdir(out):
        shutil.rmtree(out)
    shutil.copytree(site, out)
    for code, _, _ in languages:
        target = os.path.join(out, "index.html") if code == DEFAULT else os.path.join(out, code, "index.html")
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "w", encoding="utf-8") as f:
            f.write(render(page, languages, code, base))
    shutil.copytree(demo, os.path.join(out, "demo"))
    with open(version, encoding="utf-8") as f:
        text = f.read().strip()
    with open(os.path.join(out, "version.txt"), "w", encoding="utf-8") as f:
        f.write(text + "\n")
    missing = [name for name in needed(languages) if not os.path.isfile(os.path.join(out, name))]
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
    parser.add_argument("--url", default=DEFAULT_URL, help="the address the root of the page has (canonical and hreflang links)")
    args = parser.parse_args()
    error = assemble(args.site, args.demo, args.version, args.out, args.lang, args.url)
    if error:
        print(error, file=sys.stderr)
        return 1
    print(f"make_site: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
