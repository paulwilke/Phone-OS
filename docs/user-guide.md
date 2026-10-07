# User's guide

For the parents. This guide is written from the code and the other documents of this repository; every key and page is named as the English catalog (`lang/en.toml`) names it. It tells what the software does. Which parts have been confirmed on the real device is not told here: that is in [bring-up.md](bring-up.md) and [hardware-lessons.md](hardware-lessons.md). The guide exists in German ([de/benutzerhandbuch.md](de/benutzerhandbuch.md)) and in French ([fr/guide-utilisateur.md](fr/guide-utilisateur.md)) with the same structure.

## What the device is

An emergency phone for a child, on the LilyGO T-Deck Pro MAX: a small black-and-white screen that keeps its picture without power, a keyboard, three keys under the screen, a SIM card for calls and texts, a satellite receiver (GPS) and a card with the settings. It does a few things and nothing else: the child calls and writes to the people the parents allowed, sends an emergency with its position, and the device keeps quiet during school time. The parents set it up from their phone in a browser.

The child's home screen is a grid of tiles, three in a row, on two pages. The first page is the child's: **Phone**, **Text**, **SOS**, **Inbox** and, when the parents allowed the radio, **Radio**. The second page is the device's: **Network**, **GPS**, **Clock**, **Settings**, **Motion** (when the device has a motion sensor), **Tests** and **Light**, and, only when the parents switched on the parent tools in the settings, **Wi-Fi** and **Status**. A tile that has nothing to do is not there: without anybody to call there are no **Phone**, **Text** and **SOS** tiles.

## What the child can do

- **Phone**: choose whom to call from the list the parents made. The device dials only that list. A call is ended with **Hang up**; during a call the child changes the **Volume**.
- **Text**: choose whom to write to, then send one of six ready-made messages ("I am on my way", "I have arrived", "Please call me", "I will be late", "OK", "Thank you"). They are shown in the child's language and sent in the language the parents chose for texts. When the parents switched on free text (`free_text`), the child types any text and may choose **Other** and **New number**; a typed text is 160 characters, only 70 when it has accents.
- **Inbox**: the messages that came in and those that were sent (the O key switches between **Inbox** and **Sent**, X deletes a message), with **Reply** and **Call back**. A message that has not left yet says "(waiting)"; one that arrived says "arrived".
- **Light**: the front light of the screen, on and off.
- **Back** on the home screen locks the device. To unlock, press the Enter key (↵) twice in a row: the screen says "Press ↵ twice to unlock". A touch of the screen and the three keys under it never unlock, so a pocket cannot do it. The device also locks itself after 120 seconds without input, unless a call is going on. The lock screen shows what the device is connected to and "2 new messages" when something is unread.

## Who may call and write: the contact book

