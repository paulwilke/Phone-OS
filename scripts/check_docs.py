#!/usr/bin/env python3
"""Finds leftovers ("ghosts") in documentation and code. (allow-non-english: holds the German patterns it searches for.)

    scripts/check_docs.py            # from the repository root

Checks every Markdown file and every source file:
  * relative links resolve to files that exist;
  * `paths/in/backticks` that look like repository files exist;
  * no placeholder markers (TODO, FIXME, XXX, TBD, "for now", "legacy", "deprecated",
    "temporary") — open work belongs in the issue tracker, not in the files;
  * the text is English: no umlauts or typical German words. Files under `lang/`, under
    `docs/<language-code>/` (translated documentation) and files that carry the marker
    `allow-non-english` in their first lines (test data, examples of other languages) are exempt.

Exits with 1 on any finding.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {".git", ".pio", "node_modules", "_workbench", "__pycache__", ".publish"}
CODE_SUFFIXES = (".h", ".hpp", ".c", ".cpp", ".py", ".sh", ".ini", ".yml", ".yaml", ".js", ".html", ".toml")
MARKER = re.compile(r"\b(TODO|FIXME|XXX|TBD)\b|\b(for now|legacy|deprecated|temporary)\b", re.I)
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


def main():
    problems = []
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
