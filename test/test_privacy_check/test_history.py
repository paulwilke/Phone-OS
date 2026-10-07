#!/usr/bin/env python3
"""Tests of the history scan of scripts/privacy_check.py on small repositories made on the fly: a clean history
passes, and a leak is found wherever it hides in a commit: a changed line, the message, the committer's identity.
The values are assembled from parts so that the check does not flag this file.

    python3 test/test_privacy_check/test_history.py
"""
import os
import subprocess
import sys
import tempfile

SCRIPT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "scripts", "privacy_check.py")
REAL_NUMBER = "+49151" + "98765432"
REAL_MAIL = "someone" + "@" + "mail.example.de"

failures = []


def git(root, *args, env=None):
    subprocess.run(["git", *args], cwd=root, check=True, capture_output=True, env={**os.environ, **(env or {})})


def history(commits, denylist=None):
    """Runs the scan on a repository with these commits: (file text, message, committer mail)."""
    with tempfile.TemporaryDirectory() as root:
        git(root, "init", "-q")
        for text, message, committer in commits:
            with open(os.path.join(root, "file.txt"), "w") as f:
                f.write(text)
            git(root, "add", "file.txt")
            git(root, "-c", "user.name=Claude", "-c", "user.email=noreply@anthropic.com", "commit", "-q", "-m", message,
                env={"GIT_COMMITTER_NAME": "Committer", "GIT_COMMITTER_EMAIL": committer})
        args = [sys.executable, SCRIPT, "--repo", root, "--history"]
        with tempfile.NamedTemporaryFile("w", suffix=".txt") as deny:
            if denylist:
                deny.write(denylist)
                deny.flush()
                args += ["--denylist", deny.name]
            p = subprocess.run(args, capture_output=True, text=True)
        return p.returncode, p.stdout


def check(name, commits, expect, denylist=None):
    code, out = history(commits, denylist)
    ok = (code == 0) if expect is None else (code == 1 and expect in out)
    print(("ok   " if ok else "FAIL ") + name)
    if not ok:
        failures.append(name)
        print(code, out)


CLEAN = ("hello\n", "A clean message", "noreply@anthropic.com")
check("a clean history passes", [CLEAN], None)
check("a number in a changed line is found", [("call " + REAL_NUMBER + "\n", "x", "noreply@anthropic.com")], "phone number")
check("a number in a commit message is found", [("hello\n", "Call " + REAL_NUMBER + " now", "noreply@anthropic.com")], "message of commit")
check("an e-mail address in the body of a message is found", [("hello\n", "Subject\n\nThanks to " + REAL_MAIL, "noreply@anthropic.com")], "e-mail address")
check("a real e-mail address of the committer is found", [("hello\n", "x", REAL_MAIL)], "committer of commit")
check("a denylisted word in a message is found", [("hello\n", "About the Zebrafish family", "noreply@anthropic.com")], "denylist", denylist="zebrafish\n")
check("a message that only mentions the example number passes", [("hello\n", "Use +491701111111 in examples", "noreply@anthropic.com")], None)

sys.exit(1 if failures else 0)
