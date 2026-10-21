# Whom to write to

Code: `src/ui/screens/RecipientScreen.{h,cpp}`; tests in `test/test_screen_recipient/`. The page between the "Text" tile and the compose page. It lists the people the child may write to — the contacts and the numbers a call action reaches, the call targets first in the parents' order — then "Other" for the rest, and "Cancel". Choosing leaves the number in the session (`Session::recipient`, not by radio) and opens the compose page.

Where the profile lets the child choose any recipient (`freeText`), the list behind "Other" ends with **"New number"**, which opens the number page (the number page); without that switch there is no way to a number that is in no list. Radio contacts are chosen on the radio page, which also leaves them in the session.

The lists, names and numbers are written as on the phone page: a name is shortened with "...", a number never is, a target in no book is shown by its number, the page opens afresh each time. Nobody to write to: "Nobody to write to".

## Tests

`test_screen_recipient`: the short list; choosing leaves the number in the session and writes nothing to the core's calls; a contact behind Other; Back and Cancel; New number with and without free text; nobody; a target in no book; no call targets; taps; soft keys in two languages; layout in four languages; pictures in English and German.
