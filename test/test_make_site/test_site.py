#!/usr/bin/env python3
"""Checks scripts/make_site.py on small trees built on the fly (allow-non-english: German and French words as test data): the page, the demo and the version land in the
result; the page carries the words of every language and the English ones written in, and no placeholder; a run
without the demo fails and says so; a language that lacks a word of the page fails and the key is named; a result in
which a file the page needs is missing fails.

    python3 test/test_make_site/test_site.py
"""
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, "scripts", "make_site.py")


def write(path, text=""):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


# A page with a word as text, one in an attribute, one the script asks for by name, and a number.
PAGE = """<!doctype html><title data-i18n="site.meta.title"></title>
<img src="demo/en/n0.png" data-i18n-alt="site.does.phone.alt"><h1 data-i18n="site.hero.title"></h1><b data-number="2500+"></b>
<a data-native="de"></a>
<script id="data" type="application/json">__DATA__</script><script>const status = "site.device.loading";</script>"""
WORDS = {
    "en": ("English", {"meta.title": "Page", "does.phone.alt": "A picture", "hero.title": "Phone & more", "device.loading": "Ready"}),
    "de": ("Deutsch", {"meta.title": "Seite", "does.phone.alt": "Ein Bild", "hero.title": "Telefon und mehr", "device.loading": "Bereit"}),
    "fr": ("Français", {"meta.title": "Page", "does.phone.alt": "Une image", "hero.title": "Téléphone et plus", "device.loading": "Prêt"}),
}


def catalog(code, native, words):
    lines = [f'[meta]\ncode = "{code}"\nname = "{code}"\nnative = "{native}"\nplural = "one-other"\n\n[strings]']
    lines += [f'site.{key} = "{text}"' for key, text in words.items()]
    return "\n".join(lines) + "\n"


def make_tree(root, with_demo=True, with_fr=True, words=WORDS):
    write(os.path.join(root, "site", "index.html"), PAGE)
    for code, (native, table) in words.items():
        write(os.path.join(root, "lang", f"site.{code}.toml"), catalog(code, native, table))
    write(os.path.join(root, "lang", "demo.en.toml"), catalog("en", "English", {}).replace("site.", "demo."))
    write(os.path.join(root, "VERSION"), "9.9.9\n")
    if with_demo:
        write(os.path.join(root, "demo", "index.html"), "<!doctype html>")
        write(os.path.join(root, "demo", "graph.json"), "{}")
        for code in ("en", "de") + (("fr",) if with_fr else ()):
            write(os.path.join(root, "demo", code, "n0.png"), "png")


def run(root):
    return subprocess.run([sys.executable, SCRIPT, "--site", os.path.join(root, "site"), "--demo", os.path.join(root, "demo"),
                           "--version", os.path.join(root, "VERSION"), "--lang", os.path.join(root, "lang"),
                           "--out", os.path.join(root, "out")],
                          capture_output=True, text=True)


failures = 0


def check(cond, what):
    global failures
    print(("  PASS: " if cond else "  FAIL: ") + what)
    if not cond:
        failures += 1


with tempfile.TemporaryDirectory() as root:
    make_tree(root)
    p = run(root)
    check(p.returncode == 0, "a complete tree assembles: " + p.stderr.strip())
    check(os.path.isfile(os.path.join(root, "out", "index.html")), "the page is in the result")
    check(os.path.isfile(os.path.join(root, "out", "demo", "fr", "n0.png")), "the demo is in the result, under demo/")
    with open(os.path.join(root, "out", "version.txt"), encoding="utf-8") as f:
        check(f.read() == "9.9.9\n", "version.txt carries VERSION")
    with open(os.path.join(root, "out", "index.html"), encoding="utf-8") as f:
        page = f.read()
    check("__DATA__" not in page and "__" not in page.replace("__DATA__", ""), "no placeholder is left in the page")
    data = json.loads(re.search(r'<script id="data" type="application/json">(.*?)</script>', page, re.S).group(1))
    check([l["code"] for l in data["languages"]] == ["en", "de", "fr"], "the languages are embedded, English first: the default")
    check(all(data["words"][code]["site.hero.title"] == table["hero.title"] and data["words"][code]["site.device.loading"] == table["device.loading"]
              for code, (_, table) in WORDS.items()), "the words of all three languages are embedded")
    check(not any(k.startswith("demo.") for words in data["words"].values() for k in words), "only the page's own set is embedded")
    check('<h1 data-i18n="site.hero.title">Phone &amp; more</h1>' in page, "the English words are written into the markup, escaped")
    check('data-i18n-alt="site.does.phone.alt" alt="A picture"' in page, "an attribute gets its English words")
    check('<title data-i18n="site.meta.title">Page</title>' in page, "the title too")
    check('<b data-number="2500+">2,500+</b>' in page, "a number is written the English way")
    check('<a data-native="de">Deutsch</a>' in page, "a language is named by its own name")
    write(os.path.join(root, "out", "stale.txt"), "old")
    run(root)
    check(not os.path.exists(os.path.join(root, "out", "stale.txt")), "a second run starts from an empty result")

with tempfile.TemporaryDirectory() as root:
    make_tree(root, with_demo=False)
    p = run(root)
    check(p.returncode == 1 and "no demo" in p.stderr, "without the demo the run fails and names make_demo.py")
    check(not os.path.exists(os.path.join(root, "out")), "and writes no result")

with tempfile.TemporaryDirectory() as root:
    words = dict(WORDS, de=("Deutsch", {k: v for k, v in WORDS["de"][1].items() if k != "device.loading"}))
    make_tree(root, words=words)
    p = run(root)
    check(p.returncode == 1 and "site.de.toml lacks site.device.loading" in p.stderr, "a word missing in one language fails and is named: " + p.stderr.strip())
    check(not os.path.exists(os.path.join(root, "out")), "and writes no result")

with tempfile.TemporaryDirectory() as root:
    make_tree(root, words={code: entry for code, entry in WORDS.items() if code != "en"})
    p = run(root)
    check(p.returncode == 1 and "site.en.toml" in p.stderr, "without the English words the run fails: English is the default")

with tempfile.TemporaryDirectory() as root:
    make_tree(root, with_fr=False)
    p = run(root)
    check(p.returncode == 1 and "demo/fr/n0.png" in p.stderr, "a language without pictures fails and is named")

print(f"{'OK' if failures == 0 else 'FAILED'}: {failures} failure(s)")
sys.exit(1 if failures else 0)
