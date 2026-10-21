# The number page

Code: `src/ui/screens/NumberScreen.{h,cpp}`; tests in `test/test_screen_number/`. A number typed in, for the child who must write to somebody who is in no list. **It leads to a message and never to a call**: calls go to the numbers the profile allows, a security limit of the parents, not of this page. It also leads nowhere unless the profile lets the child choose any recipient (`Profile::freeText`). The brief says "when free dialling is allowed"; the profile has no such switch, and the core refuses any call to a number that is not allowed, so the page reads it as `freeText`.

Twelve large keys (1–9, +, 0, delete), because on the keyboard the digits are on the second layer. The keyboard works too: digits, and the plus only as the first character.

- **A number is never shortened.** At most 15 digits (`phone::kMaxDigits`); a sixteenth is ignored rather than written as a different number. The field takes the largest font the digits fit in.
- Fewer than three digits are a slip of the finger: "Still too short" under the field, and the page stays. Only digits count.
- **Next** hands the number in its canonical form (`+49…`, so the answer lands in the same conversation as one from the book) to the message through the session (`recipient`, not by radio) and opens the compose page.
- Back deletes the last character; with an empty field it goes back. The page opens empty each time.

Soft keys: Back or Delete, Next.

## Tests

`test_screen_number`: typing by key, plus only in front, other characters, the 15-digit limit, the full number drawn, the empty field, delete/back, empty on entering, the keypad and its taps, canonical form (national and with prefix), too short and its note, only digits count, the profile switch, soft keys by touch, layout in four languages, pictures in English and German.
