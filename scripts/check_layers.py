#!/usr/bin/env python3
"""Checks that the source layers stay pure and point the right way.

    scripts/check_layers.py          # from the repository root

Rules for everything under src/:
  * only C++ standard headers are included with angle brackets (no `<Arduino.h>`, no
    `<something/else.h>`): the logic is plain C++17 and builds on the host;
  * no mention of Arduino or a platform SDK in the code.
Rules for the layers:
  * src/hal/ (the capability interfaces) includes only standard headers and its own
    siblings, never src/app/ or src/i18n/: an interface does not know its users;
  * src/i18n/ includes nothing from the other layers.
src/app/ may include src/hal/ and src/i18n/.

Exits with 1 on any finding.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
PLATFORM = re.compile(r"\b(Arduino|ESP32|ESP-IDF|FreeRTOS)\b|esp_|WiFi\.h")


def files():
    for base, _, names in os.walk(os.path.join(ROOT, "src")):
        for n in names:
            if n.endswith((".h", ".cpp")):
                yield os.path.join(base, n)


def layer(rel):
    parts = rel.split(os.sep)
    return parts[1] if len(parts) > 2 else ""


def main():
    problems = []
    for path in files():
        rel = os.path.relpath(path, ROOT)
        where = layer(rel)
        for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
            m = INCLUDE.match(line)
            if m:
                kind, target = m.groups()
                if kind == "<" and (target.endswith(".h") or "/" in target):
                    problems.append(f"{rel}:{n}: platform or project header in angle brackets: <{target}>")
                if kind == '"':
                    normal = os.path.normpath(os.path.join(os.path.dirname(path), target))
                    inside = os.path.relpath(normal, os.path.join(ROOT, "src")).split(os.sep)[0]
                    if where == "hal" and inside != "hal":
                        problems.append(f"{rel}:{n}: an interface includes the layer {inside}/: {target}")
                    if where == "i18n" and inside != "i18n":
                        problems.append(f"{rel}:{n}: i18n includes the layer {inside}/: {target}")
            elif PLATFORM.search(line) and not line.lstrip().startswith(("//", "*")):
                problems.append(f"{rel}:{n}: platform dependency: {PLATFORM.search(line).group(0)}")
    for p in problems:
        print(p)
    if problems:
        print(f"\n{len(problems)} findings.")
        return 1
    print("check_layers: no findings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
