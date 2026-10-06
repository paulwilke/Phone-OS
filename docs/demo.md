# The generated pictures and the click-through demo

Code: `sim/Crawler.h`, the scene scripts `sim/demo/scenes*.txt` and the prelude `sim/demo/world.txt`, the page `sim/demo/page.html`, the generator `scripts/make_demo.py`; tests in `test/test_simulator/` (the walk), `test/test_demo_page/` (the page) and the generator's own checks. Nothing generated is committed: `out/` is ignored, and CI uploads `out/demo` as the artifact `demo`. The pictures are made by running the simulator ([simulator.md](simulator.md)), never by hand.

## Building it

```sh
pio run -e sim
python3 scripts/make_demo.py        # about a minute; writes out/demo/ and prints what it made
```

Open `out/demo/index.html` in a browser. The pages of the user guide may refer to the pictures by the artifact's name but must not need them to be read.

## The walk

`program --crawl <dir>` finds every picture the keys can reach, breadth first. The device has no way to save and restore its state, so each step starts a new simulator on the card, plays the prelude (`sim/demo/world.txt`: two texts and a missed call, so that the lists have something in them), presses the keys of the path and looks at the panel. A state is a picture on a screen; the graph (`graph.json`) has the states, the screen of each, and an edge for every key that leads somewhere else. It is found in English, and the same path is replayed in the other languages: the walk stops with an error if a path leads to another screen in one of them.

Two limits keep it finite and readable. A screen has many states that differ only in a selection or a scroll position, so at most `--per-screen` (30) are kept per screen and the number of the others is in the graph as `trimmed` and on the page. And two kinds of key are not pressed, because they would change the world and not only the picture: any key during a call that is on, and the choice of a ready-made message in the compose page, which sends it. The scenes show those.

## The scenes

The pictures the keys do not lead to need something to happen: a call coming in, school time, a flat battery. `sim/demo/scenes.txt` (the child's device) and the other `scenes*.txt` files (each with a `# data:` line for its card) are scripts that set the world up and call `shot <name>`; the generator runs each once per language, after switching to it.

## The page

`index.html` is one file: the graph, the scene names and the words are embedded, the pictures are beside it (`<language>/n<state>.png`, `<language>/scene-<name>.png`). It shows the picture, the seven keys (enabled where the key leads somewhere), "start over", and the list of scenes; the language selector swaps every picture and every word, and keeps the state. Pictures are drawn without smoothing.

**The page has no words of its own.** Its title, keys, status line and scene names are the `demo.*` keys of the host-only files `tools/lang/demo.<code>.toml` in English, German and French (they are not shipped to the device, so they do not weigh on the limit of a catalog on the card, [i18n.md](i18n.md)), and `test/test_demo_page` fails when the markup holds text or the script holds a sentence. A new scene needs a script line, a `demo.scene-<name>` key in every `tools/lang/demo.<code>.toml` and a row in `SCENE_WORDS` of the generator.

## What the generator checks

`scripts/make_demo.py` exits with 1 when: a word of the page or the name of a scene is missing in a catalog; a scene has no name or a name has no scene; a picture of a state or a scene is missing in a language; the walk never reaches one of the screens the keys lead to (home, lock, clock, phone, call, messages, compose, recipient); the walk has fewer than 60 states; or the number of pictures is not the number of languages times the states and scenes. CI therefore fails when a screen stops being reachable or a translation is missing.

The number-typing page is not in the walk: it opens only on a device that may write to anybody, and the child's card does not.
