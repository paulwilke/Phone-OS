#!/usr/bin/env python3
"""Counter-checks of five rules of scripts/check_docs.py: the user's guide and its two translations must have the
same headings in the same order, every document of docs/ must be linked from docs/README.md (with the list of open
work, docs/open-points.md, among them), VERSION, the newest heading of CHANGELOG.md and the release notes
docs/release-<major>.<minor>.md must agree, every Python file must compile, and a Python or Node test must be in
test_ignore. The check must say so when a rule is broken. Runs the script on small trees built on the fly (--root);
the other rules of the script report on those trees too, so the cases look only for the findings of the rule they test.

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


def run_index(files):
    with tempfile.TemporaryDirectory() as root:
        for rel, text in files.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        p = subprocess.run([sys.executable, SCRIPT, "--root", root], capture_output=True, text=True)
        return [l for l in p.stdout.split("\n") if "not linked" in l or "open-points" in l or "index of the documents" in l]


INDEX_OK = {"docs/README.md": "[a](a.md) [open](open-points.md)\n", "docs/a.md": "# A\n", "docs/open-points.md": "# Open\n"}

INDEX_CASES = [
    ("a document linked from the index passes", INDEX_OK, []),
    ("a document the index does not link is found", {**INDEX_OK, "docs/stranger.md": "# S\n"}, ["docs/stranger.md: not linked from docs/README.md"]),
    ("a translated document the index does not link is found", {**INDEX_OK, "docs/de/stranger.md": "# S\n"}, ["stranger.md: not linked from docs/README.md"]),
    ("a missing list of open work is found", {"docs/README.md": "[a](a.md)\n", "docs/a.md": "# A\n"}, ["docs/open-points.md: the list of open work does not exist"]),
    ("a missing index is found", {"docs/a.md": "# A\n", "docs/open-points.md": "# Open\n"}, ["docs/README.md: the index of the documents does not exist"]),
]


def run_release(files):
    with tempfile.TemporaryDirectory() as root:
        for rel, text in files.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        p = subprocess.run([sys.executable, SCRIPT, "--root", root], capture_output=True, text=True)
        return [l for l in p.stdout.split("\n") if l.startswith(("CHANGELOG.md", "docs/release-"))]


RELEASE_OK = {"VERSION": "4.0.0\n", "CHANGELOG.md": "# Changelog\n\n## 4.0.0 - the release\n\ntext\n\n## 3.2\n",
              "docs/release-4.0.md": "# Release 4.0\n\nVersion 4.0.0.\n"}

RELEASE_CASES = [
    ("a version with its changelog heading and release notes passes", RELEASE_OK, []),
    ("a version that the changelog does not name first is found", {**RELEASE_OK, "VERSION": "4.0.1\n", "docs/release-4.0.md": "4.0.1\n"},
     ["CHANGELOG.md: the newest heading is 4.0.0, VERSION says 4.0.1"]),
    ("a missing changelog is found", {k: v for k, v in RELEASE_OK.items() if k != "CHANGELOG.md"}, ["CHANGELOG.md: the changelog does not exist"]),
    ("a changelog without a release heading is found", {**RELEASE_OK, "CHANGELOG.md": "# Changelog\n"},
     ["CHANGELOG.md: the newest heading is missing"]),
    ("missing release notes are found", {k: v for k, v in RELEASE_OK.items() if k != "docs/release-4.0.md"},
     ["docs/release-4.0.md: the release notes of VERSION 4.0.0 do not exist"]),
    ("release notes of another version are found", {**RELEASE_OK, "docs/release-4.0.md": "# Release 4.0\n\nVersion 4.0.9.\n"},
     ["docs/release-4.0.md: does not name the version 4.0.0"]),
    ("a patch release is read against the notes of its minor version",
     {"VERSION": "4.0.1\n", "CHANGELOG.md": "## 4.0.1\n", "docs/release-4.0.md": "4.0.1\n"}, []),
]


def run_python(files):
    with tempfile.TemporaryDirectory() as root:
        for rel, text in files.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        p = subprocess.run([sys.executable, SCRIPT, "--root", root], capture_output=True, text=True)
        return [l for l in p.stdout.split("\n") if "does not compile" in l]


PYTHON_CASES = [
    ("a script that compiles passes", {"scripts/ok.py": "import os\nprint(os.name)\n"}, []),
    ("a script that does not compile is found", {"scripts/broken.py": "import os\nimport tomli_w if False else None\n"},
     ["scripts/broken.py:2: does not compile"]),
    ("a broken test script is found too", {"test/x/test.py": "def f(:\n"}, ["test/x/test.py:1: does not compile"]),
]


def run_ignore(files):
    with tempfile.TemporaryDirectory() as root:
        for rel, text in files.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        p = subprocess.run([sys.executable, SCRIPT, "--root", root], capture_output=True, text=True)
        return [l for l in p.stdout.split("\n") if "test_ignore" in l]


INI = "[env:native]\ntest_ignore =\n  test_tool\n\n[env:native-asan]\ntest_ignore =\n  test_tool\n\n[env:sim]\nplatform = native\n"
INI_NO_ASAN = "[env:native]\ntest_ignore =\n  test_tool\n\n[env:native-asan]\ntest_ignore =\n  test_other\n"
TREE = {"test/test_tool/test.py": "print(1)\n", "test/test_cpp/test_main.cpp": "int x;\n"}

IGNORE_CASES = [
    ("a Python test listed in both host environments passes", {**TREE, "platformio.ini": INI}, []),
    ("a Python test missing from the sanitizer environment is found", {**TREE, "platformio.ini": INI_NO_ASAN},
     ["test/test_tool has no C++ test and is not in test_ignore of env:native-asan"]),
    ("a Python test that is listed nowhere is found twice", {**TREE, "platformio.ini": "[env:native]\nx = 1\n\n[env:native-asan]\nx = 1\n"},
     ["env:native\n", "env:native-asan"]),
    ("a C++ test needs no entry", {"test/test_cpp/test_main.cpp": "int x;\n", "platformio.ini": INI}, []),
]


def main():
    failed = 0
    for name, files, needles in IGNORE_CASES:
        got = [g + "\n" for g in run_ignore(files)]
        ok = len(got) == len(needles) and all(any(n in g for g in got) for n in needles)
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failed += 1
            print("".join(got))
    for name, files, needles in PYTHON_CASES:
        got = run_python(files)
        ok = len(got) == len(needles) and all(any(n in g for g in got) for n in needles)
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failed += 1
            print("\n".join(got))
    for name, files, needles in RELEASE_CASES:
        got = run_release(files)
        ok = len(got) == len(needles) and all(any(n in g for g in got) for n in needles)
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failed += 1
            print("\n".join(got))
    for name, files, needles in INDEX_CASES:
        got = run_index(files)
        ok = len(got) == len(needles) and all(any(n in g for g in got) for n in needles)
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failed += 1
            print("\n".join(got))
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
