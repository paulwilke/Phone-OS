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
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIO = os.path.expanduser("~/.platformio/penv/bin/pio")
if not os.path.exists(PIO):
    PIO = "pio"

# scenario, [(file, text that is there, what it is replaced with), ...], what is broken. A scenario may have
# several rows; a row may change several files.
MUTATIONS = [
    ('normal-day', [('src/boot/Runtime.cpp', 'call == ModemState::RingingIn && !callInterrupt_', 'false && !callInterrupt_')], 'an incoming call no longer takes the screen'),
    ('school-time', [('src/app/SosFlow.h', 'kOverrideTaps = 5', 'kOverrideTaps = 3')], 'the SOS in school time needs three taps, not five'),
    ('school-time-off', [('src/app/QuietHours.cpp', 'if (!w.enabled) continue;', '')], 'a window the parents switched off still locks the phone'),
    ('remote-query-during-sos', [('src/app/SosPing.cpp', 'if (!(kind_ == Kind::Query && kind != Kind::Query)) return false;', 'if (false) return false;')], 'a remote query replaces a running emergency'),
    ('network-drop', [('src/app/Messaging.cpp', 'void Messaging::flushQueue() {\n  if (outbox_.empty()) return;', 'void Messaging::flushQueue() {\n  return;')], 'the queue is never sent when the network is back'),
    ('contacts-only', [('src/app/Messaging.cpp', 'if (!profile_.mayReceiveFrom(sms.phone)) {', 'if (false) {')], 'a stranger\'s text is stored like any other'),
    ('contacts-only-calls', [('src/app/Calls.cpp', 'if (!profile_.mayReceiveFrom(ev.phone)) {\n    if (!rejectedUnknown_) {', 'if (false) {\n    if (!rejectedUnknown_) {')], 'a stranger\'s call rings like any other'),
    ('language-during-emergency', [('src/app/Emergency.cpp', 'const std::string& language = profile_.smsLanguage;', 'const std::string language = translator_.language();')], "the SOS text follows the child's screen language"),
    ('missed-call', [('src/ui/screens/MessageListScreen.cpp', 'if (words != worded_) {', 'if (false) {')], 'an open entry keeps the words of the language it was opened in'),
    ('user-catalog', [('src/boot/Runtime.cpp', '  config::ConfigLoader::loadUserCatalogs(capabilities.store, translator, diagnostics);', '')], 'the catalogs on the card are not read'),
    ('bad-catalog', [('src/config/ConfigLoader.cpp', 'if (!add(text, &errors)) {', 'if (!add(text, &errors) && false) {')], 'a user catalog that fails is no longer reported'),
    ('sos-page', [('src/ui/screens/SosScreen.cpp', 'if (index == 0) app().chooseSos(SosPing::Kind::Emergency);', 'if (index == 0) app().chooseSos(SosPing::Kind::Pickup);')], 'the first field of the SOS page sends "come and get me"'),
    ('sos-page-school-time', [('src/ui/screens/SosScreen.cpp', 'if (input.isKey(Key::Select)) {\n        app().tapSos();', 'if (input.isKey(Key::Select)) {\n        (void)0;')], 'the key of the device no longer counts as a tap on the SOS page in school time'),
    ('language-page', [('src/ui/screens/LanguageScreen.cpp', 'if (chosen) app().setInterfaceLanguage(codes_[list_.selected()]);', '')], 'a choice on the language page changes nothing'),
    ('language-page', [('src/boot/Runtime.cpp', '  if (!core_.settingsNeedWrite()) return;', '  return;')], 'what the child changed is never written back to the card'),
    ('settings-page', [('src/boot/Runtime.cpp', '  if (!core_.settingsNeedWrite()) return;', '  return;')], 'what the child changed is never written back to the card'),
    ('settings-page', [('src/app/AppCore.cpp', 'void AppCore::setMessageVibration(bool on) {\n  profile_.messageVibration = on;', 'void AppCore::setMessageVibration(bool on) {\n  profile_.messageVibration = on || true;')], 'the vibration cannot be switched off'),
    ('wifi-page', [('src/ui/screens/WifiScreen.cpp', 'if (index < 3) app().setWifiMode(kModes[index]);', 'if (index > 3) app().setWifiMode(kModes[index]);')], 'the rows of the Wi-Fi page do nothing'),
    ('network-status-pages', [('src/ui/screens/NetScreen.cpp', '{{"bars", s.signalBars}}', '{{"bars", 0}}')], 'the network page does not say the signal'),
    ('network-status-pages', [('src/ui/screens/StatusScreen.cpp', 'ChargeState::Charging ? "status.battery-charging"', 'ChargeState::Charging ? "status.battery"')], 'the status page does not say the battery is charging'),
    ('stranger-flood', [('src/app/DropLog.h', 'kWindowSeconds = 600', 'kWindowSeconds = 0')], 'every dropped text is a line of its own again'),
    ('stranger-flood', [('src/app/DropLog.cpp', 'if (!shown.empty()) line += ": " + shown;\n  drop("num:"', 'drop("num:"')], 'the text of a dropped message is not in the log'),
    ('radio-peer', [('src/app/Messaging.cpp', 'if (profile_.radioPeer.empty() || msg.peer != profile_.radioPeer) {', 'if (false) {')], 'a radio message from any name is stored'),
    ('radio-peer', [('src/app/Messaging.cpp', 'msg.peer != profile_.radioPeer', 'msg.peer.size() != profile_.radioPeer.size()')], 'the radio peer is compared by its length, not its letters'),
    ('radio-no-peer', [('src/app/Messaging.cpp', 'if (profile_.radioPeer.empty() || msg.peer != profile_.radioPeer) {', 'if (!profile_.radioPeer.empty() && msg.peer != profile_.radioPeer) {')], 'with no radio peer configured every radio message is accepted'),
    ("wifi-admin-page",
     [("src/app/AppCore.h", "WifiController* wifiController() { return wifi_.get(); }",
       "WifiController* wifiController() { return second_.get(); }\n  std::unique_ptr<WifiController> second_;"),
      ("src/app/AppCore.cpp", "  connectivity_.setWifi(wifi_.get());", "  connectivity_.setWifi(wifi_.get());\n  second_ = std::make_unique<WifiController>(wifi, random);")],
     "a second Wi-Fi controller is back: the page tells it about use, the device runs another"),
    ("wifi-admin-page",
     [("src/web/WebApi.cpp", "  if (!authenticate(req, res, now)) return;\n",
       "  if (WifiController* early = app_.wifiController()) early->noteActivity(now);\n  if (!authenticate(req, res, now)) return;\n")],
     "a request without the password restarts the idle time"),
    ("wifi-admin-page", [("src/web/WebApi.cpp", 'strcmp(p, "/wifi-extend") == 0', 'strcmp(p, "/wifi-extend-x") == 0')],
     "the button \"keep Wi-Fi on\" is not there"),
    ('call-in-the-pocket', [('src/ui/screens/CallScreen.cpp', 'const ModemState call = context().app.callState();', 'const ModemState call = ModemState::Off;')], 'a key under the screen takes the call page away during a call'),
    ('call-in-the-pocket', [('src/ui/ScreenStack.cpp', 'if (relock && hasLock_', 'if (false && relock && hasLock_')], 'a call that rang through the lock leaves the device unlocked'),
    ('sos-during-a-call', [('src/ui/screens/SosScreen.h', 'if (app().callState() != ModemState::RingingIn) app().leaveSos();', 'app().leaveSos();')], 'a call during the SOS countdown cancels the emergency'),
]


