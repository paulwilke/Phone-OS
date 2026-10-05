# The phone page

Code: `src/ui/screens/PhoneScreen.{h,cpp}`; tests in `test/test_screen_phone/`. Whom to call: the one place where a call starts. The home tile only opens this page (a press in a pocket never calls); choosing a name dials.

## Who is on it

Only numbers the child may call: a contact, or a number a call action reaches (`Profile::mayCall`). Nothing is typed here.

- **Short list:** the call targets in the order the parents arranged them, then "Other", then "Cancel". A target that is in the contact book is shown by its name, one that is not by its number.
- **Other** opens the rest of the contacts (title "Whom to call? (Other)"); Back returns to the short list, a second Back leaves.
- **Without call targets** the short list is all the contacts, with no "Other".
- A number that is a target and also a contact, in any spelling, is listed once.
- **Nobody:** "No numbers to call", Back only.
- A name too long for its row is shortened with "..."; a number never is. The title wraps instead of running over in a longer language.
- The page opens afresh each time: the short list, the first row.

## Choosing

Select or a tap on a row. A contact row asks the core to dial; **the core decides**. When it refuses — school time ("School time until 15:00"), a number that is not allowed, cellular off, no network — it says why in the notice line and the page stays. When it accepts, the call page opens. "Cancel" goes back.

Soft keys: Back, the arrows, "Pick".

## Tests

`test_screen_phone`: the lists and their order, Other and Back, Cancel, no targets, a target outside the book, duplicates in other spellings, every row dials and is never refused as "not allowed", long names, nobody, afresh on entering, dialling, nothing dials before a choice, the three refusals, taps, soft keys, layout in four languages, pictures in English and German.
