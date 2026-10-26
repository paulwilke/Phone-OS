#!/usr/bin/env python3
"""Builds the click-through demo: a picture of every state of the interface in every shipped
language, and a static page that walks through them.

    pio run -e sim
    python3 scripts/make_demo.py            # from the repository root; writes out/demo/ (not committed)

1. The simulator walks the interface by its keys from a world with a few messages in it
   (`program --crawl`): the graph of states, and each state's picture in each language.
2. The scene scripts under sim/demo/ take the pictures the keys do not lead to (a call coming in,
   school time), once for each language.
3. sim/demo/page.html is filled with the graph, the scene names and the words of the page, which come
   from the catalogs (`demo.*`, in the host-only files `tools/lang/demo.<code>.toml`): the page has no words of its own, and a language selector swaps every
   picture and every word.

Then the result is checked: every language has a picture for every state and every scene, every
screen of the device that the keys can reach is in the walk, and the page and the graph exist. The
exit status is 1 when anything is missing, so a CI run that "built" nothing fails.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "out", "demo")
DEMO = os.path.join(ROOT, "sim", "demo")

# The screens the keys lead to on the child's device. The walk must contain each of them.
# The screens the walk on the child's card must reach. The Wi-Fi page, the status page and the page for a number
# need the parents' tools or free text, which that card does not have: scenes/scenes-parent.txt takes them.
SCREENS = ["home", "lock", "clock", "phone", "call", "messages", "compose", "recipient", "sos", "settings", "net",
           "language", "gnss", "radio", "motion", "selftest", "power"]
# Fewer states than this means the walk did not get far (the shipped interface has well over a hundred).
MIN_STATES = 60

# Every word the page uses, so that a missing translation is found here and not by a visitor.
PAGE_WORDS = [
    "demo.title", "demo.intro", "demo.language", "demo.key-up", "demo.key-down", "demo.key-select", "demo.key-back",
    "demo.key-home", "demo.key-inbox", "demo.key-compose", "demo.start-over", "demo.state", "demo.trimmed",
    "demo.call-note", "demo.scenes", "demo.back-to-walk",
]
# The name of every scene the scripts take a picture of. Written out so that a scene without a name,
# and a name without a scene, are both found.
SCENE_PREFIX = "demo" + ".scene-"
SCENE_WORDS = [
    "demo.scene-unread", "demo.scene-incoming-call", "demo.scene-call-connected", "demo.scene-call-dialing",
    "demo.scene-call-failed", "demo.scene-no-network", "demo.scene-battery-low", "demo.scene-locked",
    "demo.scene-school-time", "demo.scene-school-phone-refused", "demo.scene-school-call-refused",
    "demo.scene-parent-wifi", "demo.scene-parent-wifi-on", "demo.scene-parent-status", "demo.scene-parent-number",
]


def flatten(table, prefix=""):
    out = {}
    for key, value in table.items():
        if isinstance(value, dict):
            out.update(flatten(value, f"{prefix}{key}."))
        else:
            out[f"{prefix}{key}"] = value
    return out


def catalogs():
    result = []
    folder = os.path.join(ROOT, "lang")
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".toml"):
            continue
        with open(os.path.join(folder, name), "rb") as f:
            data = tomllib.load(f)
        words = flatten(data.get("strings", {}))
        # The words of the page are in the host-only set tools/lang/demo.<code>.toml, which the device never gets.
        host = os.path.join(ROOT, "tools", "lang", "demo." + name)
        if os.path.exists(host):
            with open(host, "rb") as f:
                words.update(flatten(tomllib.load(f).get("strings", {})))
        result.append((data["meta"]["code"], data["meta"]["native"], words))
    return result


def scene_scripts():
    """(script, card, [scene ids]) for each file in sim/demo/ that takes pictures with `shot`."""
    found = []
    for name in sorted(os.listdir(DEMO), key=lambda n: (n != "scenes.txt", n)):
        if not (name.startswith("scenes") and name.endswith(".txt")):
            continue
        text = open(os.path.join(DEMO, name), encoding="utf-8").read()
        card = re.search(r"^# data:\s*(\S+)", text, re.M)
        found.append((os.path.join("sim", "demo", name), card.group(1) if card else "sim/data", re.findall(r"^shot (\S+)", text, re.M)))
    return found


def run(args, what):
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"make_demo: {what} failed:\n{result.stdout[-1500:]}{result.stderr[-1500:]}")
        sys.exit(1)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--program", default=os.path.join(ROOT, ".pio", "build", "sim", "program"))
    parser.add_argument("--out", default=OUT)
    args = parser.parse_args()
    if not os.path.exists(args.program):
        print(f"make_demo: no simulator at {args.program}; build it with `pio run -e sim`")
        return 1
    out = os.path.abspath(args.out)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)

    languages = catalogs()
    codes = [code for code, _, _ in languages]
    problems = []
    for code, _, words in languages:
        problems += [f"{code} lacks {key}" for key in PAGE_WORDS if key not in words]

    summary = run([args.program, "--crawl", out, "--prelude", "sim/demo/world.txt", "--languages", ",".join(codes)], "the walk")
    graph = json.load(open(os.path.join(out, "graph.json")))

    scenes = []
    for script, card, ids in scene_scripts():
        for code in codes:
            driver = os.path.join(out, f".scenes-{code}.txt")
            with open(driver, "w", encoding="utf-8") as f:
                f.write(f"language {code}\nshotdir {out}/{code}\nrun {script}\n")
            run([args.program, "--data", card, "--script", driver], f"the scenes of {script} in {code}")
            os.remove(driver)
        scenes += ids
    # A scene's picture is named after it, so the page can tell it from a state.
    for code in codes:
        for scene in scenes:
            os.rename(os.path.join(out, code, f"{scene}.png"), os.path.join(out, code, f"scene-{scene}.png"))

    for scene in scenes:
        if not any(word[len(SCENE_PREFIX):] == scene for word in SCENE_WORDS):
            problems.append(f"the scene {scene} is not in SCENE_WORDS")
    for word in SCENE_WORDS:
        if not any(word[len(SCENE_PREFIX):] == scene for scene in scenes):
            problems.append(f"{word} names a scene no script takes a picture of")
        for code, _, words in languages:
            if word not in words:
                problems.append(f"{code} lacks {word}")

    page = open(os.path.join(DEMO, "page.html"), encoding="utf-8").read()
    payload = {
        "graph": graph,
        "scenes": scenes,
        "languages": [{"code": code, "native": native} for code, native, _ in languages],
        "words": {code: {k: v for k, v in words.items() if k.startswith("demo.")} for code, _, words in languages},
    }
    # "</" cannot end the script element early inside JSON text.
    text = json.dumps(payload, ensure_ascii=False).replace("</", "<\\/")
    with open(os.path.join(out, "index.html"), "w", encoding="utf-8") as f:
        f.write(page.replace("__DATA__", text))

    # What there must be.
    for code in codes:
        for node in graph["nodes"]:
            if not os.path.exists(os.path.join(out, code, f"n{node['id']}.png")):
                problems.append(f"no picture of state {node['id']} in {code}")
        for scene in scenes:
            if not os.path.exists(os.path.join(out, code, f"scene-{scene}.png")):
                problems.append(f"no picture of the scene {scene} in {code}")
    reached = {node["screen"] for node in graph["nodes"]}
    problems += [f"the walk never reaches the screen {screen}" for screen in SCREENS if screen not in reached]
    if len(graph["nodes"]) < MIN_STATES:
        problems.append(f"only {len(graph['nodes'])} states in the walk, expected at least {MIN_STATES}")
    expected = len(codes) * (len(graph["nodes"]) + len(scenes))
    found = sum(1 for _, _, names in os.walk(out) for n in names if n.endswith(".png"))
    if found != expected:
        problems.append(f"{found} pictures, expected {expected} ({len(codes)} languages x {len(graph['nodes'])} states + {len(scenes)} scenes)")

    for problem in problems:
        print("make_demo:", problem)
    if problems:
        return 1
    print(summary.strip())
    print(f"make_demo: {found} pictures, {len(graph['nodes'])} states, {len(scenes)} scenes, {len(codes)} languages -> {os.path.relpath(out, ROOT)}/index.html")
    return 0


if __name__ == "__main__":
    sys.exit(main())