def clean():
    """The build system tells a restored file from a changed one by its time and its content; a file
    changed and changed back within a second is not always noticed. A broken build is therefore never
    trusted to be rebuilt: it starts from nothing."""
    shutil.rmtree(os.path.join(ROOT, ".pio", "build", "sim"), ignore_errors=True)


def run(args):
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True)


def main():
    only = sys.argv[1:]
    failed = []
    for scenario, edits, what in MUTATIONS:
        if only and scenario not in only:
            continue
        originals = {}
        try:
            missing = []
            for path, old, new in edits:
                full = os.path.join(ROOT, path)
                originals.setdefault(path, open(full, encoding="utf-8").read())
                if old not in originals[path]:
                    missing.append(path)
            if missing:
                print(f"{scenario}: the text to break is not in {', '.join(missing)}: the table is out of date")
                failed.append(scenario)
                continue
            for path, old, new in edits:
                full = os.path.join(ROOT, path)
                text = open(full, encoding="utf-8").read().replace(old, new, 1)  # read first: opening for writing empties the file
                open(full, "w", encoding="utf-8").write(text)
            clean()
            build = run([PIO, "run", "-e", "sim"])
            if build.returncode != 0:
                print(f"{scenario}: the broken simulator does not build ({what}):\n{''.join(l + chr(10) for l in (build.stdout + build.stderr).splitlines() if 'error' in l or 'ndefined' in l or 'symbol' in l)[-1500:]}")
                failed.append(scenario)
                continue
            result = run([os.path.join(ROOT, ".pio/build/sim/program"), "--script", f"sim/scripts/{scenario}.txt"])
        finally:
            for path, text in originals.items():
                open(os.path.join(ROOT, path), "w", encoding="utf-8").write(text)
        if result.returncode == 0:
            print(f"{scenario}: STILL PASSES with {what}")
            failed.append(scenario)
        else:
            reason = [l for l in result.stdout.splitlines() if l.startswith("error")]
            print(f"{scenario}: fails with {what}  ({reason[0] if reason else 'no message'})")
    clean()
    run([PIO, "run", "-e", "sim"])  # leave a correct simulator behind
    if failed:
        print(f"\n{len(failed)} scenario(s) without a working counter-check: {', '.join(failed)}")
        return 1
    print("\nevery scenario fails when its behaviour is broken")
    return 0


if __name__ == "__main__":
    sys.exit(main())
