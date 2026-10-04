# Testing layouts in every language

<!-- allow-non-english: shows accented example text -->

German and French words are often a third longer than the English ones, and a language added later can be longer still. A screen that was drawn and checked in English only breaks on the child's device weeks later. So a layout test does not run once: it runs in every language the interface ships in, plus a pseudo-language that is deliberately worse than any of them.

The helpers are `languages::forEach` (`testing/Languages.h`) and `i18n::pseudoLocalize` / `i18n::pseudoCatalog` (`src/i18n/Pseudo.h`).

## `languages::forEach`

```cpp
languages::forEach([](const std::string& language, i18n::Translator& t) {
  FrameBuffer fb;
  const ui::Font& font = ui::font(ui::FontSize::Text);
  const ui::Rect box{8, 8, 104, font.lineHeight()};

  fb.beginRegion(box);
  ui::text::draw(fb, font, box.x, box.y, t.get("home.inbox"));
  fb.endRegion();

  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
});
```

The body runs once for every catalog in `lang/` — read from disk, as the device reads them, in the order of their file names, so a language added there is tested without touching a test — and then for the pseudo-language. The translator has all of them loaded and the current one selected; `language` is its code, for failure messages. A catalog that does not load fails the test with its file name. Tests run from the repository root, as `pio test -e native` does.

### What a layout test asserts

- **No clipping, in all languages.** The widget declares the rectangle it owns with `beginRegion`; a pixel outside it is recorded by the frame buffer ([ui-display.md](ui-display.md)). Fixed labels are never shortened, so a clip means the label does not fit its place: the layout or the catalog word is wrong.
- **A picture, in English and in the pseudo-language.** The golden files hold the layout under normal and under hostile conditions, and a change to either shows up in review as a changed picture. German and French are asserted on clipping only; their pictures would double the files for little extra information.
- **User data may be shortened, and says where.** A name or a message body that is too long goes through `ui::text::ellipsize` or `wrap` ([ui-text.md](ui-text.md)); the test uses long invented names for that.

## The pseudo-language

`pseudoCatalog(english)` makes a catalog with the code `qps-ploc` from the English one; `languages::forEach` adds it. Every message form is transformed by `pseudoLocalize`:

| What | Example | Why |
|---|---|---|
| Vowels and `c n y` become accented Latin-1 letters | `Inbox` → `Ìñbóx` | accents are taller than plain letters, so a tight line shows; a character the fonts lack shows as a box |
| Every word gets two fifths of its letters appended, rounded up (`ë ö ü ï ä`, cycling) | `Phone` → `Phöñéëö` | German and French run about that much longer than English |
| The message is put in `[ ]` | `[Phöñéëö]` | a missing closing bracket in a picture means the text was cut off |
| `{placeholders}` are left alone; a word that ends in one does not grow | `{name}: busy` → `[{name}: büsýë]` | the caller fills them in at run time, and the catalog check compares them across languages |
| Digits, punctuation, symbols, the euro sign and spaces stay | `12:30`, `↵` | they are not words |

The result uses only characters of the character set ([ui-text.md](ui-text.md)); a test checks every message of the English catalog. A "letter" for the growth rule is an ASCII letter or any two-byte UTF-8 character, so text that is already accented grows too.

The pseudo-language is a test tool: no user can select it, it is not in `lang/`, and no firmware code refers to it.

## Tests

`test/test_pseudo` checks the transformation (the accent table, the growth rule at its edges, brackets, placeholders and escaped braces, multibyte text), that the pseudo-catalog has the keys, plural forms, placeholders and plural rule of English and stays in the character set, that a sentence grows by at least a third in pixels, and that `forEach` visits the shipped languages and then the pseudo-language, with each language's own texts, from the directory it is given.
