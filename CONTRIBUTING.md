# Contributing

PhonePlus is an emergency phone for children, so a mistake costs more here than in most projects. The rules below keep the code small, readable and testable on a laptop.

## The rules

1. **English everywhere.** Code, comments, documentation, commit messages and test names are English. The only other languages are the interface catalogs in `lang/` and translated documentation in `docs/<language-code>/`.
2. **No personal data, ever.** Examples and tests use invented values: phone numbers `+491701111111`, `+491702222222` and `+493012345678`, the place `48.13745, 11.57550`, a child called "Alex". Real numbers, names, addresses, coordinates, passwords and e-mail addresses never enter the repository, its history or a pull request.
3. **Comments explain why**, never what the next line does. A comment records a decision, a constraint or a bug that was found once.
4. **No leftovers.** No placeholder markers (the words that promise work for later; `scripts/check_docs.py` has the list), no documentation of things that do not exist, no code that nothing uses. Open work is listed in [docs/open-points.md](docs/open-points.md), the one place for it, with the person who has to act.
5. **Every module has tests.** Logic that can run on a laptop is written so that it does, and `pio test -e native` covers it. A bug that was found once gets a test, and you prove the test by putting the bug back and watching it fail.
6. **Users read text from catalogs.** Anything shown to the user lives in `lang/en.toml`, `lang/de.toml` and `lang/fr.toml` (the device) or `lang/web/*.toml` (the parents' page); see [docs/i18n.md](docs/i18n.md). Pure logic modules contain no user text: they return codes and data, and the interface turns those into words.
7. **Security limits are the owner's decision.** Do not weaken a password rule, a path check, a rate limit or a validation on your own initiative. Propose the change and explain why.

## Running the checks

You need [PlatformIO Core](https://docs.platformio.org/page/core/installation.html), Python 3.11 or newer and Node.js (for the page tests). Run all of them before every commit; the continuous-integration workflow (`.github/workflows/ci.yml`) runs the same.

```bash
pio test -e native              # host tests of the C++ code
pio run -e sim                  # the simulator builds ...
for s in sim/scripts/*.txt; do .pio/build/sim/program --script "$s" >/dev/null || echo "FAIL $s"; done   # ... and every scenario passes
pio run -e device               # the firmware compiles (compile-only; flashing is the owner's decision)
python3 test/test_translating/test_doc.py # docs/translating.md followed step by step in the simulator, and broken on purpose
python3 scripts/counter_check_scenarios.py # every scenario fails when the behaviour it pins is put back (not part of CI: it rebuilds the simulator)
python3 scripts/make_demo.py    # the pictures and the click-through demo; fails when a picture is missing
node test/test_demo_page/test.mjs # the demo page
python3 scripts/check_i18n.py   # language catalogs against the English reference
python3 scripts/check_i18n.py --usage # every catalog key is used by code or a test of its own set; no unknown key in the code; no sentence as a literal in src/ui/
python3 test/test_check_i18n/test_check.py # the catalog check itself (wrong set, both sets, a missing file, the limit, a sentence in a screen)
python3 scripts/check_docs.py   # broken links, missing files, placeholder markers, non-English text, VERSION with the changelog and the release notes, screen names against their documents and the runtime, every document in docs/README.md, every Python file compiles, a Python or Node test is in `test_ignore` of platformio.ini (a new one is listed in both host environments, or `pio test` reports it as an error), the three guides with the same headings
python3 test/test_check_docs/test_check.py # the guide rule, the index rule, the release rule, the compile rule and the test-ignore rule fire when they are broken
python3 scripts/check_layers.py # no platform headers in src/, interfaces depend on nothing, drivers include only hal/
python3 test/test_check_layers/test_check.py # the layer rules fire on trees that break them
python3 scripts/check_behaviour.py --complete # every row of docs/app-behaviour.md is ticked and has its test
python3 scripts/make_font.py --check # generated fonts match assets/fonts/ (regenerate with --all)
python3 scripts/gen_config.py --check # generated configuration files match config/schema.toml
python3 test/test_schema_gen/test_gen.py # the generator of the configuration files and the page description
python3 scripts/gen_device_assets.py --check # device/ShippedAssets.h matches the catalogs and VERSION
python3 scripts/gen_web.py --check # web/index.html is current
node test/test_web_page/test.mjs # the parents' page
python3 test/test_device_tools/test_tools.py # the USB tools against a scripted console
python3 scripts/privacy_check.py
python3 scripts/privacy_check.py --history # also every changed line, message and committer of the history; run it on a fresh clone of the branch, because it reads every branch of the repository it is in
python3 test/test_privacy_check/test_history.py # the history scan finds a leak in a line, a message and a committer's identity
```

`privacy_check.py` can also read a private denylist of real names and numbers that must never appear; see its header for how to keep one outside the repository.

## Working on a change

- Work on a branch and open a pull request against `main`; never push to `main`.
- Keep a pull request to one subject, and commits small, each with a message that says what changed and why.
- A pull request description lists what it adds, what it deliberately leaves out and why, and anything that was unclear.
- A new module arrives complete: header, implementation, tests, and a section in `docs/` written from the code that describes what it does, its rules and its edge cases.
- Keep the version in [`VERSION`](VERSION) as `major.minor.patch`. Raising it takes a heading of that version at the top of [CHANGELOG.md](CHANGELOG.md), the notes `docs/release-<major>.<minor>.md` and `python3 scripts/gen_device_assets.py`, which carries it into the firmware; `scripts/check_docs.py` and `gen_device_assets.py --check` fail when one of them is missing.
