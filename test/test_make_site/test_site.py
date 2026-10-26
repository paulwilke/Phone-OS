#!/usr/bin/env python3
"""Checks scripts/make_site.py on small trees built on the fly (allow-non-english: German and French words as test data): the page, the demo and the version land in the
result; one page per language, each with its own words written in and only its own words embedded, the language in
its address (the English page at the root, the others in a folder) and the links to all of them; a run without the
demo fails and says so; a language that lacks a word of the page fails and the key is named; a page without its
markers fails and the marker is named; a result in which a file the page needs is missing fails. The demo's graph is one
over every language, so each language starts on a state of its own (a picture `demo/<code>/n<start>.png`): the page
shows its language's, the preview too, and a graph without a start for a language fails.

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
URL = "https://example.test/site/"


def write(path, text=""):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


# A page with a word as text, one in an attribute, one the script asks for by name, a number, a language link, and
# the markers make_site.py replaces: the root's language and data-root, the links of the head, the data of the words.
PAGE = """<!doctype html>
<html lang="en" data-root="">
<head><title data-i18n="site.meta.title"></title>
<meta name="site-alternates"></head>
<body><img src="demo/en/n0.png" data-picture="n0" data-i18n-alt="site.does.phone.alt"><h1 data-i18n="site.hero.title"></h1><b data-number="2500+"></b>
<a data-native="de"></a> <a href="demo/index.html" data-demo></a>
<script id="data" type="application/json">__DATA__</script><script>const status = "site.device.loading";</script></body></html>
"""
WORDS = {
    "en": ("English", {"meta.title": "Page", "does.phone.alt": "A picture", "hero.title": "Phone & more", "device.loading": "Ready"}),
    "de": ("Deutsch", {"meta.title": "Seite", "does.phone.alt": "Ein Bild", "hero.title": "Telefon und mehr", "device.loading": "Bereit"}),
    "fr": ("Français", {"meta.title": "Page", "does.phone.alt": "Une image", "hero.title": "Téléphone et plus", "device.loading": "Prêt"}),
}


def catalog(code, native, words):
    lines = [f'[meta]\ncode = "{code}"\nname = "{code}"\nnative = "{native}"\nplural = "one-other"\n\n[strings]']
    lines += [f'site.{key} = "{text}"' for key, text in words.items()]
    return "\n".join(lines) + "\n"


# The state each language's device starts in: one graph over every language, so the numbers are not all 0.
STARTS = {"en": 0, "de": 1, "fr": 2}


def make_tree(root, with_demo=True, with_fr=True, words=WORDS, page=PAGE, graph=None):
    write(os.path.join(root, "site", "index.html"), page)
    for code, (native, table) in words.items():
        write(os.path.join(root, "lang", f"site.{code}.toml"), catalog(code, native, table))
    write(os.path.join(root, "lang", "demo.en.toml"), catalog("en", "English", {}).replace("site.", "demo."))
    write(os.path.join(root, "VERSION"), "9.9.9\n")
    if with_demo:
        write(os.path.join(root, "demo", "index.html"), "<!doctype html>")
        write(os.path.join(root, "demo", "graph.json"), json.dumps({"starts": STARTS} if graph is None else graph))
        for code in ("en", "de") + (("fr",) if with_fr else ()):
            write(os.path.join(root, "demo", code, f"n{STARTS[code]}.png"), "png")


def run(root, url=URL):
    args = [sys.executable, SCRIPT, "--site", os.path.join(root, "site"), "--demo", os.path.join(root, "demo"),
            "--version", os.path.join(root, "VERSION"), "--lang", os.path.join(root, "lang"),
            "--out", os.path.join(root, "out")]
    if url is not None:
        args += ["--url", url]
    return subprocess.run(args, capture_output=True, text=True)


def embedded(page):
    return json.loads(re.search(r'<script id="data" type="application/json">(.*?)</script>', page, re.S).group(1))


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
    for name in ("index.html", "de/index.html", "fr/index.html", "version.txt", "demo/index.html", "demo/fr/n2.png"):
        check(os.path.isfile(os.path.join(root, "out", name)), f"{name} is in the result")
    with open(os.path.join(root, "out", "version.txt"), encoding="utf-8") as f:
        check(f.read() == "9.9.9\n", "version.txt carries VERSION")
    pages = {}
    for code, name in (("en", "index.html"), ("de", "de/index.html"), ("fr", "fr/index.html")):
        with open(os.path.join(root, "out", name), encoding="utf-8") as f:
            pages[code] = f.read()
    en, de, fr = pages["en"], pages["de"], pages["fr"]
    check(all("__DATA__" not in page and 'name="site-alternates"' not in page for page in pages.values()),
          "no placeholder is left in any page")

    # The English page: the default, in place, its words only.
    check('<html lang="en" data-root="">' in en, "the English page is at the root, with data-root empty")
    check('<img src="demo/en/n0.png" data-picture="n0"' in en and "../demo/" not in en, "the English page's paths are unchanged")
    check('href="demo/index.html?lang=en"' in en, "the English page's link to the full demo names English too: the demo would follow the browser without it")
    check('<title data-i18n="site.meta.title">Page</title>' in en and '<h1 data-i18n="site.hero.title">Phone &amp; more</h1>' in en,
          "the English words are written in, escaped")
    check('<b data-number="2500+">2,500+</b>' in en and '<a data-native="de">Deutsch</a>' in en,
          "a number is written the English way, a language is named by its own name")
    check(list(embedded(en)["words"]) == ["en"], "the English page embeds only the English words")
    check([l["code"] for l in embedded(en)["languages"]] == ["en", "de", "fr"], "every language is listed, English first: the default")

    # The German page: in its folder, with its own words, and its paths one folder down.
    check('<html lang="de" data-root="../">' in de, "the German page is in de/, with lang de and data-root ../")
    check('<title data-i18n="site.meta.title">Seite</title>' in de and '<h1 data-i18n="site.hero.title">Telefon und mehr</h1>' in de,
          "the German words are written into the German page")
    check('<img src="../demo/de/n1.png" data-picture="n1"' in de and "demo/en/" not in de.split("<script")[0],
          "the German page reaches its own pictures one folder down, the state its language starts in: src=\"../demo/de/n1.png\"")
    check('<b data-number="2500+">2.500+</b>' in de, "a number is written the German way into the German page")
    check('href="../demo/index.html?lang=de"' in de, "the German page's link to the full demo carries its language")
    check('<a data-native="de">Deutsch</a>' in de, "the language names are the same on every page")
    check(list(embedded(de)["words"]) == ["de"] and embedded(de)["words"]["de"]["site.hero.title"] == "Telefon und mehr",
          "the German page embeds only the German words")

    # The French page carries its own words too.
    check('<html lang="fr" data-root="../">' in fr and '<h1 data-i18n="site.hero.title">Téléphone et plus</h1>' in fr,
          "the French page is in fr/, with its own words")

    # The links of the head: canonical, every language, x-default, and the preview picture, with the address given.
    check('<link rel="canonical" href="https://example.test/site/de/">' in de, "the German page's canonical address is its own")
    check('<link rel="canonical" href="https://example.test/site/">' in en, "the English page's canonical address is the root")
    for page, name in ((en, "English"), (de, "German"), (fr, "French")):
        check('<link rel="alternate" hreflang="de" href="https://example.test/site/de/">' in page
              and '<link rel="alternate" hreflang="fr" href="https://example.test/site/fr/">' in page
              and '<link rel="alternate" hreflang="en" href="https://example.test/site/">' in page,
              f"the {name} page links to the English, German and French pages")
        check('<link rel="alternate" hreflang="x-default" href="https://example.test/site/">' in page,
              f"the {name} page's x-default is the English page")
    check('<meta property="og:image" content="https://example.test/site/demo/de/n1.png">' in de,
          "the German page's preview picture is absolute, in German and the one its device starts on")

    write(os.path.join(root, "out", "stale.txt"), "old")
    run(root)
    check(not os.path.exists(os.path.join(root, "out", "stale.txt")), "a second run starts from an empty result")

    p = run(root, url=None)
    with open(os.path.join(root, "out", "de", "index.html"), encoding="utf-8") as f:
        check('<link rel="canonical" href="https://paulwilke.github.io/Phone-OS/de/">' in f.read(),
              "without --url the links name the GitHub Pages address")

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
    check(p.returncode == 1 and "demo/fr/n2.png" in p.stderr, "a language without the picture it starts on fails and is named")

with tempfile.TemporaryDirectory() as root:
    make_tree(root, graph={"nodes": []})
    p = run(root)
    check(p.returncode == 1 and "starts" in p.stderr and not os.path.exists(os.path.join(root, "out")),
          "a graph without starts (the walk of an older version) fails and says to build it again: " + p.stderr.strip())

with tempfile.TemporaryDirectory() as root:
    make_tree(root, graph={"starts": {"en": 0, "de": 1}})
    p = run(root)
    check(p.returncode == 1 and "no start state in fr" in p.stderr, "a language the graph does not start in fails and is named: " + p.stderr.strip())

with tempfile.TemporaryDirectory() as root:
    make_tree(root, page=PAGE.replace('<meta name="site-alternates">', ""))
    p = run(root)
    check(p.returncode == 1 and "site-alternates" in p.stderr, "a page without the alternates placeholder fails and names it: " + p.stderr.strip())
    check(not os.path.exists(os.path.join(root, "out")), "and writes no result")

with tempfile.TemporaryDirectory() as root:
    make_tree(root, page=PAGE.replace('<html lang="en" data-root="">', '<html lang="en">'))
    p = run(root)
    check(p.returncode == 1 and "data-root" in p.stderr, "a page whose html tag lacks data-root fails and names it")

with tempfile.TemporaryDirectory() as root:
    make_tree(root)
    p = run(root, url="ftp://example.test/")
    check(p.returncode == 1 and "--url" in p.stderr, "an address that is not http or https fails and names --url")

print(f"{'OK' if failures == 0 else 'FAILED'}: {failures} failure(s)")
sys.exit(1 if failures else 0)
