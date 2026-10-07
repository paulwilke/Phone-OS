#!/usr/bin/env python3
"""Finds leftovers ("ghosts") in documentation and code. (allow-non-english: holds the German patterns it searches for.)

    scripts/check_docs.py            # from the repository root
    scripts/check_docs.py --root <dir>   # another tree (the tests of this script use it)

Checks every Markdown file and every source file:
  * relative links resolve to files that exist;
  * `paths/in/backticks` that look like repository files exist;
  * no placeholder markers (TODO, FIXME, XXX, TBD, "for now", "legacy", "deprecated",
    "temporary", "decision to confirm") — open work belongs in docs/open-points.md, not scattered over the files;
  * VERSION holds one `major.minor.patch` line, the newest heading of CHANGELOG.md names that version, and
    docs/release-<major>.<minor>.md exists and names it, so that a version is never raised without its notes;
  * every screen name of src/ui/ScreenIds.h has a docs/screen-<name>.md and every such document names a
    screen, and Runtime.cpp registers every screen name (the documents describe pages the product has);
  * every Markdown file under docs/ is linked from docs/README.md, so that no document is a stranger, and
    docs/open-points.md exists: open work is listed in that one place and nowhere else;
  * the user's guide and its translations have the same headings in the same order (level for level), so
    that a section added to one is added to all;
  * the text is English: no umlauts or typical German words. Files under `lang/`, under
    `docs/<language-code>/` (translated documentation) and files that carry the marker
    `allow-non-english` in their first lines (test data, examples of other languages) are exempt.

Exits with 1 on any finding.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if len(sys.argv) == 3 and sys.argv[1] == "--root":
    ROOT = os.path.abspath(sys.argv[2])
SKIP_DIRS = {".git", ".pio", "node_modules", "_workbench", "__pycache__", ".publish", ".claude", "out"}
CODE_SUFFIXES = (".h", ".hpp", ".c", ".cpp", ".py", ".sh", ".ini", ".yml", ".yaml", ".js", ".html", ".toml")
MARKER = re.compile(r"\b(TODO|FIXME|XXX|TBD)\b|\b(for now|legacy|deprecated|temporary|decision to confirm)\b", re.I)
GERMAN = re.compile(r"[äöüÄÖÜß]|\b(und|nicht|oder|ist|sind|wird|eine?|der|die|das|für|mit|auch|noch)\b")
LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
TICK = re.compile(r"`([A-Za-z0-9_./-]+/[A-Za-z0-9_.-]+\.[A-Za-z0-9]+)`")


def exempt_language(rel):
    parts = rel.split(os.sep)
    return parts[0] == "lang" or (len(parts) > 1 and parts[0] == "docs" and re.fullmatch(r"[a-z]{2}(-[A-Z]{2})?", parts[1]) is not None)


def files():
    for base, dirs, names in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for n in names:
            if n.endswith(".md") or n.endswith(CODE_SUFFIXES):
                yield os.path.join(base, n)


def version_problem():
    try:
        text = open(os.path.join(ROOT, "VERSION"), encoding="utf-8").read()
    except OSError:
        return "VERSION: file is missing"
    if not re.fullmatch(r"\d+\.\d+\.\d+\n?", text):
        return f"VERSION: expected `major.minor.patch`, found {text.strip()!r}"
    return None


def release_problems():
    """The version, the changelog and the release notes agree. Skipped while VERSION itself is broken."""
    try:
        version = open(os.path.join(ROOT, "VERSION"), encoding="utf-8").read().strip()
    except OSError:
        return []
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        return []
    problems = []
    try:
        changelog = open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8").read()
    except OSError:
        problems.append("CHANGELOG.md: the changelog does not exist")
    else:
        newest = re.search(r"^## (\S+)", changelog, re.M)
        if not newest or newest.group(1) != version:
            problems.append(f"CHANGELOG.md: the newest heading is {newest.group(1) if newest else 'missing'}, VERSION says {version}")
    notes = "docs/release-" + ".".join(version.split(".")[:2]) + ".md"
    try:
        text = open(os.path.join(ROOT, notes), encoding="utf-8").read()
    except OSError:
        problems.append(f"{notes}: the release notes of VERSION {version} do not exist")
    else:
        if version not in text:
            problems.append(f"{notes}: does not name the version {version}")
    return problems


def screen_problems():
    """The screen names, their documents and their registration in the runtime agree."""
    try:
        ids = open(os.path.join(ROOT, "src", "ui", "ScreenIds.h"), encoding="utf-8").read()
        runtime = open(os.path.join(ROOT, "src", "boot", "Runtime.cpp"), encoding="utf-8").read()
    except OSError as error:
        return [f"{error.filename}: cannot be read"]
    names = [n for n in re.findall(r"^\s*k([A-Z][A-Za-z]*),", ids, re.M) if n != "Count"]
    problems = []
    documented = {f[len("screen-"):-3] for f in os.listdir(os.path.join(ROOT, "docs")) if re.fullmatch(r"screen-[a-z]+\.md", f)}
    for name in names:
        if name.lower() not in documented:
            problems.append(f"src/ui/ScreenIds.h: screen k{name} has no docs/screen-{name.lower()}.md")
        if f"stack_.add(ui::screen::k{name}," not in runtime:
            problems.append(f"src/boot/Runtime.cpp: screen k{name} is not registered")
    for doc in sorted(documented - {n.lower() for n in names}):
        problems.append(f"docs/screen-{doc}.md: no screen of that name in src/ui/ScreenIds.h")
    return problems


GUIDES = [os.path.join("docs", "user-guide.md"), os.path.join("docs", "de", "benutzerhandbuch.md"),
          os.path.join("docs", "fr", "guide-utilisateur.md")]


def headings(path):
    """The heading levels of a Markdown file in order, outside code fences: the structure a translation keeps."""
    levels, fenced = [], False
    for line in open(path, encoding="utf-8").read().split("\n"):
        if line.startswith("```"):
            fenced = not fenced
        elif not fenced:
            m = re.match(r"(#{1,6}) \S", line)
            if m:
                levels.append(len(m.group(1)))
    return levels


def guide_problems():
    """The three guides tell the same things in the same order: same number of headings, same levels."""
    problems = []
    reference = headings(os.path.join(ROOT, GUIDES[0])) if os.path.exists(os.path.join(ROOT, GUIDES[0])) else None
    if reference is None:
        return [f"{GUIDES[0]}: the user's guide is missing"]
    for rel in GUIDES[1:]:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            problems.append(f"{rel}: the translation of the user's guide is missing")
            continue
        levels = headings(path)
        if levels != reference:
            first = next((i for i, (a, b) in enumerate(zip(levels, reference)) if a != b), min(len(levels), len(reference)))
            problems.append(f"{rel}: headings differ from {GUIDES[0]}: {len(levels)} against {len(reference)}, first difference at heading {first + 1}")
    return problems


def index_problems():
    """docs/README.md names every document of docs/, and the one list of open work is there."""
    index = os.path.join(ROOT, "docs", "README.md")
    if not os.path.exists(index):
        return ["docs/README.md: the index of the documents does not exist"]
    with open(index, encoding="utf-8") as f:
        linked = {os.path.normpath(os.path.join("docs", t.split("#")[0])) for t in LINK.findall(f.read())}
    problems = []
    for base, dirs, names in os.walk(os.path.join(ROOT, "docs")):
        for n in sorted(names):
            rel = os.path.relpath(os.path.join(base, n), ROOT)
            if n.endswith(".md") and rel != os.path.join("docs", "README.md") and rel not in linked:
                problems.append(f"{rel}: not linked from docs/README.md")
    if not os.path.exists(os.path.join(ROOT, "docs", "open-points.md")):
        problems.append("docs/open-points.md: the list of open work does not exist")
    return problems


def main():
    problems = screen_problems() + guide_problems() + index_problems() + release_problems()
    if version_problem():
        problems.append(version_problem())
    for path in files():
        rel = os.path.relpath(path, ROOT)
        try:
            lines = open(path, encoding="utf-8").read().split("\n")
        except UnicodeDecodeError:
            continue
        allow_foreign = any("allow-non-english" in l for l in lines[:6]) or exempt_language(rel)
        for n, line in enumerate(lines, 1):
            if rel != os.path.join("scripts", "check_docs.py") and MARKER.search(line):
                problems.append(f"{rel}:{n}: placeholder marker: {MARKER.search(line).group(0)}")
            if not allow_foreign and GERMAN.search(line):
                problems.append(f"{rel}:{n}: not English: {GERMAN.search(line).group(0)!r}")
            if path.endswith(".md"):
                for target in LINK.findall(line):
                    if re.match(r"(https?:|mailto:|#)", target):
                        continue
                    t = target.split("#")[0]
                    if t and not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), t))):
                        problems.append(f"{rel}:{n}: broken link: {target}")
                for target in TICK.findall(line):
                    if target.startswith(("http", "~", "/")) or "*" in target or "<" in target:
                        continue
                    # out/ is build output (the demo, the simulator's card): a clean checkout has none of it.
                    if target.startswith("out/"):
                        continue
                    if not (os.path.exists(os.path.join(ROOT, target)) or os.path.exists(os.path.join(os.path.dirname(path), target))):
                        problems.append(f"{rel}:{n}: refers to a file that does not exist: {target}")
    for p in problems[:300]:
        print(p)
    if problems:
        print(f"\n{len(problems)} findings.")
        return 1
    print("check_docs: no findings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
