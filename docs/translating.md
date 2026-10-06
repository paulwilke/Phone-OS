# Translating

The product's words are in **two files per language**, with English as the reference of each ([i18n.md](i18n.md) has the format: `[meta]`, then `[strings]` with the keys, `{placeholders}` and plural forms):

| File | What it says | Who reads it |
|---|---|---|
| `lang/<code>.toml` | the device: its screens, notices, SMS texts, system entries | the child, and the parents in the SMS |
| `lang/web/<code>.toml` | the parents' web page and the configuration form (`web.*`, `config.*`) | the parents, in the browser |

**A translator does the device first, then the page, and each can be done alone.** A language with only the device file has a device in its language and a page in English; one with only the page file has a page in the parents' language and a device in English (the page file alone does not add a language to the device's picker). Every shipped language is checked against English by `python3 scripts/check_i18n.py`, each set against its own English file: the same keys, the same placeholders, and (device file only) only characters the device's fonts can draw; the page goes to a browser and has no such limit.

A language does not have to be shipped to be used: a catalog with a new code on the card, in `/lang/`, adds a language to the device and to the web page, and the configuration can ask for it (`[interface] language = "xx"`). The words of the page go on the card as `/lang/web/xx.toml`; the page reads them when it is opened, so a change shows after the device has been restarted (as the page asks for after saving the language).

## The limit

**A catalog on the card may be at most 32,768 bytes** (`ConfigLoader::kMaxCatalogBytes`), **each file for itself** (the device's and the page's are not added up); a larger file is skipped with an `OversizedFile` diagnostic. The device must not load unlimited text from the card into a small memory. The limit is the owner's decision, to be looked at again with real memory numbers.

What that means today (bytes, `scripts/check_i18n.py` prints them):

| Catalog | Size | |
|---|---|---|
| `lang/en.toml` | 13,396 | fits; **copy it whole** as the start of a new device language: change `[meta]`, translate, keep the `{placeholders}` |
| `lang/de.toml` | 14,296 | fits |
| `lang/fr.toml` | 14,887 | fits |
| `lang/web/en.toml` | 8,441 | fits; the start of a new page language |
| `lang/web/de.toml` | 9,989 | fits |
| `lang/web/fr.toml` | 10,217 | fits |

Every shipped file fits the limit, so a translation as complete as the shipped ones fits as two files. A message you leave out of a file falls back to English, and `[meta]` is required. A test keeps both English files under the limit, so the starting points always fit; the words of the demo page are not in these files (they are in `tools/lang/`, which the device never gets, [i18n.md](i18n.md)).

## Try your catalog in the simulator

You need no device. Put the catalog and a configuration that asks for it on a card, and look at the screens.

1. Build the simulator once: `pio run -e sim` ([simulator.md](simulator.md)).
2. Make a directory for the card, with your catalog and the settings of the demo card (the catalog may be `lang/en.toml` with a new code and your words, which fits the limit; the page's words are `lang/web/en.toml` the same way, put on the card as `/lang/web/xx.toml`, in a `lang/` folder that already has a file in it):

   ```sh
   mkdir -p my-card/lang && cp -r sim/data/config my-card/
   cp my-translation.toml my-card/lang/xx.toml
   ```

   In the copy of `settings.toml` set `language = "xx"` under `[interface]`.
3. Run it: `.pio/build/sim/program --data my-card`. At the prompt, `log` shows what the simulator said about the card: `config: oversized-file /lang/xx.toml` (or `/lang/web/xx.toml`, logged when the page is first opened) says the catalog is over the limit, `config: user-catalog-error ...` names one that was rejected and why (a syntax error, a `{placeholder}` that English does not have), and `config: bad-catalog-code ...` says the configuration asks for a language nobody has. `language xx` switches to it at any time, `language en` back.
4. Look: `screenshot xx.png`, or `ascii`, or a window with `pio run -e sim-sdl` ([simulator.md](simulator.md)). The messages of the screens that exist today appear as you walk through them: `sms +491701111111 hello` and `key inbox`, `call +491702222222`, `date 2026-09-15 08:00`, `battery 5`. The pseudo-language `language qps-ploc` is longer than any real one: if a screen fits it, it fits yours; one that clips in yours shows as cut-off words.
5. To see the web page, `wifi ap` and open `http://127.0.0.1:8080/` (password `test-password-1`); your language is in its selector.

A script can check what you see, in your language, by key: `see home.phone` is true when the screen says your word for `home.phone`. A scenario (`sim/scripts/`, [simulator.md](simulator.md)) written that way runs in every language, yours included.

What the simulator cannot show is whether a word fits the real panel's glyphs where the shipped fonts differ from the preview, or how it reads in the hand: it draws the device's own bitmap fonts, so the width is the real one, and the rest is for a person to judge.
