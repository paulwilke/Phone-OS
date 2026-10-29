#!/usr/bin/env python3
"""Scans the repository (its tracked files, a directory or its history) for personal data before publishing. It says nothing about what the device does with data.

    scripts/privacy_check.py                       # all files tracked by git
    scripts/privacy_check.py --dir <path>          # a directory (e.g. a clean clone about to be pushed)
    scripts/privacy_check.py --history             # also the whole git history (slow)
    scripts/privacy_check.py --repo <path>         # use another repository for the file list and --history
    scripts/privacy_check.py --denylist <file>     # a private list of things that must never appear
    scripts/privacy_check.py --selftest

Two sources of findings:

1. **General rules**, kept in the repository and run by CI: phone numbers that are not
   one of the invented examples, IMSI/ICCID-like numbers, e-mail addresses, private
   keys, absolute home directories, and coordinates that are not the example places.
2. **A denylist** of your own: a file with one entry per line (a literal, or `re:` plus
   a regular expression) naming real numbers, names, places and passwords that must
   never be published. It is never part of the repository; pass it with `--denylist`,
   set `PRIVACY_DENYLIST`, or keep it as `.privacy-denylist` in the repository root
   (which `.gitignore` excludes). A line starting with `#` is a comment; a literal is matched
   without regard to case, a `re:` pattern as written.

Exits with 1 when anything is found. Run it before every promotion to the public
repository; CI runs the general rules.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DENYLIST = None

# Invented phone numbers of examples and tests. A real number never looks like these:
# +49170 followed by repeated digits or by 12345…, and two obvious placeholders.
FICTIONAL_NUMBER = re.compile(r"^\+?(49170(0+|1+|2+|9+|12345\d*)|493012345678|34600123456|34XXXXXXXXX)$")
NUMBER = re.compile(r"(?<![\w.])(\+|00)[1-9][0-9]{9,14}(?![\w])")
IMSI = re.compile(r"(?<![0-9])(2[0-9]{2}[0-9]{2,3}[0-9]{9,10}|89[0-9]{17,19})(?![0-9])")
EMAIL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}")
EMAIL_OK = re.compile(
    r"(noreply|no-reply|example\.(com|org|invalid)|users\.noreply\.github\.com|^git@(github\.com|bitbucket\.org|gitlab\.com)$)")
HOME = re.compile(r"/(Users|home)/[a-z][a-z0-9_-]+/")
KEY = re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")
COORD = re.compile(r"(?<![0-9.])-?[0-9]{1,2}\.[0-9]{4,6}\s*,\s*-?[0-9]{1,3}\.[0-9]{4,6}")
COORD_OK = ("48.13745", "48.137452", "48.10000", "48.1", "33.86514", "33.865143")

BINARY_SUFFIXES = (".png", ".jpg", ".jpeg", ".gif", ".ico", ".bmp", ".ttf", ".bin", ".fw", ".woff", ".woff2", ".zip", ".gz")


def tracked_files():
    out = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode()
    return [os.path.join(ROOT, f) for f in out.split("\0") if f]


def walk(directory):
    for base, dirs, names in os.walk(directory):
        dirs[:] = [d for d in dirs if d != ".git"]
        for name in names:
            yield os.path.join(base, name)


def load_denylist():
    path = DENYLIST or os.environ.get("PRIVACY_DENYLIST") or os.path.join(ROOT, ".privacy-denylist")
    rules = []
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            rules.append(re.compile(line[3:]) if line.startswith("re:") else re.compile(re.escape(line), re.I))
    return rules


def scan_text(name, text, deny, hits):
    for number, line in enumerate(text.split("\n"), 1):
        for m in NUMBER.finditer(line):
            if not FICTIONAL_NUMBER.match(m.group(0).replace(" ", "")):
                hits.append((name, number, "phone number", m.group(0)))
        for m in IMSI.finditer(line):
            hits.append((name, number, "IMSI/ICCID-like number", m.group(0)))
        for m in EMAIL.finditer(line):
            if not EMAIL_OK.search(m.group(0)):
                hits.append((name, number, "e-mail address", m.group(0)))
        for m in HOME.finditer(line):
            hits.append((name, number, "home directory", m.group(0)))
        if KEY.search(line):
            hits.append((name, number, "private key", "BEGIN PRIVATE KEY"))
        for m in COORD.finditer(line):
            if not any(m.group(0).lstrip("-").startswith(ok) for ok in COORD_OK):
                hits.append((name, number, "coordinates", m.group(0)))
        for rule in deny:
            m = rule.search(line)
            if m:
                hits.append((name, number, "denylist", m.group(0)))


def scan_files(files, root):
    deny = load_denylist()
    hits = []
    for f in files:
        if f.lower().endswith(BINARY_SUFFIXES) or not os.path.isfile(f):
            continue
        try:
            text = open(f, encoding="utf-8").read()
        except (UnicodeDecodeError, OSError):
            continue
        scan_text(os.path.relpath(f, root), text, deny, hits)
    return hits


def scan_history():
    deny = load_denylist()
    hits = []
    log = subprocess.run(["git", "log", "--all", "-p", "--format=COMMIT %h %an <%ae>", "--no-color"],
                         cwd=ROOT, capture_output=True, text=True, errors="replace").stdout
    commit = ""
    for line in log.split("\n"):
        if line.startswith("COMMIT "):
            commit = line[7:]
            for rule in deny:
                if rule.search(line):
                    hits.append((f"commit {commit}", 0, "denylist (author)", rule.pattern))
            continue
        if not line.startswith("+") or line.startswith("+++"):
            continue
        scan_text(f"commit {commit}", line[1:], deny, hits)
    hits += scan_commit_texts(deny)
    return hits


def scan_commit_texts(deny):
    """What a commit says about itself: the message, and the committer next to the author. Both go public with the
    history, and the scan of the changed lines does not see them."""
    hits = []
    # NUL ends a field and the byte 0x01 ends a commit; neither occurs in a message.
    log = subprocess.run(["git", "log", "--all", "--format=%h%x00%cn <%ce>%x00%B%x01"],
                         cwd=ROOT, capture_output=True, text=True, errors="replace").stdout
    for entry in log.split("\x01"):
        if not entry.strip():
            continue
        short, committer, message = entry.strip("\n").split("\0", 2)
        scan_text(f"message of commit {short}", message, deny, hits)
        scan_text(f"committer of commit {short}", committer, deny, hits)
    return hits


def selftest():
    deny = []
    hits = []
    # The probe values are assembled so that the check does not flag its own source file.
    probe = " ".join(["Dad +49170" + "1111111", "+49151" + "98765432", "a" + "@b.de", "/Us" + "ers/max/y",
                      "52.5200" + "1,13.4049" + "5"])
    scan_text("x", probe, deny, hits)
    kinds = sorted(h[2] for h in hits)
    assert kinds == sorted(["home directory", "coordinates", "e-mail address", "phone number"]), kinds
    hits = []
    scan_text("x", "+49170" + "1111111 +49301" + "2345678 48.13745,11.57550 noreply@users.noreply.github.com", deny, hits)
    assert not hits, hits
    print("selftest ok")


def main():
    global ROOT, DENYLIST
    args = sys.argv[1:]
    if "--selftest" in args:
        return selftest()
    if "--repo" in args:
        ROOT = os.path.abspath(args[args.index("--repo") + 1])
    if "--denylist" in args:
        DENYLIST = os.path.abspath(args[args.index("--denylist") + 1])
    if "--dir" in args:
        directory = os.path.abspath(args[args.index("--dir") + 1])
        hits = scan_files(list(walk(directory)), directory)
    else:
        hits = scan_files(tracked_files(), ROOT)
    if "--history" in args:
        hits += scan_history()
    if not (DENYLIST or os.environ.get("PRIVACY_DENYLIST") or os.path.exists(os.path.join(ROOT, ".privacy-denylist"))):
        print("note: no denylist found — only the general rules apply.", file=sys.stderr)
    for name, number, kind, what in hits[:200]:
        print(f"{name}:{number}: {kind}: {what}")
    if hits:
        print(f"\n{len(hits)} findings — do not publish like this.", file=sys.stderr)
        sys.exit(1)
    print("privacy_check: no findings")


if __name__ == "__main__":
    main()