**Only the people in the contact book may write to the device and call it. Nobody else.** The contact book is the list of contacts (`contacts.toml`, the **Contacts** tab of the parents' page) together with the numbers of the call buttons. There is no switch for it. A text or a call from anybody else is dropped without the child learning of it: no ring, no vibration, no notice; one line in the log says that something knocked.

The limits of this rule, so that nobody trusts it too much:

- **A number can be forged.** The device believes the sender number the network hands over. Whoever knows a family number can send from it. The rule keeps out advertising, sales calls, strangers and floods; it is not protection against somebody who knows the family and forges a number.
- **Texts from the provider are dropped too** (balance, tariff, PIN notices). They are not lost: the log carries the first 120 characters of the first dropped text of a sender in ten minutes. Read them on the **Log** tab of the parents' page. Dropped text can therefore land in the log, and whoever has the web password can read the log.
- **A family member who withholds the number is rejected** like any other hidden call.
- A flood does not fill the log: one line per sender in ten minutes, then one line that counts the rest.
- The radio is a separate path with its own rule; it is off unless the parents switched it on.

A contact has a role: **parent** or **family** (`role`). A parent may also ask the device for its position by text (below); family may call and write.

## School time

School time is a set of windows (`[[quiet_hours]]` in `settings.toml`: `days`, `from`, `to`, `enabled`; for example `mon-fri` from `08:00` to `13:30`; at most two windows per day, and a window may cross midnight). A window counts only while it is **active**: on the parents' page each window has an "Active" checkbox (`enabled = true` or `false`; a window without the key is active). A device without any window starts with one built-in window, Monday to Friday 08:00 to 12:30, **not active**: no school time runs until a parent ticks it and saves. While an active window runs:

- the status bar says "School time" and the lock page says "School time until 13:30";
- the child cannot call: the **Phone** tile says "School time until 13:30" and stays;
- **every incoming call from a contact is rejected without ringing, parents included.** The child sees "(name) is trying to call", and the call is in the inbox as "Missed call from (name)", so a parent can call back;
- texts still arrive and are stored; only a text from a parent makes the device vibrate;
- **SOS** still works, but needs five taps (below).

Without a known time of day there is no school time: the device has no clock of its own and takes the time from the network. A device that has just started without network does not lock.

## SOS

Press the **SOS** tile. The device starts looking for the position at once, so that it is ready when the child has decided. The emergency goes to the number of the first call button (`[[tiles]]` with `action = "call"`); put the person who should receive it first.

Outside school time the page offers three choices: "Emergency! I am here", "Come and get me" and "Cancel". If the child chooses nothing, the page counts "Emergency in 40 s" and then sends the emergency by itself. **Back** or **Cancel** sends nothing. The text carries the child's name, a map link and the coordinates with their accuracy ("Position accurate to 12 m"); with no satellite reception it says so ("No position: no GPS reception") and goes out anyway after the search time. The page shows the steps "Locating", "Position found", "Sent", and to whom ("To (name)", or "waits for network"). If there is no network and no radio it says "Not sent" and offers "Try again".

In school time the page says "School time until 13:30. The device is locked." and "A real emergency? Tap 5 times, each within 10 s of the last." The fifth tap, each within ten seconds of the one before, sends the emergency and lifts the school-time lock until the end of that school time (so that the child can call and be called). Four taps do nothing; a pause of ten seconds starts the count again. The lift is written to the log.

A parent can also ask where the child is: a text that consists of nothing but `#WhereAreYou` (or `#WoBistDu`, `#OuEsTu`; capitals do not matter), sent from a parent's number, makes the device answer with its position. A stranger gets no answer and does not learn that the command exists. On the **Status** page the line "Remote queries" says how visible this is to the child: "silent", "in the history" or "with a notice"; Enter (soft key "Queries") steps through the three.

## The parents' page

The parents' page is a web page that the device serves over Wi-Fi. It is for the parents' phone or computer, never for the child.

1. **Switch Wi-Fi on.** The **Wi-Fi** tile is on the second page of the home screen and exists only when the parent tools are on (`parent_tools = true` in `settings.toml`). On the page **Wi-Fi** choose **Access point** (the device's own network, for a place without a home network) or **Home network** (a network saved on the card); **Off** switches it off. The page then shows the "Network: ...", the "Password: ..." of the access point (a new random one at every start, unless one is set in `secrets.toml`; it is shown on the device's screen only) and "Page: http://(address)". With a battery too low for Wi-Fi the device refuses and says "Battery too low for Wi-Fi".
2. **Join and open the address** on the parents' phone, in a browser.
3. **Sign in** with the web password (**Web password** in the form `secrets.toml`, at least 4 characters). Without a stored password the page stays locked: put one into `secrets.toml` on the card first. Five wrong passwords lock the page for sixty seconds. The page speaks plain HTTP, so the password and the secrets cross the Wi-Fi unencrypted: use it on the device's own access point or on a network you trust.
4. **The tabs:** **Status**, **Settings**, **Contacts**, **Secrets**, **Files**, **Log**, **Backup**. The settings and the contacts are forms; change them and press **Save**; the page answers "Saved." and says "Unsaved changes" while a change is not saved. A file the device would refuse at start is never stored. Most settings take effect after a restart: press **Restart** on the **Status** tab. Message vibration and the visibility of remote queries take effect at once.
5. **Keep the page open:** Wi-Fi switches itself off after 10 minutes without use (`auto_off_minutes`); every request that has the password starts the time again, and the **Status** tab shows "Wi-Fi switches off in (n) min" and has the button **Keep Wi-Fi on**. **Switch Wi-Fi off** ends it at once.
6. **The log:** the **Log** tab (**Refresh**, **Clear log**) shows what the device did: calls refused, texts dropped, changes made on the page (path and size only, never the content), the lift of school time after an SOS. The level is `[log] level`.

## The settings

On the device, the page **Options** (second page of the home screen, tile **Settings**) has five boxes; Enter or a tap changes the one in focus: **Volume in calls** ("4 of 7"), **Message vibration**, **Save mode**, **Wi-Fi** (opens the Wi-Fi page) and **Language** (opens the language page, each language by its own name). Save mode turns cellular and Wi-Fi off and keeps the radio; it needs a radio and says "Switch the radio on first" without one.

On the **Status** page, Enter steps through the visibility of remote queries, and "Switch off: press x twice" switches the device off. With the USB cable plugged in it cannot, and says "Switch off: unplug USB first". "Restart: press r twice" restarts the device (press r again at "Restart? Press r again"); any other key takes the first press back, and a restart works with the cable plugged in.

The rest is on the parents' page or in the three files on the card. The keys most parents use:

| File | Key | What it does |
|---|---|---|
| `settings.toml` | `[child] name` | the child's name in texts and on the status page |
| `settings.toml` | `[interface] language` | the language of the device |
| `settings.toml` | `[sms] language` | the language of the texts the device sends |
| `settings.toml` | `[features] free_text`, `radio`, `parent_tools` | free texts and any recipient; the radio; the Wi-Fi and **Status** tiles |
| `settings.toml` | `[[quiet_hours]]` | the school-time windows |
| `settings.toml` | `[[tiles]]` | the call buttons; the first is the SOS recipient |
| `settings.toml` | `[time] timezone` | the time zone, for school time and the clock |
| `settings.toml` | `[battery] capacity_mah` | the battery size, default 1500 |
| `contacts.toml` | `[[contact]] number`, `name`, `role` | who may call and write |
| `secrets.toml` | `sim_pin`, `web_password` | the SIM PIN and the password of the parents' page |

Every key is listed in [configuration-reference.md](configuration-reference.md). On the **Status** page, "Configuration: 1 problem" and the first one ("settings.toml: check child.name") tell that something on the card was not understood; the whole list is in the log. What was not understood is ignored and the default applies.

## Languages and how to add one

English, German and French are in the device. The child's language is chosen on the page **Language** or with `[interface] language`; the language of the texts to the parents is `[sms] language`; the parents' page has its own selector. A language that is not in the device can be added without a computer program: copy `lang/en.toml` to the card as `/lang/xx.toml`, change `[meta]` (the code, the names), translate the words and keep the `{placeholders}`; a file may be at most 32,768 bytes. The words of the parents' page go to `/lang/web/xx.toml` in the same way. Then set `[interface] language = "xx"` and restart. How to try a catalog without the device is in [translating.md](translating.md).

## Battery and charging

The device is charged through its USB cable. The **Status** page shows "Battery: 80%" (and ", charging" or ", full on cable" while a cable is plugged in) and "Remaining: 2 h 30 min". The time remaining is "unknown" while a cable is plugged in.

The battery's gauge chip is delivered with a wrong battery size. At every start the device compares it with `[battery] capacity_mah` (the default is **1500** mAh) and writes the chip only when they differ. A percentage that the cell voltage contradicts is not shown: the page says "Battery: unknown" instead of a wrong number. Whether the write really takes on the chip is not confirmed on the device ([device-power.md](device-power.md)).

## No network, no SIM

- **No network** ("No network" on the lock page and on **Network**): a call is refused with the notice "No network"; a text is kept in a queue of eight ("Message waits for network") and goes out when the network is back; an SOS says "Not sent" and can be tried again, and the radio is used when it is on.
- **No SIM** (**Network** says "SIM: no card" and "There is no SIM card in the device."): the device cannot call or text. A SIM that needs its PIN says "PIN missing": the PIN is `sim_pin` in `secrets.toml`, and the device tries it once per start. If the PIN was wrong ("PIN wrong"), do not restart until it is right: three wrong tries lock the SIM, and only the provider's PUK can then unlock it ("locked").
- **SMS only** and "calls not ready" mean that the network does not allow calls for this SIM, or has not made them ready yet; texts work.

## When the screen stays blank

The screen keeps its last picture without power, so a picture that does not change does not prove that the device runs, and a blank screen does not always mean it is dead.

1. The device may be **asleep or locked**: press a key; on a lock screen press Enter twice.
2. The battery may be **empty**: plug in the USB cable and wait a few minutes.
3. The device may have been **switched off** on the **Status** page: plug in the cable or start it again.
4. With no card or a card the device cannot read, the device still starts, with defaults: no contacts, no call buttons, so no **Phone**, **Text** and **SOS** tiles. Put the card in again, or read the log as in the next step.
5. If nothing helps, connect the USB cable to a computer: the device's console shows the log and the card's files (`scripts/deviceconfig.py`, [device-console.md](device-console.md)). Opening the port restarts the device.
