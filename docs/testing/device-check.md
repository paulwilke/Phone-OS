<!-- allow-non-english: holds an example text with umlauts and the German and French words the page is switched to. -->
# The device check

One guide to walk through at the device, in one go, in this order. It is the way the owner closes the open points that need the device ([open-points.md](../open-points.md)): each step says what it is for, what must be true before, what to do with your hands, what you should see and which screen or log line confirms it. The order is chosen so that the device is restarted as seldom as possible and charged only once, at the end.

The guide is reusable. It checks what 4.2.11 holds on the device (D-54, D-56 in [decisions.md](../decisions.md) and the open points named in each step). When a later version changes a step, change the step in the same pull request; when an open point is closed by a pass, remove it from [open-points.md](../open-points.md) as that file says, and write the decision's "Verified" line.

How long it takes: Block A about 45 minutes with the device on the desk, Block B 5 minutes at the cable, Block C is optional and runs overnight.

## 0. Preparation

Do all of this before step 1. Nothing in the steps below is explained twice.

1. **A USB cable** for the device and the computer (the computer needs the repository and `python3`; if `python3` says that `serial` is missing, use `~/.platformio/penv/bin/python` in its place in every command below).
2. **A parent's phone.** Its number must be in `contacts.toml` of the device with `role = parent`, and it must be ticked **SOS** (`sos = true`), so that it gets the SOS text of step 4 and may ask for the position in step 2. Check it on the parents' page (tab **Contacts**) in step 5 if you are unsure, and come back.
3. **Tell the parents** (whoever uses that phone) that during the check an SOS text and a position reply will arrive on it, and that it is a test.
4. **Not in school time.** The SOS test (step 4) is meant for outside school time: the SOS page in school time asks for five taps instead. Do the check at a time the school-time windows do not cover.
5. **Battery well above 20 %** for Block A (Wi-Fi is refused when the battery is too low). For Block C the battery must be fully charged: see there.
6. **Write down the firmware version.** It is on the start line `Boot: the runtime runs, firmware <version>` of the system log (Block B) and on the **Status** tab of the parents' page (line "Firmware"). Write it at the top of your notes, with the date. It should say 4.2.11.
7. **A pen and paper** or a notes file *outside* the repository. Write for each step: pass or fail, and the time. Logs and notes with phone numbers never go into the repository.
8. **A metal tin with a lid** (a biscuit tin, a pan with a lid), or aluminium foil to wrap the device in two layers. It must keep the phone network out: you test that in step 1.
9. **A window sill** with a view of the sky for step 2 (a position needs satellites; indoors the reply may come without one, which still passes the query but shows no map link).

## Block A. The device on the desk (about 45 minutes)

Keep the clock: the moment you switch the device on in step 1 is minute 0. Step 7 restarts the device and must not begin before minute 30, because the loop line of step 2 is written at minute 30.

The device is unlocked by pressing **Enter (↵) twice in a row** (the lock screen says "Press ↵ twice to unlock"). The first page of the home screen holds Phone, Text, SOS and Inbox; the second page holds Wi-Fi, Status, Settings, Motion and Tests (a tap on the right half of the strip under the tiles shows the second page).

### Step 1. A start without a network, and texts that waited (points 39 and 40)

*Purpose:* shows that a device switched on where there is no network keeps searching and then registers, and that three long texts with umlauts that waited at the operator arrive whole.

*Before:* the device is off (second page, **Settings**, box **Switch off**, press it twice; with the cable plugged in it says "unplug USB first", so unplug it). The tin or the foil is ready next to it. The parent's phone is at hand.

*Do:*

1. From the parent's phone send **three texts** to the device's number, each **longer than 70 characters** and each with umlauts, for example:
   - `Schöne Grüße von Mama: Wir holen dich heute um fünf Uhr ab, bitte warte vor der Schule auf uns.`
   - `Größere Änderung: Der Bus fährt heute früher, und Papa wartet schon an der Haltestelle für dich.`
   - `Übermorgen können wir länger üben, wenn du magst, aber zuerst gibt es ein Mittagessen für alle.`

   The device is off, so the operator keeps them.
2. Switch the device on. **Within about 10 seconds** put it in the tin and close the lid (or wrap it in two layers of foil, folded shut at all edges).
3. Wait **5 minutes** (set a timer). Do not open the tin.
4. Take it out. Unlock it if it has locked itself.

*Expect:* within seconds of leaving the tin the three texts arrive. The lock screen goes from "No network" to the operator's name and "Connected".

