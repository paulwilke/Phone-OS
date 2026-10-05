# The language page

Code: `src/ui/screens/LanguageScreen.{h,cpp}`; tests in `test/test_screen_language/`. Every language the translator has, by the name it calls itself ("Deutsch", "Français"), so that a child who cannot read the present language finds theirs. The languages parents put on the card (`lang/user/`) are listed like the built-in ones; a name that is too long is shortened with "..." inside its row.

**Choosing applies at once.** The page calls `AppCore::setInterfaceLanguage(code)` and nothing else: it does not switch the translator itself (a test gives the page a translator of its own and shows that only the core's changed). The next frame, this one included, is in the new language. The core keeps the code in the profile (`Profile::interfaceLanguage`) and applies it again at start; writing it to the card is the configuration's job. The language of the SMS is the parents' and does not change.

The page does not close itself, so whoever chose sees the result; Back leaves. "Now: Deutsch" stands under the title, because the focus moves away from the language in use; the page opens with the focus on it. The order is the translator's, by language code.

## Tests

`test_screen_language`: every language by its own name, the focus on the one in use and following it, choosing through the core, the page asking the core only, choosing the one in use, the SMS language staying, touch, back, a language from the card, many languages scrolling to the last, a long name, layout in four languages, pictures in English and German.
