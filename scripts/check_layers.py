#!/usr/bin/env python3
"""Checks that the source layers stay pure and point the right way.

    scripts/check_layers.py          # from the repository root
    scripts/check_layers.py --root <dir>   # another tree (the tests of this script use it)

Rules for everything under src/:
  * only C++ standard headers are included with angle brackets (no `<Arduino.h>`, no
    `<something/else.h>`): the logic is plain C++17 and builds on the host;
  * no mention of Arduino or a platform SDK in the code.
Rules for the layers:
  * src/hal/ (the capability interfaces) includes only standard headers and its own
    siblings, never src/app/ or src/i18n/: an interface does not know its users;
  * src/i18n/ includes nothing from the other layers;
  * src/app/ may include src/hal/ and src/i18n/;
  * src/config/ may include src/config/, src/app/, src/hal/ and src/i18n/ — no src/web/,
    no src/ui/: the configuration layer does not know the web or the display;
  * src/web/ may include src/web/, src/config/, src/app/, src/hal/ and src/i18n/ — no src/ui/:
    the web layer does not know the display;
  * src/drivers/ implements the interfaces of src/hal/: it may include src/drivers/ and src/hal/ and
    nothing else (no application, no UI, no configuration, no web, no catalogs), and no MeshCore. A
    platform header belongs behind the small transport interface of a driver, which the tests replace
    with a scripted fake, so the rule above (no platform names under src/) holds for drivers too;
  * src/board/ holds what the device's chips need and can be tested on the host: the pin map, the seams to
    the buses and pins (I2cBus, UartPort, Gpio, Delay), register decoding and start-up sequences. It may
    include src/board/, src/hal/ and src/drivers/ (a board class implements a capability or a transport
    of a driver) and, from src/ui/, only ui/Input.h (the plain data of an input event that the keyboard and the
    touch decoders produce) and ui/Display.h (the size of the panel the touch coordinates live in), and, from src/app/, only app/SafePath.h
    (the one check of every path from outside, which the card's store applies like every other store), and nothing else;
  * src/boot/ is the composition of the device (the runtime both the firmware and the simulator run): it
    may include every other layer of src/, and no other layer includes it;
  * nothing under src/ reaches outside src/: the firmware never includes sim/ or testing/ (the
    simulator is a platform that includes src/, not the other way round), and nothing under src/
    includes device/;
  * device/ is the firmware's platform code, the counterpart of sim/: it may include src/ and the
    Arduino and ESP32 headers, and never sim/ or testing/ (a fake in the firmware would hide a missing
    implementation);
  * src/ui/ includes src/ui/, src/i18n/ and, from src/app/, only app/AppCore.h: the facade and
    the plain data it hands out (the state, notices, the message history, the contact book), and
    app/ShakeDetector.h, a value class that counts shakes from samples. A
    screen never includes a hardware interface or any other part of the application directly,
    and never a board or a driver, so it can be drawn on any Display and tested without a device.

Everywhere (src/, sim/, testing/, test/): no include names MeshCore, whose code the project does not use.

Exits with 1 on any finding.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if len(sys.argv) == 3 and sys.argv[1] == "--root":
    ROOT = os.path.abspath(sys.argv[2])
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
PLATFORM = re.compile(r"\b(Arduino|ESP32|ESP-IDF|FreeRTOS)\b|\besp_|WiFi\.h")


def files():
    for base, _, names in os.walk(os.path.join(ROOT, "src")):
        for n in names:
            if n.endswith((".h", ".cpp")):
                yield os.path.join(base, n)


APP_FACADE = os.path.join(ROOT, "src", "app", "AppCore.h")
# The one other header of src/app/ a screen may use: a value class with no device behind it, which counts
# shakes from the samples a screen is given. The sensor itself reaches the screen through ui/MotionSource.h.
APP_PURE = os.path.join(ROOT, "src", "app", "ShakeDetector.h")


UI_INPUT = (os.path.join(ROOT, "src", "ui", "Input.h"), os.path.join(ROOT, "src", "ui", "Display.h"),
            os.path.join(ROOT, "src", "app", "SafePath.h"))


def resolve(path, target):
    """The file an include names: next to the including file first, then from src/ (the include
    path of the build). Resolving only the first way would read "i18n/Translator.h" in src/ui/
    as src/ui/i18n/Translator.h and judge the wrong layer."""
    beside = os.path.normpath(os.path.join(os.path.dirname(path), target))
    if os.path.exists(beside):
        return beside
    return os.path.normpath(os.path.join(ROOT, "src", target))


def layer(rel):
    parts = rel.split(os.sep)
    return parts[1] if len(parts) > 2 else ""


def other_files():
    for top in ("sim", "testing", "test", "device"):
        for base, _, names in os.walk(os.path.join(ROOT, top)):
            for n in names:
                if n.endswith((".h", ".cpp")):
                    yield os.path.join(base, n)


def device_files():
    for base, _, names in os.walk(os.path.join(ROOT, "device")):
        for n in names:
            if n.endswith((".h", ".cpp")):
                yield os.path.join(base, n)


def main():
    problems = []
    for path in device_files():
        rel = os.path.relpath(path, ROOT)
        for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
            m = INCLUDE.match(line)
            if m and m.group(1) == '"':
                target = m.group(2)
                if re.match(r"(\.\./)*(sim|testing)/", target):
                    problems.append(f"{rel}:{n}: the firmware includes {target}; sim/ and testing/ are not the firmware's")
    for path in list(files()) + list(other_files()):
        for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
            m = INCLUDE.match(line)
            if m and "meshcore" in m.group(2).lower():
                problems.append(f"{os.path.relpath(path, ROOT)}:{n}: MeshCore include: {m.group(2)}")
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
                    normal = resolve(path, target)
                    inside = os.path.relpath(normal, os.path.join(ROOT, "src")).split(os.sep)[0]
                    if inside == "..":
                        problems.append(f"{rel}:{n}: reaches outside src/ (sim/ and testing/ are not the firmware's): {target}")
                    if normal.startswith(os.path.join(ROOT, "device") + os.sep) or re.match(r"(\.\./)*device/", target):
                        problems.append(f"{rel}:{n}: src/ includes device/, the platform code that includes src/: {target}")
                    if where == "board" and inside not in ("board", "hal", "drivers") and normal not in UI_INPUT:
                        problems.append(f"{rel}:{n}: board may include only board/, hal/, drivers/, not {inside}/: {target}")
                    if inside == "boot" and where != "boot":
                        problems.append(f"{rel}:{n}: {where or 'src'}/ includes boot/, the composition that includes everything else: {target}")
                    if where == "hal" and inside != "hal":
                        problems.append(f"{rel}:{n}: an interface includes the layer {inside}/: {target}")
                    if where == "drivers" and inside not in ("drivers", "hal"):
                        problems.append(f"{rel}:{n}: a driver may include only drivers/ and hal/, not {inside}/: {target}")
                    if where == "i18n" and inside != "i18n":
                        problems.append(f"{rel}:{n}: i18n includes the layer {inside}/: {target}")
                    if where == "config" and inside not in ("config", "app", "hal", "i18n"):
                        problems.append(f"{rel}:{n}: config may include only config/, app/, hal/, i18n/, not {inside}/: {target}")
                    if where == "web" and inside not in ("web", "config", "app", "hal", "i18n"):
                        problems.append(f"{rel}:{n}: web may include only web/, config/, app/, hal/, i18n/, not {inside}/: {target}")
                    if where == "ui" and inside not in ("ui", "i18n") and normal not in (APP_FACADE, APP_PURE):
                        problems.append(f"{rel}:{n}: ui may include only ui/, i18n/, app/AppCore.h and app/ShakeDetector.h, not {inside}/: {target}")
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
