# Phone numbers and contacts

Code: `src/app/PhoneNumber.{h,cpp}` and `src/app/ContactBook.{h,cpp}`, tests in `test/test_phone_number/` and `test/test_contact_book/`. Pure C++17 without hardware or global state.

The whitelist decides who may reach the child. Both modules return data and problem codes, never text; the interface turns a code into a sentence in the parents' language.

## The central promise

**Only the people in the contact book may write to the device and call it. Nobody else.** No advertising, no sales calls, no strangers, no flooding of the child with messages. This is a security rule of the household and the first thing the device promises the parents; it has no switch and no exception.

`Profile::mayReceiveFrom(number)` answers it for both ways in. A number is allowed when it is in the contact book, whatever its role (parent, family, anybody stored), or is the target of a call action, and it is the same number in every spelling (`0170 1111111`, `0049 170 1111111` and `+491701111111` are one person). Everything else is not allowed: a stranger, a provider's name or short code, an empty sender, a withheld number.

| What arrives | From a number that is not allowed | From an allowed number |
|---|---|---|
| a text | dropped before anything else: not stored, not unread, no vibration, no notice, not offered to the remote query; one log line | as always: stored, vibration, remote query when an admin sends the trigger word |
| a call | hung up at once: no ring, no vibration, no notice, no history entry, no missed call; the screen never changes; one log line | rings; in school time rejected with a notice naming the caller |

The parents can see in the log that something knocked, on every log level: `SMS dropped: sender not in the contact book (+491709999999): Your balance is 3.00 EUR`, `Call rejected: caller not in the contact book (+491709999999)`, `Call rejected: number withheld` ([log-level.md](log-level.md)). **The first line of a sender in ten minutes carries the first 120 characters of the text** of a dropped message, so that a balance, a tariff or a PIN notice can be read there; **dropped text can therefore land in the log, and the log is readable through the web page with the web password.** A call has no text. The child is told nothing. Details: [messaging.md](messaging.md), [calls.md](calls.md).

### What the rule does not do

- **It trusts the sender number the network delivers.** Whoever knows a family number can send from it: a number can be spoofed, and the device cannot tell. The rule keeps out advertising, sales calls, strangers and floods; it is not protection against somebody who knows the family and forges a number.
- **Texts from the provider are dropped too** (balance, tariff, PIN information). The parents read them in the log line, which carries the start of the text (see above).
- **A family member who withholds the number is rejected** like any other hidden call.
- **A flood is cheap for the child and for the log:** at most one line per sender per ten minutes, then one line that counts the rest ("... (17 more in the last 10 minutes)"), and these lines are kept on every log level ([log-level.md](log-level.md)). Many different numbers still write a line each.
- **The radio is a separate path** with its own rule: only the configured peer may write ([messaging.md](messaging.md)); this rule does not cover it.

## Phone numbers

The same number arrives as `+491701111111`, `0049 170 1111111` or `0170-1111111`, depending on the network and on who typed it. `phone::normalize()` reduces all of them to one canonical form so that comparing two numbers is a string comparison.

| Input | Canonical form | Rule |
|---|---|---|
| `+49 170 1111111` | `+491701111111` | separators (space, `-`, `/`, `.`, brackets) are dropped |
| `0049 170 1111111` | `+491701111111` | a leading `00` becomes `+` |
| `0170 1111111` | `+491701111111` | a single leading `0` is replaced by the country code |
| `+49 (0)170 1111111` | `+491701111111` | the bracketed `(0)` right after the country code is dropped |
| `112`, `116 117` | `112`, `116117` | no prefix at all: a short code, left as it is |

The country code for national numbers is `+49` unless `ContactBook::setCountryCode()` sets another one (`+34` and `34` both work).

### Rejected input

`normalize()` returns false, with an empty output, for:

