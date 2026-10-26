#!/usr/bin/env python3
"""Checks the rules scripts/make_demo.py holds the graph of the walk to, on small graphs made here: the demo is not
built from a graph in which a key that is drawn leads nowhere, in which a state cannot get back to the home screen, in
which a language cannot be chosen from another, or which holds a language the device does not have.

    python3 test/test_make_demo/test_graph.py
"""
import copy
import importlib.util
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("make_demo", os.path.join(ROOT, "scripts", "make_demo.py"))
make_demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(make_demo)

CODES = ["en", "de", "fr"]
KEYS = make_demo.KEYS

failures = 0


def check(cond, what):
    global failures
    print(("  PASS: " if cond else "  FAIL: ") + what)
    if not cond:
        failures += 1


def good_graph():
    """Every screen in every language; the home screen leads to each by Enter and each leads back by Back, every other
    key was pressed and changed nothing; the Language screen of a language chooses each other language with a key and
    a tap."""
    nodes, edges, starts = [], [], {}
    ids = {}
    for code in CODES:
        for screen in make_demo.SCREENS:
            ids[(code, screen)] = len(nodes)
            nodes.append({"id": len(nodes), "screen": screen, "lang": code, "depth": 0 if screen == "home" else 1,
                          "soft": [], "idle": []})
        starts[code] = ids[(code, "home")]
    for code in CODES:
        home = ids[(code, "home")]
        for screen in make_demo.SCREENS:
            if screen == "home":
                continue
            there = ids[(code, screen)]
            edges.append({"from": home, "key": "select", "to": there})
            edges.append({"from": there, "key": "back", "to": home})
    for code in CODES:
        language = ids[(code, "language")]
        others = [c for c in CODES if c != code]
        edges.append({"from": language, "key": "up", "to": ids[(others[0], "home")]})
        edges.append({"from": language, "key": "down", "to": ids[(others[1], "home")]})
        edges.append({"from": language, "tap": {"x": 0, "y": 40, "w": 240, "h": 30}, "to": ids[(others[0], "home")]})
        edges.append({"from": language, "tap": {"x": 0, "y": 70, "w": 240, "h": 30}, "to": ids[(others[1], "home")]})
    # Every key a state does not lead anywhere with was pressed and changed nothing.
    for node in nodes:
        have = {e["key"] for e in edges if e["from"] == node["id"] and "key" in e}
        node["idle"] = [k for k in KEYS if k not in have]
    # The soft keys drawn: Back everywhere; the arrows of the phone page, which are at the end of a list and do nothing
    # there, as on the device; Choose on the Language screen, which does nothing on the language in use.
    for node in nodes:
        node["soft"] = {"phone": ["back", "up", "down"], "language": ["back", "select"]}.get(node["screen"], [] if node["screen"] == "home" else ["back"])
    return {"starts": starts, "complete": True, "nodes": nodes, "edges": edges}


def problems(graph):
    return make_demo.check_graph(graph, CODES)


def has(found, words):
    return any(words in p for p in found)


graph = good_graph()
found = problems(graph)
check(found == [], "a graph that has everything passes: " + "; ".join(found[:3]))

# The arrows of a list that is at its end are drawn and do nothing, as on the device; the choice of the language in use too.
phone = next(n for n in graph["nodes"] if n["screen"] == "phone" and n["lang"] == "en")
check("up" in phone["soft"] and "up" in phone["idle"] and problems(graph) == [], "a scroll arrow that is drawn and does nothing at the end of a list is not a dead key")
language = next(n for n in graph["nodes"] if n["screen"] == "language" and n["lang"] == "en")
check("select" in language["idle"] and problems(graph) == [],
      "Enter on the language that is in use changes nothing and is allowed (the device does nothing there either)")

# A key that was never pressed: the walk stopped short (the old cap on the states of a screen did this).
g = copy.deepcopy(graph)
node = next(n for n in g["nodes"] if n["screen"] == "settings" and n["lang"] == "de")
g["edges"] = [e for e in g["edges"] if not (e["from"] == node["id"] and e.get("key") == "back")]
found = problems(g)
check(has(found, f"state {node['id']} (settings, de): the key back was never pressed"), "a state with a key neither followed nor noted fails and names the state and the key: " + "; ".join(found[:2]))

