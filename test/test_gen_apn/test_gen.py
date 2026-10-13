#!/usr/bin/env python3
"""Tests for scripts/gen_apn_table.py.

Run with:  python3 test/test_gen_apn/test_gen.py

  1. Which entries of a small file are taken: the first plain default entry of each operator, not a test network, not
     a virtual operator, not a bearer for other things, not an APN with a character that does not belong in one.
  2. The list is sorted by code and by the number of MNC digits, and a two-digit and a three-digit code are two operators.
  3. --check passes on what the generator wrote and says what is wrong with an unsorted list, a duplicate, a bad APN,
     a count that does not match, or a file in another format.
  4. --check with --source fails when the stored list is not what the file gives.
  5. The list stored in the repository is well formed.
"""
import hashlib
import os
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "scripts"))
import gen_apn_table  # noqa: E402

SCRIPT = os.path.join(REPO, "scripts", "gen_apn_table.py")
SAMPLE = """<?xml version="1.0" encoding="UTF-8"?>
<apns version="8">
  <apn carrier="Test network" mcc="001" mnc="01" apn="phone" type="default"/>
  <apn carrier="Not a bearer for data" mcc="262" mnc="01" apn="ims" type="ims"/>
  <apn carrier="Virtual" mcc="262" mnc="01" apn="virtual.example" type="default" mvno_type="spn" mvno_match_data="x"/>
  <apn carrier="First plain" mcc="262" mnc="01" apn="first.example" type="default,supl,ia"/>
  <apn carrier="Second plain" mcc="262" mnc="01" apn="second.example" type="default"/>
  <apn carrier="Bad characters" mcc="262" mnc="02" apn="bad apn" type="default"/>
  <apn carrier="Empty" mcc="262" mnc="03" apn="" type="default"/>
  <apn carrier="Two digits" mcc="338" mnc="05" apn="two.example" type="default"/>
  <apn carrier="Three digits" mcc="338" mnc="050" apn="three.example" type="default"/>
  <apn carrier="Earlier code" mcc="208" mnc="01" apn="orange" type="default"/>
  <apn carrier="No code" apn="nocode" type="default"/>
  <apn carrier="Long code" mcc="2620" mnc="01" apn="long" type="default"/>
  <apn carrier="Spaced type" mcc="310" mnc="260" apn="fast.example" type="mms, default"/>
</apns>
"""

passed = failed = 0


def check(name, condition, detail=""):
    global passed, failed
    if condition:
        print(f"  PASS  {name}")
        passed += 1
    else:
        print(f"  FAIL  {name}" + (f": {detail}" if detail else ""))
        failed += 1


def run(*args):
    return subprocess.run([sys.executable, "-I", SCRIPT, *args], capture_output=True, text=True)


with tempfile.TemporaryDirectory() as tmp:
    source = os.path.join(tmp, "apns.xml")
    out = os.path.join(tmp, "ApnTable.cpp")
    with open(source, "w", encoding="utf-8") as f:
        f.write(SAMPLE)

    rows = gen_apn_table.entries_from(source)
    check("the first plain default entry of an operator is taken; the rest are not",
          rows == [(208, 1, 2, "orange"), (262, 1, 2, "first.example"), (310, 260, 3, "fast.example"),
                   (338, 5, 2, "two.example"), (338, 50, 3, "three.example")], str(rows))
    check("a two-digit and a three-digit code are two operators, sorted by code and digits",
          (338, 5, 2, "two.example") in rows and (338, 50, 3, "three.example") in rows and rows == sorted(rows))

    result = run("--source", source, "--out", out)
    check("the generator writes the list", result.returncode == 0 and os.path.exists(out), result.stderr)
    text = open(out, encoding="utf-8").read()
    check("the file names its source, its licence and the hash", "Apache License, Version 2.0" in text and
          hashlib.sha256(SAMPLE.encode()).hexdigest() in text and "android.googlesource.com" in text)
    check("--check passes on what was written", run("--check", "--out", out).returncode == 0)
    check("--check with the same source passes", run("--check", "--source", source, "--out", out).returncode == 0)

    with open(source, "w", encoding="utf-8") as f:
        f.write(SAMPLE.replace("first.example", "other.example"))
    changed = run("--check", "--source", source, "--out", out)
    check("--check with another source says the stored list is not what the file gives",
          changed.returncode == 1 and "not what the source file gives" in changed.stderr, changed.stderr)

    def damaged(name, replace):
        path = os.path.join(tmp, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write(replace(text))
        return run("--check", "--out", path)

    check("an unsorted list is found", damaged("unsorted.cpp", lambda t: t.replace('{208, 1, 2, "orange"},\n    {262, 1, 2, "first.example"},',
                                                                                    '{262, 1, 2, "first.example"},\n    {208, 1, 2, "orange"},')).returncode == 1)
    check("an operator twice is found", damaged("twice.cpp", lambda t: t.replace('{208, 1, 2, "orange"},', '{208, 1, 2, "orange"},\n    {208, 1, 2, "orange"},')).returncode == 1)
    check("an APN with a space is found", damaged("space.cpp", lambda t: t.replace("orange", "or ange")).returncode == 1)
    check("a count that does not match is found", damaged("count.cpp", lambda t: t.replace("kTableSize = 5;", "kTableSize = 6;")).returncode == 1)
    check("a file in another format is found", damaged("format.cpp", lambda t: "int main() {}\n").returncode == 1)
    check("a missing file is found", run("--check", "--out", os.path.join(tmp, "nothing.cpp")).returncode == 1)

stored = open(os.path.join(REPO, "src", "drivers", "ApnTable.cpp"), encoding="utf-8").read()
problems = gen_apn_table.check_stored(stored)
check("the list stored in the repository is well formed", not problems, "; ".join(problems))
check("it has the operators of Germany", all(any(r[0] == 262 and r[1] == m for r in gen_apn_table.parse_stored(stored)[0]) for m in (1, 2, 3)))

print(f"\n{passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
