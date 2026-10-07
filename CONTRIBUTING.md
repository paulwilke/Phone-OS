# Contributing

PhonePlus is an emergency phone for children, so a mistake costs more here than in most projects. The rules below keep the code small, readable and testable on a laptop.

## The rules

1. **English everywhere.** Code, comments, documentation, commit messages and test names are English. The only other languages are the interface catalogs in `lang/` and translated documentation in `docs/<language-code>/`.
2. **No personal data, ever.** Examples and tests use invented values: phone numbers `+491701111111`, `+491702222222` and `+493012345678`, the place `48.13745, 11.57550`, a child called "Alex". Real numbers, names, addresses, coordinates, passwords and e-mail addresses never enter the repository, its history or a pull request.
3. **Comments explain why**, never what the next line does. A comment records a decision, a constraint or a bug that was found once.
4. **No leftovers.** No placeholder markers (the words that promise work for later; `scripts/check_docs.py` has the list), no documentation of things that do not exist, no code that nothing uses. Open work belongs in the issue tracker.
5. **Every module has tests.** Logic that can run on a laptop is written so that it does, and `pio test -e native` covers it. A bug that was found once gets a test, and you prove the test by putting the bug back and watching it fail.
6. **Users read text from catalogs.** Anything shown to the user lives in `lang/en.toml`, `lang/de.toml` and `lang/fr.toml` (the device) or `lang/web/*.toml` (the parents' page); see [docs/i18n.md](docs/i18n.md). Pure logic modules contain no user text: they return codes and data, and the interface turns those into words.
7. **Security limits are the maintainer's decision.** Do not weaken a password rule, a path check, a rate limit or a validation on your own initiative. Propose the change and explain why.

## Running the checks

You need [PlatformIO Core](https://docs.platformio.org/page/core/installation.html) and Python 3.11 or newer. Run all of them before every commit; the continuous-integration workflow runs the same eight.

```bash
pio test -e native              # host tests of the C++ code
python3 scripts/check_i18n.py   # language catalogs against the English reference
python3 scripts/check_i18n.py --usage # every catalog key is used by code or a test of its own set; no unknown key in the code
python3 test/test_check_i18n/test_check.py # the catalog check itself (wrong set, both sets, a missing file, the limit)
python3 scripts/check_docs.py   # broken links, missing files, placeholder markers, non-English text, VERSION, screen names against their documents and the runtime, the three guides with the same headings
python3 scripts/check_layers.py # no platform headers in src/, interfaces depend on nothing, drivers include only hal/
python3 scripts/check_behaviour.py --complete # every row of docs/app-behaviour.md is ticked and has its test
python3 scripts/make_font.py --check # generated fonts match assets/fonts/ (regenerate with --all)
python3 scripts/gen_device_assets.py --check # device/ShippedAssets.h matches the catalogs and VERSION
python3 test/test_device_tools/test_tools.py # the USB tools against a scripted console
python3 test/test_check_layers/test_check.py # the layer rules fire on trees that break them
python3 test/test_check_docs/test_check.py # the three guides must keep the same headings: the rule fires when they do not
python3 scripts/privacy_check.py
```

`privacy_check.py` can also read a private denylist of real names and numbers that must never appear; see its header for how to keep one outside the repository.

## Working on a change

- Work on a branch and open a pull request against `main`; never push to `main`.
- Keep a pull request to one subject, and commits small, each with a message that says what changed and why.
- A pull request description lists what it adds, what it deliberately leaves out and why, and anything that was unclear.
- A new module arrives complete: header, implementation, tests, and a section in `docs/` written from the code that describes what it does, its rules and its edge cases.
- Keep the version in [`VERSION`](VERSION) as `major.minor.patch`.
