#!/usr/bin/env python3
"""Checks docs/app-behaviour.md, the inventory of the application core's behaviour, against the tests.

    scripts/check_behaviour.py             # a row marked `done` must have its test
    scripts/check_behaviour.py --complete  # and no row may be left `open`

Every table row names the test that pins the behaviour. A row is `open` until the test exists
and passes and `done` afterwards; this check refuses a `done` row whose test function is not in
the test directory the section names, so a row cannot be ticked without a test. It also refuses
a test name used twice, a row without a test and a status that is neither word. Exits with 1 on
any finding.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "docs", "app-behaviour.md")
TESTS_LINE = re.compile(r"tests in `test/([a-z0-9_]+)/`", re.I)
ROW = re.compile(r"^\| ([A-Z]{3}-\d+) \| .* \| `(test_[a-z0-9_]+)` \| .* \| (\w+) \|$")


def main():
    complete = "--complete" in sys.argv[1:]
    problems = []
    directory = None
    seen = {}
    ids = set()
    rows = done = 0
    sources = {}
    with open(DOC, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if line.startswith("## "):
                directory = None
            m = TESTS_LINE.search(line)
            if m:
                directory = m.group(1)
            if not re.match(r"^\| [A-Z]{3}-\d+ \|", line):
                continue
            row = ROW.match(line)
            if not row:
                problems.append(f"{DOC}:{number}: a row without a test or a status: {line[:60]}")
                continue
            rid, test, status = row.groups()
            rows += 1
            if rid in ids:
                problems.append(f"{DOC}:{number}: id {rid} twice")
            ids.add(rid)
            key = (directory, test)
            if key in seen:
                problems.append(f"{DOC}:{number}: the test {test} is used twice (also line {seen[key]})")
            seen[key] = number
            if status not in ("open", "done"):
                problems.append(f"{DOC}:{number}: {rid}: status is `{status}`, not open or done")
            elif status == "open" and complete:
                problems.append(f"{DOC}:{number}: {rid} is still open")
            elif status == "done":
                done += 1
                path = os.path.join(ROOT, "test", directory or "", "test_main.cpp")
                if directory not in sources:
                    sources[directory] = open(path, encoding="utf-8").read() if os.path.exists(path) else None
                text = sources[directory]
                if text is None:
                    problems.append(f"{DOC}:{number}: {rid} is done but test/{directory}/test_main.cpp does not exist")
                elif not re.search(r"\bvoid\s+" + re.escape(test) + r"\s*\(", text):
                    problems.append(f"{DOC}:{number}: {rid} is done but {test} is not in test/{directory}/test_main.cpp")
    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} findings.")
        return 1
    print(f"check_behaviour: {rows} rows, {done} done" + (", none open" if complete else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