*Confirms:* the Inbox (middle key under the screen, or the **Inbox** tile) shows the **three texts, each whole**, with the umlauts and the full length. In the log of Block B, in this order: `Cellular: no network found, searching on` **once**, then `Cellular: IMS registered`. There is no `Modem UART: receiving failed` and no `Messages: reading a text failed` anywhere.

*If it does not show the "no network found" line:* the device registered before the lid was closed or the tin lets the signal through. The three texts are then delivered anyway (that half of the step holds), but point 39 is not proved. Repeat only that half later: switch off, close the tin faster or add foil, switch on, five minutes.

### Step 2. Open the motion page, then ask for the position (point 20, D-54 (a))

*Purpose:* the motion page's sensor start blocks the loop for about 11 seconds and the next loop line should say so (point 20). Then the parent's phone asks where the device is and gets an answer (D-54 (a)).

*Before:* the device is out of the tin, unlocked, with a network. Put it on the window sill.

*Do:*

1. Second page of the home screen, tile **Motion**. The page says "Sensor is starting ... (takes about 10 seconds)" and after about 11 seconds shows a ball. Leave with Back. (This is once, as point 20 asks.)
2. From the parent's phone send **exactly** `#WhereAreYou` (nothing before or after, no full stop; capitals do not matter). Wait up to two minutes.

*Expect:* the parent's phone receives a text from the device: the child's name with "is here", the coordinates and a map link, and "accurate to <n> m". With no satellite reception yet it says "No GPS signal after <n> s" and no map link: this still passes the query, but wait on the sill a few minutes and ask once more to see a link.

*Confirms:* on the device the Inbox holds the system entry **Location asked by <name>** (the name of the parent in the contact book) and the sent reply. The log of Block B has a `Loop: longest pass <n> ms` line written after the motion page was opened, with the longest pass of about 11000 ms or more (point 20); the line is written every half hour, about minute 30.

### Step 3. A call through the lock (point 38 (2b))

*Purpose:* a locked device that takes a call is locked again when the call is over.

*Before:* the device is locked. Press nothing and wait until the lock screen is up (the home screen locks itself after about two minutes without a key; **Back** or a tap does not lock it, wait). The lock screen shows the operator and "Press ↵ twice to unlock".

*Do:* call the device from the parent's phone. On the device press **Answer**, talk a few seconds, press **Hang up**.

*Expect and confirm:* the call page covers the lock while it rings and during the call; the three keys under the screen do nothing there. After Hang up, **the lock screen is back** ("Press ↵ twice to unlock"), not the home screen. Test it: touch the screen once; nothing happens.

### Step 4. A call during the SOS countdown (point 38 (3))

*Purpose:* an emergency that nobody chose goes out by itself even if a call comes in during the countdown, and the SOS page shows "Sent" afterwards.

*Before:* outside school time (the SOS page does not say "School time until ..."). The parent's phone is in your other hand with the dial pad open and the device's number typed, ready to call.

*Do:*

1. Unlock the device (↵ twice), first page, tile **SOS**. The page says "Emergency in 40 s" with three choices. **Choose nothing.**
2. At once call the device from the parent's phone. Press **Answer** within the 40 seconds. Talk for a few seconds, press **Hang up**.

*Expect and confirm:* the call page covers the SOS page while it rings; the countdown goes on. After the call the SOS page shows the steps ending in **"Sent"** with "To <name>" (the parent). The parent's phone has received the SOS text ("<child's name> sends SOS", the coordinates or "No GPS signal after ..." and a map link when there was a fix); it may have arrived during the call or right after it. Press Back to leave the SOS page.

### Step 5. The parents' page: help text, languages, the log (D-56, point 42)

*Purpose:* the parents' page asks for its help one tab at a time (D-56); each tab shows a short explanation under its fields; the language can be switched; the whole log can be loaded (point 42).

*Before:* the device is unlocked, the battery is not low.

*Do:*

