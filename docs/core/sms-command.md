# Remote query by SMS

Code: `src/app/SmsCommand.{h,cpp}`, tests in `test/test_sms_command/`. Pure C++17: the device is reached through `CommandHost`.

There is exactly one remote query: the location. A message that consists of **nothing but a trigger word** (any case, blanks and line breaks around it allowed) from an **admin number** (a parent in the [contact book](contacts.md)) starts a location reply. Anything else, including "#WhereAreYou today?", is an ordinary message for the child. There is no secret word; the protection is the admin whitelist, which can only check the sender number as the network delivers it ([security.md](../security.md)).

## Trigger words

The words are given by the caller in `Config::triggers` so that every language has its own. The shipped catalogs provide them as the message `sms.command.where` (`#WhereAreYou`, `#WoBistDu`, `#OuEsTu`); a caller collects the word of every language, so a parent may use any of them. The setting `[sms] extra_trigger_words` in `settings.toml` (a list of words, empty by default, taken at the next start; [configuration-reference.md](../configuration-reference.md)) adds words of the parents' own: `AppCore::configureTriggers` appends them to the words of the catalogs.

- Letters A–Z match in either case; other characters (umlauts, accents) must match as written.
- Blanks around a configured word are ignored; empty or blank entries are skipped, because an empty word would match every blank message.
- With no trigger configured, nothing is a command.

## Outcomes

`handle(phone, body)` returns `NotACommand` (store the message) or `Handled` (do not store it as a message). For a command it asks the host and records the outcome:

| Outcome | When | Reply |
|---|---|---|
| `Executed` | an admin asked and the location reply started | the location text, from the SOS flow |
| `Busy` | an admin asked while a location reply or SOS ping runs | none |
| `Unauthorized` | the sender is not an admin, or the number is empty | none: a stranger does not learn that the command exists |

An empty number is unauthorized without asking the host, so a faulty contact book cannot open it.

## Accountability

Every query is passed to `recordCommand` with the time (UTC and uptime), the number and the outcome, executed or refused. That cannot be switched off. The setting `CommandNotice` (`Silent`, `History` by default, `Notify`) only decides how visible the system entry "query received" is on the device; `setConfig()` and `setNotice()` change it at run time and never touch the record. It does not hide the reply: the reply with the position is an ordinary outgoing message and stays in the history and in the card's message log whatever the setting says (D-54).

A device can refuse queries entirely: with `[remote_queries] enabled = false` (default `true`) an admin's query is treated like a refused one: no reply, the outcome is recorded, and the notice setting decides how visible the refusal is. What the device does against strangers and what it cannot know about a forged sender number is in [security.md](../security.md).

The location itself is found by the same flow as the SOS ping with the kind `Query`, not by a separate flow: a second implementation once failed on the device without a known cause, while the SOS flow is proven and reports its failures.
