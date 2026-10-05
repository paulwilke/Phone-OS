#!/usr/bin/env python3
"""Proves that every scenario of the simulator can fail: breaks the behaviour it is about, on
purpose, and expects the scenario to stop with an error.

    python3 scripts/counter_check_scenarios.py          # from the repository root; takes a few minutes

For each row of MUTATIONS the file is changed, `pio run -e sim` builds the simulator, the scenario
runs, and the file is put back (also when something goes wrong). A scenario that still passes with
its behaviour broken proves nothing and is reported. Not part of the CI: it builds the simulator a
dozen times. Run it when a scenario is written or changed.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIO = os.path.expanduser("~/.platformio/penv/bin/pio")
if not os.path.exists(PIO):
    PIO = "pio"

# scenario, file, text that is there, what it is replaced with, what is broken
MUTATIONS = [
    ("normal-day", "src/boot/Runtime.cpp", "call == ModemState::RingingIn && !callInterrupt_", "false && !callInterrupt_",
     "an incoming call no longer takes the screen"),
    ("school-time", "src/app/SosFlow.h", "kOverrideTaps = 5", "kOverrideTaps = 3", "the SOS in school time needs three taps, not five"),
    ("remote-query-during-sos", "src/app/SosPing.cpp", "if (!(kind_ == Kind::Query && kind != Kind::Query)) return false;",
     "if (false) return false;", "a remote query replaces a running emergency"),
    ("network-drop", "src/app/Messaging.cpp", "void Messaging::flushQueue() {\n  if (outbox_.empty()) return;",
     "void Messaging::flushQueue() {\n  return;", "the queue is never sent when the network is back"),
    ("save-mode", "src/app/Connectivity.h", "kProbeSeconds = 180", "kProbeSeconds = 60", "the probe lasts one minute, not three"),
    ("language-during-emergency", "src/app/Emergency.cpp", "const std::string& language = profile_.smsLanguage;",
     "const std::string language = translator_.language();", "the SOS text follows the child's screen language"),
    ("missed-call", "src/ui/screens/MessageListScreen.cpp", "if (words != worded_) {", "if (false) {",
     "an open entry keeps the words of the language it was opened in"),
    ("user-catalog", "src/boot/Runtime.cpp", "  config::ConfigLoader::loadUserCatalogs(capabilities.store, translator, diagnostics);", "",
     "the catalogs on the card are not read"),
    ("bad-catalog", "src/config/ConfigLoader.cpp", "if (!translator.addUser(text.c_str(), text.size(), &errors)) {", "if (!translator.addUser(text.c_str(), text.size(), &errors) && false) {",
     "a user catalog that fails is no longer reported"),
]


def run(args):
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True)


def main():
    only = sys.argv[1:]
    failed = []
    for scenario, path, old, new, what in MUTATIONS:
        if only and scenario not in only:
            continue
        full = os.path.join(ROOT, path)
        original = open(full, encoding="utf-8").read()
        if old not in original:
            print(f"{scenario}: the text to break is not in {path}: the table is out of date")
            failed.append(scenario)
            continue
        try:
            open(full, "w", encoding="utf-8").write(original.replace(old, new, 1))
            build = run([PIO, "run", "-e", "sim"])
            if build.returncode != 0:
                print(f"{scenario}: the broken simulator does not build:\n{build.stdout[-800:]}")
                failed.append(scenario)
                continue
            result = run([os.path.join(ROOT, ".pio/build/sim/program"), "--script", f"sim/scripts/{scenario}.txt"])
        finally:
            open(full, "w", encoding="utf-8").write(original)
        if result.returncode == 0:
            print(f"{scenario}: STILL PASSES with {what}")
            failed.append(scenario)
        else:
            reason = [l for l in result.stdout.splitlines() if l.startswith("error")]
            print(f"{scenario}: fails with {what}  ({reason[0] if reason else 'no message'})")
    run([PIO, "run", "-e", "sim"])  # leave a correct simulator behind
    if failed:
        print(f"\n{len(failed)} scenario(s) without a working counter-check: {', '.join(failed)}")
        return 1
    print("\nevery scenario fails when its behaviour is broken")
    return 0


if __name__ == "__main__":
    sys.exit(main())
