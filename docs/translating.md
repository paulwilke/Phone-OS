# Translating

The product's words are in `lang/<code>.toml`, one catalog per language, with English as the reference ([i18n.md](i18n.md) has the format: `[meta]`, then `[strings]` with the keys, `{placeholders}` and plural forms). Every shipped language is checked against English by `python3 scripts/check_i18n.py`: the same keys, the same placeholders, only characters the device's fonts can draw.

A language does not have to be shipped to be used: a catalog with a new code on the card, in `/lang/`, adds a language to the device and to the web page, and the configuration can ask for it (`[interface] language = "xx"`).

## The limit

**A catalog on the card may be at most 32,768 bytes** (`ConfigLoader::kMaxCatalogBytes`); a larger file is skipped with an `OversizedFile` diagnostic. The device must not load unlimited text from the card into a small memory. The limit is the owner's decision, to be looked at again with real memory numbers.

What that means today (bytes, `scripts/check_i18n.py` prints them):

| Catalog | Size | |
|---|---|---|
| `lang/en.toml` | 14,591 | fits; **copy it whole** as the start of a new language: change `[meta]`, translate, keep the `{placeholders}` |
| `lang/de.toml` | 16,956 | 572 over |
| `lang/fr.toml` | 17,400 | 1,016 over |

The shipped German and French catalogs are over the limit, so a translation as complete as they are does not fit as one file. What a translator does meanwhile: keep the lines you translated and delete the rest. A message you leave out falls back to English, and `[meta]` is required. A test keeps English under the limit, so the starting point always fits; the words of the demo page are not in these files (they are in `tools/lang/`, which the device never gets, [i18n.md](i18n.md)).

## Try your catalog in the simulator

You need no device. Put the catalog and a configuration that asks for it on a card, and look at the screens.

1. Build the simulator once: `pio run -e sim` ([simulator.md](simulator.md)).
2. Make a directory for the card, with your catalog and the settings of the demo card (the catalog may be `lang/en.toml` with a new code and your words, which fits the limit):

   ```sh
   mkdir -p my-card/lang && cp -r sim/data/config my-card/
   cp my-translation.toml my-card/lang/xx.toml
   ```

   In the copy of `settings.toml` set `language = "xx"` under `[interface]`.
3. Run it: `.pio/build/sim/program --data my-card`. At the prompt, `log` shows what the simulator said about the card: `config: oversized-file /lang/xx.toml` says the catalog is over the limit, `config: user-catalog-error ...` names one that was rejected and why (a syntax error, a `{placeholder}` that English does not have), and `config: bad-catalog-code ...` says the configuration asks for a language nobody has. `language xx` switches to it at any time, `language en` back.
4. Look: `screenshot xx.png`, or `ascii`, or a window with `pio run -e sim-sdl` ([simulator.md](simulator.md)). The messages of the screens that exist today appear as you walk through them: `sms +491701111111 hello` and `key inbox`, `call +491702222222`, `date 2026-09-15 08:00`, `battery 5`. The pseudo-language `language qps-ploc` is longer than any real one: if a screen fits it, it fits yours; one that clips in yours shows as cut-off words.
5. To see the web page, `wifi ap` and open `http://127.0.0.1:8080/` (password `test-password-1`); your language is in its selector.

A script can check what you see, in your language, by key: `see home.phone` is true when the screen says your word for `home.phone`. A scenario (`sim/scripts/`, [simulator.md](simulator.md)) written that way runs in every language, yours included.

What the simulator cannot show is whether a word fits the real panel's glyphs where the shipped fonts differ from the preview, or how it reads in the hand: it draws the device's own bitmap fonts, so the width is the real one, and the rest is for a person to judge.