# A soft key that is drawn and leads nowhere.
g = copy.deepcopy(graph)
node = next(n for n in g["nodes"] if n["screen"] == "call" and n["lang"] == "en")
g["edges"] = [e for e in g["edges"] if not (e["from"] == node["id"] and e.get("key") == "back")]
node["idle"].append("back")
found = problems(g)
check(has(found, "the soft key back is drawn and leads nowhere"), "Hang up drawn and dead fails: " + "; ".join(found[:2]))
check(has(found, "(call, en) cannot get back to the home screen"), "and a state with no way out says that it cannot get back")

# A state that cannot get back to the home screen, though every key was pressed.
g = copy.deepcopy(graph)
node = next(n for n in g["nodes"] if n["screen"] == "sos" and n["lang"] == "fr")
g["edges"] = [e for e in g["edges"] if e["from"] != node["id"]]
node["idle"] = list(KEYS)
node["soft"] = []
found = problems(g)
check(has(found, f"state {node['id']} (sos, fr) cannot get back to the home screen"), "a state that cannot get back to the home screen fails: " + "; ".join(found[:2]))

# The walk is not complete.
g = copy.deepcopy(graph)
g["complete"] = False
check(has(problems(g), "not complete"), "a walk that stopped at its limit fails")

# The pseudo-language is not a language of the device.
g = copy.deepcopy(graph)
g["nodes"].append({"id": len(g["nodes"]), "screen": "home", "lang": "qps-ploc", "depth": 3, "soft": [], "idle": list(KEYS)})
check(has(problems(g), "qps-ploc, which is not a language of the device"), "a state in the pseudo-language fails and names it")

# The language screen must choose each other language, with a key and with a finger.
g = copy.deepcopy(graph)
language = next(n for n in g["nodes"] if n["screen"] == "language" and n["lang"] == "fr")
g["edges"] = [e for e in g["edges"] if not (e["from"] == language["id"] and "tap" in e and g["nodes"][e["to"]]["lang"] == "en")]
found = problems(g)
check(has(found, "in fr the Language screen cannot choose en with a tap") and not has(found, "with a key"), "a language that cannot be chosen by tap fails: " + "; ".join(found[:2]))
g = copy.deepcopy(graph)
g["edges"] = [e for e in g["edges"] if not (g["nodes"][e["from"]]["screen"] == "language" and g["nodes"][e["from"]]["lang"] == "de" and "key" in e and g["nodes"][e["to"]]["lang"] == "fr")]
found = problems(g)
check(has(found, "in de the Language screen cannot choose fr with a key"), "and by key")

# Only the Language screen changes the language: a restart from the Radio page that came back in English was the bug.
g = copy.deepcopy(graph)
radio = next(n for n in g["nodes"] if n["screen"] == "radio" and n["lang"] == "de")
english_home = next(n for n in g["nodes"] if n["screen"] == "home" and n["lang"] == "en")
g["edges"].append({"from": radio["id"], "key": "select", "to": english_home["id"]})
found = problems(g)
check(has(found, f"state {radio['id']} (radio, de): the key leads to state {english_home['id']} in en, and only the Language screen changes the language"),
      "an edge that changes the language outside the Language screen fails and names it: " + "; ".join(found[:2]))
g = copy.deepcopy(graph)
g["edges"].append({"from": radio["id"], "wait": 5, "to": english_home["id"]})
check(has(problems(g), "the wait leads to state"), "so does a wait")

# Every screen must be in the walk in every language.
g = copy.deepcopy(graph)
g["nodes"] = [n for n in g["nodes"]]
victim = next(n for n in g["nodes"] if n["screen"] == "motion" and n["lang"] == "de")
victim["screen"] = "status"
check(has(problems(g), "never reaches the screen motion in de"), "a screen that the walk misses in one language fails")

# Each language starts in its own picture.
g = copy.deepcopy(graph)
g["starts"]["fr"] = g["starts"]["en"]
check(has(problems(g), "the start state of fr is in en"), "a start that is in another language fails")
g = copy.deepcopy(graph)
del g["starts"]["de"]
check(has(problems(g), "no start state in de"), "a language without a start fails")

print(f"{'OK' if failures == 0 else 'FAILED'}: {failures} failure(s)")
sys.exit(1 if failures else 0)
