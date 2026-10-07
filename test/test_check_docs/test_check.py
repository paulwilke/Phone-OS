#!/usr/bin/env python3
"""Counter-checks of the guide rule in scripts/check_docs.py: the user's guide and its two translations must have the
same headings in the same order, and the check must say so when one does not. Runs the script on small trees built
on the fly (--root); the other rules of the script report on those trees too, so the cases look only for the
guide's own findings.

    python3 test/test_check_docs/test_check.py
"""
import os
import subprocess
import sys
import tempfile

SCRIPT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "scripts", "check_docs.py")

EN = "# Guide\n\n## One\n\ntext\n\n### Sub\n\n```\n# not a heading\n```\n\n## Two\n"
DE = "# Handbuch\n\n## Eins\n\n### Unter\n\n## Zwei\n"
FR = "# Guide\n\n## Un\n\n### Sous\n\n## Deux\n"


def run(en, de, fr):
    with tempfile.TemporaryDirectory() as root:
        for rel, text in (("docs/user-guide.md", en), ("docs/de/benutzerhandbuch.md", de), ("docs/fr/guide-utilisateur.md", fr)):
            if text is None:
                continue
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        p = subprocess.run([sys.executable, SCRIPT, "--root", root], capture_output=True, text=True)
        return [l for l in p.stdout.split("\n") if "guide" in l and ("headings" in l or "missing" in l)]


CASES = [
    ("the same structure passes (a # in a code fence is no heading)", (EN, DE, FR), []),
    ("a section missing in German is found", (EN, "# Handbuch\n\n## Eins\n\n### Unter\n", FR), ["benutzerhandbuch.md: headings differ"]),
    ("a section of another level in French is found", (EN, DE, FR.replace("### Sous", "## Sous")), ["guide-utilisateur.md: headings differ"]),
    ("an extra section in English is found in both", (EN + "\n## Three\n", DE, FR), ["benutzerhandbuch.md: headings differ", "guide-utilisateur.md: headings differ"]),
    ("a missing translation is found", (EN, None, FR), ["benutzerhandbuch.md: the translation of the user's guide is missing"]),
    ("a missing guide is found", (None, DE, FR), ["docs/user-guide.md: the user's guide is missing"]),
]


def main():
    failed = 0
    for name, files, needles in CASES:
        got = run(*files)
        ok = len(got) == len(needles) and all(any(n in g for g in got) for n in needles)
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failed += 1
            print("\n".join(got))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
