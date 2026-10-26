#!/usr/bin/env python3
"""Builds the click-through demo: a picture of every state of the interface in every shipped
language, and a static page that walks through them.

    pio run -e sim
    python3 scripts/make_demo.py            # from the repository root; writes out/demo/ (not committed)

1. The simulator walks the interface by its keys and by taps from a world with a few messages in it
   (`program --crawl`), started once in each language: one graph of states, the language being part of a state (the
   Language screen leads from one into another), and the picture of every state in the language it is in. Nothing is
   cut off: every key and every tap of every state is followed, and the check below fails on a key that is drawn and
   leads nowhere.
2. The scene scripts under sim/demo/ take the pictures the keys do not lead to (a call coming in,
   school time), once for each language.
3. sim/demo/page.html is filled with the graph, the scene names and the words of the page, which come
   from the catalogs (`demo.*`, in the host-only files `tools/lang/demo.<code>.toml`): the page has no words of its own, and a language selector swaps every
   picture and every word.

Then the result is checked (`check_graph`, and the files): every state has its picture and every language its scenes,
every screen of the device that the keys can reach is in the walk in every language, the walk has no pseudo-language,
every key of every state was pressed and every soft key that is drawn leads somewhere, every state can get back to the
home screen, and each language can be chosen from each other on the Language screen by key and by tap. The exit status
is 1 when anything is missing, so a CI run that "built" nothing fails.
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
    "demo.key-home", "demo.key-inbox", "demo.key-compose", "demo.start-over", "demo.state",
    "demo.scenes", "demo.back-to-walk",
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


def run(args, what, progress=False):
    """Runs a program from the repository root; with `progress` its standard error is the terminal's (the walk says how far it is)."""
    result = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=None if progress else subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"make_demo: {what} failed:\n{result.stdout[-1500:]}{result.stderr[-1500:] if result.stderr else ''}")
        sys.exit(1)
    return result.stdout


# The seven keys of the device, in the order the walk presses them.
KEYS = ["up", "down", "select", "back", "home", "inbox", "compose"]
# Where the device answers a soft key that is drawn with the picture it has already, and the page is right to show the
# key without a way on. Every other drawn soft key must lead somewhere: one that does not is a walk that stopped short
# or a key that does nothing on the device.
# The arrows of a list are drawn on the soft key bar whatever the cursor does: at the end of a list the key does nothing.
SCROLL_KEYS = ("up", "down")
SOFT_KEY_IDLE_OK = {
    ("language", "select"),   # choosing the language that is already in use
    ("motion", "select"),     # Reset on the motion page, with the sensor at rest: there is nothing to reset
}