- anything with a letter in it: a sender name such as "Provider" is not a number;
- empty input, a lone `+`, `0`, `00`, and numbers whose country code would start with 0 (`+0…`, `000…`);
- a `+` anywhere but at the start, or twice;
- more than 15 digits (the E.164 limit). Such input is rejected, not shortened: shortening would turn two different long numbers into the same one;
- a national number when the configured country code is not a country code. A misconfiguration therefore makes numbers unreadable, which never matches anyone, instead of matching the wrong person;
- an output buffer that is too small (`phone::kNormalizedSize` bytes always suffice).

### Comparing

`phone::same(a, b, cc)` is true when both normalise and are equal. Two unreadable inputs are never the same, so a caller that hides its number or sends a name matches no contact.

Digits without a `+`, `00` or `0` in front (`491701111111`) are not guessed to carry a country code: they stay as they are and do not match `+491701111111`. A network that reports callers that way would be rejected, and the whitelist stays strict.

### What reaches the modem

The modem takes a dial string only: digits, `*`, `#` and a leading `+` ([at-layer.md](../modem/at-layer.md)). The numbers of the SOS recipients and the call buttons are kept as the parents wrote them, so every text and every call passes through `phone::dialable(number, cc)` on its way to the modem (`Messaging::deliver`, `Calls::call`): the canonical form when the input is a number, the input itself when it is not (the modem refuses it then). `+49 170 1111111` reaches the modem as `+491701111111`, and `0170 1111111` as `+491701111111` too, also abroad, where the visited network would read a national number as its own. Before 4.2.7 such a number was handed over as written: the modem refused it, the SOS reported "sent" for a text that waited in the queue for ever, and the call button did nothing (`test_runtime`: `test_numbers_written_with_blanks_reach_the_modem_as_dial_strings`; the `Modem` contract refuses such a number in every rig).

## The contact book

`phone::ContactBook` holds up to 16 contacts, each with a canonical number, a name of at most 23 bytes and an `admin` flag. Parents are the contacts with `admin`: they may send remote commands. Everyone else on the list may only call and write.

### File format

One contact per line:

```
# Family
+491701111111 admin Parent One
0170 2222222  admin  Parent Two
+493012345678 Grandma
```

- A line starting with `#` (after optional blanks) is a comment. A `#` later in a line is part of the name.
- Fields are separated by blanks or tabs. Lines end with `\n`, `\r\n` or `\r`; empty lines are skipped.
- The **number** comes first and may contain blanks. It ends at the first word that is not made of number characters, so `0170 2222222 3B Parents` is the number `0170 2222222` and the name `3B Parents`. A name made only of digits (`2`) cannot be told from the rest of the number and is read as part of it, so give names at least one letter.
- The word **`admin`**, exactly this spelling, grants parent rights. `Admin`, `ADMIN` and `administrator` do not: a typo leaves a contact without rights, which is the safe way to fail.
- The rest of the line is the **name**. Trailing blanks are removed. A name that is too long is cut at a character boundary, never inside a UTF-8 character. Without a name, the canonical number serves as the name.
- A byte order mark at the start of the file (Windows editors add one) is skipped.

### Problems

`load()` skips lines it cannot use and records them in `problems()`, each with its 1-based line number, counted the way an editor counts:

| Kind | Meaning |
|---|---|
| `UnreadableNumber` | the line does not start with a phone number |
| `DuplicateNumber` | the number is on file already; the first line counts, so a later `admin` line does not take effect |
| `TooManyContacts` | the list is full; reported once, at the first line left out |

At most 16 problems are kept, so a damaged file cannot grow the list.

### Lookup

`find()`, `isKnown()` and `isAdmin()` normalise the number they are given, so the notation does not matter. A hidden number, a name or a null pointer matches nobody.

`displayName()` returns the contact's name; for an unknown caller the number as it was given; and an empty string when there is neither. It never returns null. The interface shows its own wording for "unknown" from the catalog.

`load()` replaces the previous contents. Set the country code before loading: the numbers are normalised when they are read.
