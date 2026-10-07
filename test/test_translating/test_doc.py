#!/usr/bin/env python3
"""Follows docs/translating.md step by step: a new language `xx` is made from the English files, put on a card,
loaded in the simulator, switched to, and its words are seen. Then it breaks each step on purpose and expects the
simulator to say so the way the document promises (the counter-checks), so that neither the document nor the
simulator can drift apart from the other without this test failing.

    pio run -e sim && python3 test/test_translating/test_doc.py

The commands of the document's shell block are run as written (only the two names in them are replaced by paths in a
scratch directory); the console commands and the log lines the document names must stand in the document and in the
simulator's answer.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROGRAM = os.path.join(REPO, ".pio", "build", "sim", "program")
DOC = os.path.join(REPO, "docs", "translating.md")
PASSED = FAILED = 0


def check(name, condition, detail=""):
    global PASSED, FAILED
    if condition:
        print(f"  PASS  {name}")
        PASSED += 1
    else:
        print(f"  FAIL  {name}" + (f": {detail}" if detail else ""))
        FAILED += 1


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def translated(english, word_key, word):
    """The English file as a new language: another code and name, one word changed (the document: change [meta], translate)."""
    text = english.replace('code = "en"', 'code = "xx"', 1).replace('name = "English"', 'name = "Testish"', 1)
    text = text.replace('native = "English"', 'native = "Xtest"', 1)
    text, n = re.subn(rf'^{re.escape(word_key)} = "[^"]*"', f'{word_key} = "{word}"', text, count=1, flags=re.M)
    assert n == 1, word_key
    return text


def make_card(root, doc_block, device_catalog, page_catalog, interface_language="xx"):
    """Step 2 of the document: the shell block as written, then the translation and the setting."""
    card = os.path.join(root, "my-card")
    translation = os.path.join(root, "my-translation.toml")
    write(translation, device_catalog)
    commands = doc_block.replace("my-card", card).replace("my-translation.toml", translation)
    done = subprocess.run(commands, shell=True, cwd=REPO, capture_output=True, text=True)
    assert done.returncode == 0, done.stderr
    settings = os.path.join(card, "config", "settings.toml")
    text = read(settings)
    # "In the copy of settings.toml set language = "xx" under [interface]": the first language line is that one.
    text, n = re.subn(r'(\[interface\]\s*\nlanguage = )"[a-z-]+"', rf'\1"{interface_language}"', text, count=1)
    assert n == 1
    write(settings, text)
    if page_catalog is not None:
        write(os.path.join(card, "lang", "web", "xx.toml"), page_catalog)
    return card


def run(card, root, script):
    """Step 3: `.pio/build/sim/program --data my-card`, with a script instead of the prompt."""
    path = os.path.join(root, "script.txt")
    write(path, script)
    done = subprocess.run([PROGRAM, "--data", card, "--work", os.path.join(root, "work"), "--script", path],
                          capture_output=True, text=True, timeout=120)
    return done.returncode, done.stdout + done.stderr


def main():
    if not os.path.exists(PROGRAM):
        print("the simulator is not built: run `pio run -e sim` first")
        return 1
    doc = read(DOC)
    blocks = re.findall(r"```sh\n(.*?)```", doc, re.S)
    block = next((b for b in blocks if "my-card" in b), None)
    check("the document has the shell block that makes the card", block is not None)
    if block is None:
        return 1
    # What the steps of the document say must stand there, so that this test follows the document and not a copy of it.
    for phrase in ("language = \"xx\"", ".pio/build/sim/program --data my-card", "`language xx`", "`language en`", "`see home.phone`",
                   "`wifi ap`", "http://127.0.0.1:8080/", "test-password-1", "config: oversized-file /lang/xx.toml",
                   "config: user-catalog-error", "config: bad-catalog-code", "`log`", "32,768 bytes"):
        check(f"the document names {phrase}", phrase in doc)

    english = read(os.path.join(REPO, "lang", "en.toml"))
    page = read(os.path.join(REPO, "lang", "web", "en.toml"))
    device_xx = translated(english, "home.phone", "Xphone")
    page_xx = translated(page, "web.tab.status", "Xstatus")
    follow = "advance 5s\ncheck language xx\nsee home.phone\nsay Xphone\nsay-not Phone\nlanguage en\nsay Phone\nlanguage xx\nsay Xphone\n"

    with tempfile.TemporaryDirectory() as root:
        # Steps 2 to 4: the language is the device's, its word is on the screen, and switching goes both ways.
        card = make_card(root, block, device_xx, page_xx)
        code, out = run(card, root, follow + "log 50\n")
        check("a language made from the English files is loaded and its word is seen", code == 0, out)
        check("a catalog without a problem leaves no complaint in the log",
              "user-catalog-error" not in out and "oversized-file" not in out and "bad-catalog-code" not in out, out)
        # Step 5: the page, through the real socket, offers the language.
        code, out = run(card, root, "advance 5s\nwifi ap\nweb get /api/languages\nweb get /api/lang/xx\nweb get /api/lang/zz\n")
        check("the page answers for the new language", "web: 200 GET /api/lang/xx" in out and "web: 200 GET /api/languages" in out, out)
        check("the page does not answer for a language nobody has", "web: 404 GET /api/lang/zz" in out, out)

    # Counter-checks: each step broken, and the document's promise about it must hold.
    with tempfile.TemporaryDirectory() as root:
        card = make_card(root, block, device_xx, None, interface_language="en")
        code, out = run(card, root, follow)
        check("counter: without the setting the card does not make the device speak the language",
              code != 0 and "language is en, expected xx" in out, out)
    with tempfile.TemporaryDirectory() as root:
        card = make_card(root, block, device_xx.replace('{name}: busy', '{nmae}: busy', 1), None)
        code, out = run(card, root, "advance 5s\nlog 50\n")
        check("counter: a {placeholder} that English does not have is named in the log as the document says",
              "config: user-catalog-error" in out, out)
        code, out = run(card, root, "advance 5s\nlanguage xx\n")
        check("counter: such a catalog does not become a language", code != 0, out)
    with tempfile.TemporaryDirectory() as root:
        card = make_card(root, block, device_xx + "\n# padding\n" * 4000, None)
        code, out = run(card, root, "advance 5s\nlog 50\n")
        check("counter: a catalog over the limit is named in the log as the document says",
              "config: oversized-file /lang/xx.toml" in out, out)
    with tempfile.TemporaryDirectory() as root:
        card = make_card(root, block, device_xx, None)
        shutil.rmtree(os.path.join(card, "lang"))
        code, out = run(card, root, "advance 5s\nlog 50\n")
        check("counter: a configuration that asks for a language nobody has is named in the log as the document says",
              "config: bad-catalog-code" in out, out)

    print(f"\n{PASSED} passed, {FAILED} failed")
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