def check_graph(graph, codes):
    """What must be true of the graph of the walk, as a list of problems (empty when it holds): it is complete, each
    language starts in its own picture and no state is in a language the device does not have (the pseudo-language is a
    layout tool of the simulator), every key of every state was pressed (it has an edge or is noted as unchanged), a soft
    key that is drawn is not dead, every state can get back to the home screen, and on the Language screen every
    language can be chosen from every other with a key and with a tap, and no step but a choice on the Language screen
    changes the language (a restart keeps it)."""
    problems = []
    nodes = graph.get("nodes", [])
    if not graph.get("complete"):
        problems.append("the walk is not complete: a state was left unfollowed")
    starts = graph.get("starts", {})
    for code in codes:
        start = starts.get(code)
        if not isinstance(start, int) or not 0 <= start < len(nodes):
            problems.append(f"the walk has no start state in {code}")
        elif nodes[start]["lang"] != code:
            problems.append(f"the start state of {code} is in {nodes[start]['lang']}")
    for language in sorted({n["lang"] for n in nodes} - set(codes)):
        problems.append(f"the walk has states in {language}, which is not a language of the device")
    outgoing = {n["id"]: [] for n in nodes}
    for edge in graph.get("edges", []):
        outgoing[edge["from"]].append(edge)
    for n in nodes:
        where = f"state {n['id']} ({n['screen']}, {n['lang']})"
        pressed = {e["key"] for e in outgoing[n["id"]] if "key" in e} | set(n.get("idle", []))
        problems += [f"{where}: the key {k} was never pressed" for k in KEYS if k not in pressed]
        for k in n.get("soft", []):
            if k in n.get("idle", []) and k not in SCROLL_KEYS and (n["screen"], k) not in SOFT_KEY_IDLE_OK:
                problems.append(f"{where}: the soft key {k} is drawn and leads nowhere")
    # Every state can get back to the home screen: walk the edges backwards from every home.
    back = {n["id"]: [] for n in nodes}
    for edge in graph.get("edges", []):
        back[edge["to"]].append(edge["from"])
    home = [n["id"] for n in nodes if n["screen"] == "home"]
    reached = set(home)
    queue = list(home)
    while queue:
        for previous in back[queue.pop()]:
            if previous not in reached:
                reached.add(previous)
                queue.append(previous)
    for n in nodes:
        if n["id"] not in reached:
            problems.append(f"state {n['id']} ({n['screen']}, {n['lang']}) cannot get back to the home screen")
    # The Language screen: each language can be chosen from each other by key and by tap.
    chosen = set()
    for n in nodes:
        if n["screen"] == "language":
            for e in outgoing[n["id"]]:
                chosen.add((n["lang"], nodes[e["to"]]["lang"], "key" if "key" in e else "tap"))
    for here in codes:
        for there in codes:
            if here != there:
                problems += [f"in {here} the Language screen cannot choose {there} with a {how}" for how in ("key", "tap")
                             if (here, there, how) not in chosen]
    # Only the Language screen changes the language: a restart (the Radio page's) and every other step keeps it, as on
    # the device, where the language is kept on the card.
    for edge in graph.get("edges", []):
        before, after = nodes[edge["from"]], nodes[edge["to"]]
        if before["lang"] != after["lang"] and before["screen"] != "language":
            how = next(k for k in ("key", "tap", "wait") if k in edge)
            problems.append(f"state {before['id']} ({before['screen']}, {before['lang']}): the {how} leads to state "
                            f"{after['id']} in {after['lang']}, and only the Language screen changes the language")
    # Every screen the keys lead to is in the walk, in every language.
    for code in codes:
        shown = {n["screen"] for n in nodes if n["lang"] == code}
        problems += [f"the walk never reaches the screen {screen} in {code}" for screen in SCREENS if screen not in shown]
    return problems


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

    # English first: it is the first start of the walk (state 0 is English, as the web page's markup assumes before its
    # script has run) and the first choice of the demo's selector.
    languages = sorted(catalogs(), key=lambda entry: (entry[0] != "en", entry[0]))
    codes = [code for code, _, _ in languages]
    problems = []
    for code, _, words in languages:
        problems += [f"{code} lacks {key}" for key in PAGE_WORDS if key not in words]

    summary = run([args.program, "--crawl", out, "--prelude", "sim/demo/world.txt", "--languages", ",".join(codes)], "the walk", progress=True)
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
    problems += check_graph(graph, codes)
    for node in graph["nodes"]:
        if not os.path.exists(os.path.join(out, node["lang"], f"n{node['id']}.png")):
            problems.append(f"no picture of state {node['id']} in {node['lang']}")
    for code in codes:
        for scene in scenes:
            if not os.path.exists(os.path.join(out, code, f"scene-{scene}.png")):
                problems.append(f"no picture of the scene {scene} in {code}")
    if len(graph["nodes"]) < MIN_STATES:
        problems.append(f"only {len(graph['nodes'])} states in the walk, expected at least {MIN_STATES}")
    expected = len(graph["nodes"]) + len(codes) * len(scenes)
    found = sum(1 for _, _, names in os.walk(out) for n in names if n.endswith(".png"))
    if found != expected:
        problems.append(f"{found} pictures, expected {expected} ({len(graph['nodes'])} states, each in the language it is in, + {len(scenes)} scenes x {len(codes)} languages)")

    for problem in problems:
        print("make_demo:", problem)
    if problems:
        return 1
    print(summary.strip())
    print(f"make_demo: {found} pictures, {len(graph['nodes'])} states in {len(codes)} languages, {len(graph['edges'])} edges, {len(scenes)} scenes -> {os.path.relpath(out, ROOT)}/index.html")
    return 0


if __name__ == "__main__":
    sys.exit(main())
