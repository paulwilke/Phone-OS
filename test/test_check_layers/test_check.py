#!/usr/bin/env python3
"""Counter-checks of scripts/check_layers.py: every rule about device/ and src/board/ must fire on a tree that
breaks it and stay silent on one that does not. Runs the script on small trees built on the fly (--root).

    python3 test/test_check_layers/test_check.py
"""
import os
import subprocess
import sys
import tempfile

SCRIPT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "scripts", "check_layers.py")


def run(files):
    with tempfile.TemporaryDirectory() as root:
        for rel, text in files.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        p = subprocess.run([sys.executable, SCRIPT, "--root", root], capture_output=True, text=True)
        return p.returncode, p.stdout


GOOD = {
    "src/hal/Gpio.h": "#pragma once\n",
    "src/board/Pins.h": '#include "../hal/Gpio.h"\n',
    "device/main.cpp": '#include <Arduino.h>\n#include "board/Pins.h"\n',
}

CASES = [
    ("a clean tree passes", GOOD, 0, ""),
    ("device may include Arduino and src", GOOD, 0, ""),
    ("src includes device", {**GOOD, "src/board/Bad.h": '#include "../../device/Esp32Gpio.h"\n'}, 1, "includes device/"),
    ("src names Arduino", {**GOOD, "src/board/Bad.h": "int x = Arduino;\n"}, 1, "platform dependency"),
    ("board includes app", {**GOOD, "src/app/A.h": "", "src/board/Bad.h": '#include "../app/A.h"\n'}, 1, "board may include only"),
    ("board includes ui", {**GOOD, "src/ui/U.h": "", "src/board/Bad.h": '#include "../ui/U.h"\n'}, 1, "board may include only"),
    ("board includes the input event of the ui", {**GOOD, "src/ui/Input.h": "", "src/board/Ok.h": '#include "../ui/Input.h"\n'}, 0, ""),
    ("board includes the size of the panel", {**GOOD, "src/ui/Display.h": "", "src/board/Ok2.h": '#include "../ui/Display.h"\n'}, 0, ""),
    ("board includes a screen header", {**GOOD, "src/ui/Screen.h": "", "src/board/Bad.h": '#include "../ui/Screen.h"\n'}, 1, "board may include only"),
    ("board includes a driver", {**GOOD, "src/drivers/D.h": "", "src/board/Ok.h": '#include "../drivers/D.h"\n'}, 0, ""),
    ("a driver includes board", {**GOOD, "src/drivers/D.h": '#include "../board/Pins.h"\n'}, 1, "driver may include only"),
    ("device includes sim", {**GOOD, "device/Bad.h": '#include "../sim/Simulator.h"\n'}, 1, "not the firmware's"),
    ("device includes a fake", {**GOOD, "device/Bad.h": '#include "../testing/fakes/FakeGpio.h"\n'}, 1, "not the firmware's"),
    ("device includes MeshCore", {**GOOD, "device/Bad.h": '#include "MeshCore.h"\n'}, 1, "MeshCore"),
]


def main():
    failed = 0
    for name, files, code, needle in CASES:
        got, out = run(files)
        ok = got == code and needle in out
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failed += 1
            print(out)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