1. Second page, tile **Wi-Fi**, choose **Access point**. The page shows "Network: ...", "Password: ..." and "Page: http://...". Join that network with the parent's phone or a computer (it has no internet; that is right).
2. Open `http://192.168.4.1` in the browser (the access point's own address).
3. Sign in with the web password (the **Web password** of `secrets.toml`). Wi-Fi switches itself off after 10 minutes without use; the **Status** tab has **Keep Wi-Fi on**.
4. **Click every tab** one after the other: **Status**, **Basics**, **School time**, **Advanced**, **Contacts**, **Secrets**, **Files**, **Log**, **Backup**.
5. Choose the language (the **Language** choice of the page) **German**, then **French**; in each, open **Basics** and **Advanced** again.
6. Open the **Log** tab and press **Load the whole log**.
7. While you are on the **Contacts** tab check that the parent's phone has `role` parent and **SOS** ticked.

*Expect and confirm:*

- On each of **Basics**, **School time**, **Advanced**, **Contacts** and **Secrets** a short explanation (grey text) stands under the fields as soon as the tab opens. A field that has a label but no explanation, or a label that shows a key such as `timezone` or `config.…` instead of words, fails the step; reload the page once, and if it persists, note the tab and the language.
- Every tab has its words in German and in French (for example the tab named "Log" has a German and a French name) and the explanations change language.
- **Load the whole log:** works only as a visible change once the log is above 12 KB (the tab first shows the last 12 KB). If the log is shorter nothing changes visibly: write "log too short" and do not count it as failed. If it is longer, the first line of the file (the oldest start) appears at the top.
- The grey buttons give no visible feedback when pressed (known, point 42); judge by the result.

### Step 6. Vibration, the keys, the wording (points 6, 2 and 15)

*Purpose:* these three are judged by a person holding the device.

*Do:*

1. **Vibration (point 6).** Second page, tile **Tests**, check **Vibration**. The page plays five patterns in turn and names each ("Vibration: click" and so on). Hold the device in a pocket or against your leg, as the child would, and feel them. Expect: click (one short pulse), ring (one long), message (two short), sent (three short), failed (a short and a long). Answer "Felt it?" for each: could you tell all five apart in a pocket?
2. **The `$` and the microphone key (point 2).** In **Tests**, check **Keys**. Press the `$` key: the page names "Arrow up". Press the microphone key: it names "Arrow down". Then on the first page of the home screen use both to move between the tiles, and in the text field of **Text** while typing. Write down whether they feel right or which other key would be better.
3. **The wording (point 15).** You have seen the German and French pages in step 5; now switch the device itself: second page, **Settings**, box **Language**, choose Deutsch and walk through the first page, the lock screen, the SOS page and the Inbox; then Français. Note every word that sounds wrong. The places where an opinion is wanted are listed in [open-points.md](../open-points.md) under "Wording for a native speaker". Set the language back to English when you are done.

*Confirms:* your notes. These three have no log line.

### Step 7. A device that refuses position queries (D-54 (b))

*Purpose:* with `remote_queries.enabled = false` the device does not reply to a position query and shows the attempt in the history. It is last because it needs a restart.

*Before:* at least **30 minutes** have passed since you switched the device on in step 1 (the loop line of step 2). The page is open and signed in.

*Do:*

1. On the page, tab **Basics**, set **Answer position queries** (`remote_queries.enabled`) to **off** (false) and press **Save** (the page answers "Saved.").
2. Tab **Status**, **Restart**. The device restarts; the page loses the connection.
3. When the device is up again (the lock screen shows the operator) send `#WhereAreYou` from the parent's phone again. Wait two minutes.
4. Put it back: unlock, second page, **Wi-Fi**, **Access point**, rejoin, sign in, **Basics**, set **Answer position queries** to **on**, **Save**, **Status**, **Restart**.

*Expect:* **no reply** reaches the parent's phone.

*Confirms:* the Inbox holds the system entry **Query from <name> refused** (and no sent reply). After step 4 send `#WhereAreYou` a last time: the reply comes again and the entry is **Location asked by <name>**. A device that is left with `false` refuses its parents: do not skip step 4.

## Block B. At the cable (5 minutes)

*Purpose:* the log shows what the steps above did; this is where every log line named in Block A is confirmed.

*Before:* the check of Block A is over and the device is on. Plug it in and note that **opening the serial port restarts the device** (that is expected; the log on the card keeps everything).

*Do:*

```bash
python3 scripts/deviceconfig.py read system.log --all > ~/device-check-system.log
```

If it stops with a message that starts "The device answered from byte", run the same command once more. Then look at the lines:

```bash
grep -n -E "Boot: the runtime runs|no network found|IMS registered|Modem sleeps|Loop: longest|WEB: restart|receiving failed|reading a text failed" ~/device-check-system.log
```

(The output file is in your home folder, outside the repository, on purpose.)

*Expect and confirm,* for each start of the device in the output (the first start is step 1):

| Line | Meaning |
|---|---|
| `Boot: the runtime runs, firmware 4.2.11` | the version you wrote down in step 0 |
| `Cellular: no network found, searching on` once after the first start, then `Cellular: IMS registered (PDN8 ...)` | step 1, point 39 |
| `Modem sleeps` after `Cellular: IMS registered` (or about 120 seconds after the registration when the IMS does not come) | point 37; the lock screen said "Connected" within about half a minute of the network |
| `Loop: longest pass ... ms` of about minute 30 with a longest pass of about 11000 ms or more | point 20, step 2 |
| `WEB: restart requested` twice (step 7, two restarts) | step 7 happened |
| **no** `Modem UART: receiving failed` and **no** `Messages: reading a text failed` | point 40: if one appears, copy it with its time and the nearest `Loop:` lines for the notes |

The file holds phone numbers and text fragments: keep it outside the repository and delete it when the notes are done.

## Block C. Optional, overnight

### The discharge test (point 3)

*Purpose:* shows that the fuel gauge says 0 % near the switch-off at 3300 mV, and does not hold 7 % for hours (D-47).

*Before:* the device has been charged on the cable until the charger says "charge done" (`python3 scripts/deviceconfig.py battery` says so). The firmware carries D-47: the system log of the last start says `Battery: the fuel gauge's end-of-discharge thresholds are 3300/3390/3440 mV, compensation off already, nothing to write`.

*Do:*

1. Unplug the cable. Leave the device on, locked, Wi-Fi off, on the desk. Do not touch it. It runs for about a day.
2. It switches itself off at 3300 mV with the picture "Battery empty" (the log says `Battery: <mV> mV for a minute, switching off to protect the cell`).
3. Plug in the cable, switch the device on if it does not start itself. Read the records:

```bash
python3 scripts/deviceconfig.py read energy.log --all > ~/device-check-energy.log
python3 scripts/deviceconfig.py battery
```

*Expect and confirm:* in `~/device-check-energy.log` (one line every ten minutes: epoch, uptime, percent, mV, mA, mAh, state) the last line of the run is the one near 3300 mV; the percentage is **0 % within about 30 minutes of it** (the last three lines show 0 %), and there is **no hold at 7 % for hours**. Count the lines at 7 % with `awk '$3==7' ~/device-check-energy.log | wc -l`: each line is ten minutes, and 18 or more (three hours) fails. A hold longer than before by about 45 minutes is expected while the learned full charge is still below what the cell delivers; it should vanish once that charge is near 1350 mAh. `battery` shows the learned full charge: write it down.

### The current of the idle device (point 25)

Optional: it needs an ammeter in the battery lead. With the device locked and the modem asleep, read the current and compare it with the 55 to 62 mA of the energy log; the comparison with the modem's supply cut needs a test build that does not exist yet (point 25 in [open-points.md](../open-points.md)).

## The summary

| Step | Closes | Confirmation |
|---|---|---|
| 1 | points 39 and 40 (D-40, D-41) | three whole texts in the Inbox; log: `Cellular: no network found, searching on` once, then `Cellular: IMS registered`; no `Modem UART: receiving failed`, no `Messages: reading a text failed` |
| 2 | D-54 (a); point 20 | reply on the parent's phone, Inbox entry "Location asked by <name>"; log: `Loop: longest pass` of about 11000 ms or more after the motion page |
| 3 | point 38 (2b) | the lock screen is back after Hang up |
| 4 | point 38 (3) | SOS page "Sent", SOS text on the parent's phone |
| 5 | D-56; point 42 | explanation under the fields of every tab, in English, German and French; first line of a log above 12 KB after **Load the whole log** |
| 6 | points 6, 2, 15 | your notes: five patterns told apart, "Arrow up" and "Arrow down", the wording |
| 7 | D-54 (b) | no reply, Inbox entry "Query from <name> refused"; log `WEB: restart requested` |
| B | point 37 and the log lines of 1, 2, 7 | `Modem sleeps` after `Cellular: IMS registered`; no `Modem UART: receiving failed`, no `Messages: reading a text failed` |
| C | point 3 (D-47); point 25 | 0 % within about 30 minutes of the switch-off, no hold at 7 % of hours; the ammeter reading |

After the walk, give the notes to the agent. A step that passed is removed from [open-points.md](../open-points.md) and its decision's "Verified" line is written in the same commit; a step that failed stays, with what you saw.
