#!/usr/bin/env python3
"""Tests for scripts/check_i18n.py: the two sets of catalogs (lang/ for the device, lang/web/ for the page).

Run with:  python3 test/test_check_i18n/test_check.py

Each case copies the repository (without its build output) to a scratch directory, damages one thing
and runs the copied checker, so that what is tested is what the checker says about a real tree:
  1. the tree as it is passes both checks;
  2. a key of the device set that only web code uses is reported by --usage, and accepted once device code uses it;
  3. a key of the web set that only device code uses is reported by --usage, and accepted once the page generator uses it;
  4. a key in both sets fails the check;
  5. a key missing in one language of the web set fails the check;
  6. a language with a device catalog and no web catalog fails the check;
  7. lang/en.toml or lang/web/en.toml over the limit fails the check, and each file is judged alone;
  8. the sizes of every file are printed;
  9. a sentence written as a literal in the interface (src/ui/) is reported by --usage; a sentence in a comment or in an
     assertion message is not;
 10. a web catalog written in ASCII transliteration (accents spelled out or dropped) is reported, the same one with its
     accents is not.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PASSED = FAILED = 0


def check(name, condition, detail=""):
    global PASSED, FAILED
    if condition:
        print(f"  PASS  {name}")
        PASSED += 1
    else:
        print(f"  FAIL  {name}" + (f": {detail}" if detail else ""))
        FAILED += 1


def lit(*parts):
    """A key as the code writes it, built from parts so that this file is not itself a user of the probe keys."""
    return '"' + ".".join(parts) + '"'


class Tree:
    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="check_i18n_")
        self.root = os.path.join(self.dir, "repo")
        shutil.copytree(REPO, self.root, symlinks=True,
                        ignore=shutil.ignore_patterns(".git", ".pio", "out", "node_modules", "__pycache__", "golden"))

    def path(self, rel):
        return os.path.join(self.root, rel)

    def read(self, rel):
        with open(self.path(rel), encoding="utf-8") as f:
            return f.read()

    def write(self, rel, text):
        with open(self.path(rel), "w", encoding="utf-8") as f:
            f.write(text)

    def line(self, rel, key):
        return next(l for l in self.read(rel).split("\n") if l.startswith(key + " ") or l.startswith(key + "="))

    def move_key(self, key, source, target):
        """Moves the line of `key` from one catalog file to another, word for word."""
        line = self.line(source, key)
        self.write(source, self.read(source).replace(line + "\n", ""))
        self.write(target, self.read(target).rstrip("\n") + "\n" + line + "\n")

    def run(self, *args):
        done = subprocess.run([sys.executable, self.path("scripts/check_i18n.py"), *args], capture_output=True, text=True)
        return done.returncode, done.stdout + done.stderr

    def close(self):
        shutil.rmtree(self.dir, ignore_errors=True)


def main():
    tree = Tree()
    try:
        code, out = tree.run()
        check("the tree passes the check", code == 0, out)
        code, out = tree.run("--usage")
        check("the tree passes the usage check", code == 0, out)
        out = tree.run()[1]
        check("the size of every file is printed", all(name in out for name in ("lang/en.toml", "lang/web/en.toml", "lang/web/fr.toml")), out)
    finally:
        tree.close()

    # 2. a word of the page in the device set: a new key, in the device files, that only web code mentions
    tree = Tree()
    try:
        for c in ("en", "de", "fr"):
            tree.write(f"lang/{c}.toml", tree.read(f"lang/{c}.toml").rstrip("\n") + '\nprobe.page = "Probe"\n')
        tree.write("src/web/WebApi.cpp", tree.read("src/web/WebApi.cpp") + '\n// ' + lit("probe", "page") + '\n')
        code, out = tree.run()
        check("a probe key in the device set keeps the structure valid", code == 0, out)
        code, out = tree.run("--usage")
        check("--usage reports a key of the device set that only web code uses",
              code == 1 and "`probe.page`" in out and "only web code uses it" in out, out)
        tree.write("src/web/WebApi.cpp", tree.read("src/web/WebApi.cpp").replace("// " + lit("probe", "page"), ""))
        tree.write("src/app/AppCore.cpp", tree.read("src/app/AppCore.cpp") + '\n// ' + lit("probe", "page") + '\n')
        code, out = tree.run("--usage")
        check("--usage accepts the key once device code uses it", code == 0, out)
    finally:
        tree.close()

    # 3. a word of the device in the web set: a new key, in the web files, that only device code mentions
    tree = Tree()
    try:
        for c in ("en", "de", "fr"):
            tree.write(f"lang/web/{c}.toml", tree.read(f"lang/web/{c}.toml").rstrip("\n") + '\nweb.probe = "Probe"\n')
        tree.write("src/app/AppCore.cpp", tree.read("src/app/AppCore.cpp") + '\n// ' + lit("web", "probe") + '\n')
        code, out = tree.run("--usage")
        check("--usage reports a key of the web set that only device code uses",
              code == 1 and "`web.probe`" in out and "only device code uses it" in out, out)
        tree.write("scripts/gen_web.py", tree.read("scripts/gen_web.py") + '\n# ' + lit("web", "probe") + '\n')
        code, out = tree.run("--usage")
        check("--usage accepts the key once the page generator uses it", code == 0, out)
    finally:
        tree.close()

    # 9. a sentence in a literal of the interface
    tree = Tree()
    try:
        probe = "src/ui/screens/ProbeScreen.cpp"
        tree.write(probe, '// "Hello there" is only a comment\nstatic_assert(true, "this one is for developers");\nconst char* const kA = "ok";\n')
        code, out = tree.run("--usage")
        check("--usage accepts a sentence in a comment and an assertion message", code == 0, out)
        tree.write(probe, 'const char* const kWords = "Hello there";\n')
        code, out = tree.run("--usage")
        check("--usage reports a sentence in a string literal of the interface",
              code == 1 and "ProbeScreen.cpp:1: a sentence in a string literal" in out, out)
    finally:
        tree.close()

    # 10. a page written in ASCII transliteration
    tree = Tree()
    try:
        original = tree.read("lang/web/de.toml")
        plain = original.replace("\u00e4", "ae").replace("\u00f6", "oe").replace("\u00fc", "ue").replace("\u00c4", "Ae").replace("\u00d6", "Oe").replace("\u00dc", "Ue").replace("\u00df", "ss")
        tree.write("lang/web/de.toml", plain)
        code, out = tree.run()
        check("a web catalog in ASCII transliteration is reported",
              code == 1 and "lang/web/de.toml: only" in out and "ASCII transliteration" in out, out)
        tree.write("lang/web/de.toml", original)
        code, out = tree.run()
        check("the same catalog with its umlauts passes", code == 0, out)
    finally:
        tree.close()

    # 4. a key in both sets
    tree = Tree()
    try:
        line = tree.line("lang/web/en.toml", "web.tab.log")
        tree.write("lang/en.toml", tree.read("lang/en.toml").rstrip("\n") + "\n" + line + "\n")
        code, out = tree.run()
        check("a key in both sets fails the check", code == 1 and "one of the two sets only" in out, out)
    finally:
        tree.close()

    # 5. a key missing in one language of the web set
    tree = Tree()
    try:
        line = tree.line("lang/web/fr.toml", "web.tab.log")
        tree.write("lang/web/fr.toml", tree.read("lang/web/fr.toml").replace(line + "\n", ""))
        code, out = tree.run()
        check("a key missing in a web catalog fails the check", code == 1 and "lang/web/fr.toml: missing `web.tab.log`" in out, out)
    finally:
        tree.close()

    # 6. a language without a web catalog
    tree = Tree()
    try:
        os.remove(tree.path("lang/web/de.toml"))
        code, out = tree.run()
        check("a language without a web catalog fails the check", code == 1 and "lang/web/de.toml is missing" in out, out)
    finally:
        tree.close()

    # 7. the limit, per file
    for rel in ("lang/en.toml", "lang/web/en.toml"):
        tree = Tree()
        try:
            tree.write(rel, tree.read(rel) + "# " + "x" * 33000 + "\n")
            code, out = tree.run()
            check(f"{rel} over the limit fails the check", code == 1 and f"{rel} is" in out and "over the 32768" in out, out)
        finally:
            tree.close()
    tree = Tree()
    try:
        # Together the two English files are far over the limit of one, which is not what the limit is about.
        total = sum(os.path.getsize(tree.path(f)) for f in ("lang/en.toml", "lang/web/en.toml"))
        limit = int(re.search(r"kMaxCatalogBytes\s*=\s*(\d+)", tree.read("src/config/ConfigLoader.h")).group(1))
        tree.write("src/config/ConfigLoader.h", tree.read("src/config/ConfigLoader.h").replace(str(limit), str(total - 1), 1))
        code, out = tree.run()
        check("the limit applies to each file, not to the two together", code == 0, out)
    finally:
        tree.close()

    print(f"\n{PASSED} passed, {FAILED} failed")
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
