# Benutzerhandbuch

Für die Eltern. Dieses Handbuch ist aus dem Code und den übrigen Dokumenten dieses Repositorys geschrieben; Tasten und Seiten heißen so, wie der deutsche Katalog (`lang/de.toml`) sie nennt. Es sagt, was die Software tut. Welche Teile am echten Gerät bestätigt sind, steht hier nicht: das steht in [bring-up.md](../bring-up.md) und [hardware-lessons.md](../hardware-lessons.md) (englisch). Das Handbuch gibt es auch auf Englisch ([user-guide.md](../user-guide.md)) und auf Französisch ([fr/guide-utilisateur.md](../fr/guide-utilisateur.md)), mit demselben Aufbau.

## Was das Gerät ist

Ein Notfalltelefon für ein Kind, auf dem LilyGO T-Deck Pro MAX: ein kleiner Schwarz-Weiß-Bildschirm, der sein Bild ohne Strom behält, eine Tastatur, drei Tasten unter dem Bildschirm, eine SIM-Karte für Anrufe und Texte, ein Satellitenempfänger (GPS) und eine Karte mit den Einstellungen. Es kann wenig und sonst nichts: Das Kind ruft die Menschen an und schreibt ihnen, die die Eltern erlaubt haben, schickt einen Notfall mit seiner Position, und das Gerät ist in der Schulzeit still. Die Eltern richten es am eigenen Telefon im Browser ein.

Der Startbildschirm des Kindes ist ein Raster aus Kacheln, drei pro Reihe, auf zwei Seiten. Die erste Seite gehört dem Kind: **Telefon**, **Text**, **SOS**, **Eingang** und, wenn die Eltern den Funk erlaubt haben, **Funk**. Die zweite Seite gehört dem Gerät: **Netz**, **GPS**, **Uhr**, **Option**, **Beweg.** (wenn das Gerät einen Bewegungssensor hat), **Tests** und **Licht**, und nur dann, wenn die Eltern in den Einstellungen die Elternwerkzeuge eingeschaltet haben, **WLAN** und **Status**. Eine Kachel ohne Aufgabe ist nicht da: Ohne jemanden zum Anrufen gibt es keine Kacheln **Telefon**, **Text** und **SOS**.

## Was das Kind kann

- **Telefon**: aussuchen, wen es anruft, aus der Liste der Eltern. Das Gerät wählt nur diese Liste. Ein Anruf endet mit **Auflegen**; im Gespräch stellt das Kind die **Lautstärke** ein.
- **Text**: aussuchen, wem es schreibt, dann eine von sechs fertigen Nachrichten senden ("Ich bin unterwegs", "Ich bin angekommen", "Ruf mich bitte an", "Ich komme später", "OK", "Danke"). Sie erscheinen in der Sprache des Kindes und gehen in der Sprache ab, die die Eltern für Texte gewählt haben. Wenn die Eltern Freitext eingeschaltet haben (`free_text`), tippt das Kind jeden Text und darf **Andere** und **Neue Nummer** wählen; ein getippter Text hat 160 Zeichen, mit Akzenten nur 70.
- **Eingang**: die eingegangenen und die gesendeten Nachrichten (die Taste O schaltet zwischen **Eingang** und **Gesendet** um, X löscht eine Nachricht), mit **Antworten** und **Rückruf**. Eine Nachricht, die noch nicht abgegangen ist, trägt "(wartet)"; eine, die da ist, "angekommen".
- **Licht**: das Frontlicht des Bildschirms, an und aus.
- **Zurück** auf dem Startbildschirm sperrt das Gerät. Zum Entsperren zweimal hintereinander die Eingabetaste (↵) drücken: Der Bildschirm sagt "Zum Entsperren zweimal ↵ drücken". Eine Berührung des Bildschirms und die drei Tasten darunter entsperren nie, so kann es die Tasche nicht tun. Das Gerät sperrt sich auch selbst nach 120 Sekunden ohne Eingabe, außer während eines Gesprächs. Der Sperrbildschirm zeigt, womit das Gerät verbunden ist, und "2 neue Nachrichten", wenn etwas ungelesen ist.

## Wer anrufen und schreiben darf: das Kontaktbuch

**Nur die Menschen im Kontaktbuch dürfen dem Gerät schreiben und es anrufen. Sonst niemand.** Das Kontaktbuch ist die Liste der Kontakte (`contacts.toml`, der Reiter **Kontakte** der Elternseite) zusammen mit den Nummern der Anrufknöpfe. Einen Schalter dafür gibt es nicht. Ein Text oder ein Anruf von jemand anderem wird verworfen, ohne dass das Kind es erfährt: kein Klingeln, keine Vibration, kein Hinweis; eine Zeile im Protokoll sagt, dass jemand angeklopft hat.

Die Grenzen dieser Regel, damit niemand ihr zu viel zutraut:

- **Eine Nummer lässt sich fälschen.** Das Gerät glaubt die Absendernummer, die das Netz liefert. Wer eine Familiennummer kennt, kann von ihr aus senden. Die Regel hält Werbung, Verkaufsanrufe, Fremde und Überschwemmung fern; sie schützt nicht vor jemandem, der die Familie kennt und eine Nummer fälscht.
- **Auch Texte des Anbieters werden verworfen** (Guthaben, Tarif, PIN-Hinweise). Sie sind nicht verloren: Das Protokoll trägt die ersten 120 Zeichen des ersten verworfenen Textes eines Absenders in zehn Minuten. Lesen Sie sie im Reiter **Protokoll** der Elternseite. Verworfener Text kann also im Protokoll landen, und wer das Web-Passwort hat, kann das Protokoll lesen.
- **Ein Familienmitglied, das die Nummer unterdrückt, wird abgewiesen** wie jeder verdeckte Anruf.
- Eine Flut füllt das Protokoll nicht: eine Zeile je Absender in zehn Minuten, dann eine Zeile, die den Rest zählt.
- Der Funk ist ein eigener Weg mit eigener Regel; er ist aus, solange die Eltern ihn nicht eingeschaltet haben.

Ein Kontakt hat eine Rolle: **parent** (Eltern) oder **family** (Familie) (`role`). Ein Elternteil darf das Gerät auch per Text nach seiner Position fragen (unten); die Familie darf anrufen und schreiben.

## Schulzeit

Die Schulzeit ist eine Menge von Zeitfenstern (`[[quiet_hours]]` in `settings.toml`: `days`, `from`, `to`; zum Beispiel `mon-fri` von `08:00` bis `13:30`; höchstens zwei Fenster je Tag, ein Fenster darf über Mitternacht gehen). Solange eines läuft:

- sagt die Statusleiste "Schulzeit", und der Sperrbildschirm sagt "Schulzeit bis 13:30";
- kann das Kind nicht anrufen: Die Kachel **Telefon** sagt "Schulzeit bis 13:30" und bleibt stehen;
- **wird jeder eingehende Anruf eines Kontakts ohne Klingeln abgewiesen, auch von den Eltern.** Das Kind sieht "(Name) will anrufen", und der Anruf steht im Eingang als "Verpasster Anruf von (Name)", damit ein Elternteil zurückrufen kann;
- kommen Texte weiter an und werden gespeichert; nur ein Text von einem Elternteil lässt das Gerät vibrieren;
- geht **SOS** weiter, braucht aber fünf Tipper (unten).

Ohne bekannte Uhrzeit gibt es keine Schulzeit: Das Gerät hat keine eigene Uhr und nimmt die Zeit vom Netz. Ein Gerät, das gerade ohne Netz gestartet ist, sperrt nicht.

## SOS

Die Kachel **SOS** drücken. Das Gerät beginnt sofort, die Position zu suchen, damit sie bereit ist, wenn das Kind sich entschieden hat. Der Notfall geht an die Nummer des ersten Anrufknopfs (`[[tiles]]` mit `action = "call"`); setzen Sie die Person, die ihn bekommen soll, an die erste Stelle.

Außerhalb der Schulzeit bietet die Seite drei Möglichkeiten: "Notfall! Hier bin ich", "Hol mich hier ab" und "Abbrechen". Wählt das Kind nichts, zählt die Seite "Notfall in 40 s" herunter und schickt den Notfall dann selbst. **Zurück** oder **Abbrechen** sendet nichts. Der Text trägt den Namen des Kindes, einen Kartenlink und die Koordinaten mit ihrer Genauigkeit ("Position auf 12 m genau"); ohne Satellitenempfang sagt er das ("Ohne Position: kein GPS-Empfang") und geht nach der Suchzeit trotzdem ab. Die Seite zeigt die Schritte "Ortung läuft", "Position gefunden", "Gesendet" und an wen ("An (Name)", oder "wartet auf Netz"). Gibt es weder Netz noch Funk, sagt sie "Nicht gesendet" und bietet "Nochmal" an.

In der Schulzeit sagt die Seite "Schulzeit bis 13:30. Das Gerät ist gesperrt." und "Echter Notfall? 5 Mal tippen, jedes Mal höchstens 10 s nach dem letzten." Der fünfte Tipper, jeder höchstens zehn Sekunden nach dem vorigen, sendet den Notfall und hebt die Schulzeit-Sperre bis zum Ende dieser Schulzeit auf (damit das Kind anrufen kann und angerufen werden kann). Vier Tipper bewirken nichts; eine Pause von zehn Sekunden beginnt die Zählung neu. Die Aufhebung steht im Protokoll.

Ein Elternteil kann auch fragen, wo das Kind ist: Ein Text, der aus nichts als `#WoBistDu` besteht (oder `#WhereAreYou`, `#OuEsTu`; Groß- und Kleinschreibung zählt nicht), von der Nummer eines Elternteils, lässt das Gerät mit seiner Position antworten. Ein Fremder bekommt keine Antwort und erfährt nicht, dass es den Befehl gibt. Auf der Seite **Status** sagt die Zeile "Abfragen", wie sichtbar das für das Kind ist: "still", "im Verlauf" oder "mit Hinweis"; die Eingabetaste (Softtaste "Abfragen") schaltet durch die drei.

## Die Elternseite

Die Elternseite ist eine Webseite, die das Gerät über WLAN bereitstellt. Sie ist für das Telefon oder den Computer der Eltern, nie für das Kind.

1. **WLAN einschalten.** Die Kachel **WLAN** liegt auf der zweiten Seite des Startbildschirms und ist nur da, wenn die Elternwerkzeuge an sind (`parent_tools = true` in `settings.toml`). Auf der Seite **WLAN** **Zugangspunkt** wählen (das eigene Netz des Geräts, für einen Ort ohne Heimnetz) oder **Heimnetz** (ein auf der Karte gespeichertes Netz); **Aus** schaltet es aus. Die Seite zeigt dann "Netz: ...", das "Passwort: ..." des Zugangspunkts (bei jedem Start ein neues zufälliges, solange keines in `secrets.toml` steht; es steht nur auf dem Bildschirm des Geräts) und "Seite: http://(Adresse)". Ist der Akku für WLAN zu leer, lehnt das Gerät ab und sagt "Akku zu leer für WLAN".
2. **Beitreten und die Adresse öffnen**, am Telefon der Eltern, in einem Browser.
3. **Anmelden** mit dem Web-Passwort (**Web-Passwort** in der Datei `secrets.toml`, mindestens 4 Zeichen). Ohne gespeichertes Passwort bleibt die Seite gesperrt: Tragen Sie zuerst eines in `secrets.toml` auf der Karte ein. Fünf falsche Passwörter sperren die Seite für sechzig Sekunden. Die Seite spricht unverschlüsseltes HTTP, Passwort und Geheimnisse laufen also unverschlüsselt über das WLAN: Nutzen Sie sie am eigenen Zugangspunkt des Geräts oder in einem Netz, dem Sie trauen.
4. **Die Reiter:** **Status**, **Einstellungen**, **Kontakte**, **Zugangsdaten**, **Dateien**, **Protokoll**, **Sicherung**. Die Einstellungen und die Kontakte sind Formulare; ändern Sie sie und drücken Sie **Speichern**; die Seite antwortet "Gespeichert." und sagt "Ungespeicherte Änderungen", solange eine Änderung nicht gespeichert ist. Eine Datei, die das Gerät beim Start ablehnen würde, wird nie gespeichert. Die meisten Einstellungen gelten nach einem Neustart: Drücken Sie **Neustart** im Reiter **Status**. Nachrichtenvibration und die Sichtbarkeit der Abfragen gelten sofort.
5. **Die Seite offen halten:** Das WLAN schaltet sich nach 10 Minuten ohne Benutzung selbst aus (`auto_off_minutes`); jede Anfrage mit dem Passwort beginnt die Zeit neu, und der Reiter **Status** zeigt "WLAN schaltet sich aus in (n) min" und hat die Taste **WLAN eingeschaltet lassen**. **WLAN ausschalten** beendet es sofort.
6. **Das Protokoll:** Der Reiter **Protokoll** (**Aktualisieren**, **Protokoll löschen**) zeigt, was das Gerät getan hat: abgewiesene Anrufe, verworfene Texte, Änderungen auf der Seite (nur Pfad und Größe, nie der Inhalt), die Aufhebung der Schulzeit nach einem SOS. Die Stufe ist `[log] level`.

## Die Einstellungen

Am Gerät hat die Seite **Optionen** (zweite Seite des Startbildschirms, Kachel **Option**) fünf Kästchen; die Eingabetaste oder ein Tippen ändert das Kästchen im Fokus: **Lautstärke im Gespräch** ("4 von 7"), **Vibration bei Nachrichten**, **Sparmodus**, **WLAN** (öffnet die WLAN-Seite) und **Sprache** (öffnet die Sprachseite, jede Sprache mit ihrem eigenen Namen). Der Sparmodus schaltet Mobilfunk und WLAN aus und behält den Funk; er braucht einen Funk und sagt ohne einen "Erst den Funk einschalten".

Auf der Seite **Status** schaltet die Eingabetaste durch die Sichtbarkeit der Abfragen, und "Ausschalten: x zweimal drücken" schaltet das Gerät aus. Mit eingestecktem USB-Kabel geht das nicht, und es sagt "Ausschalten: erst USB abziehen".

Der Rest steht auf der Elternseite oder in den drei Dateien auf der Karte. Die Schlüssel, die die meisten Eltern brauchen:

| Datei | Schlüssel | Wirkung |
|---|---|---|
| `settings.toml` | `[child] name` | der Name des Kindes in Texten und auf der Statusseite |
| `settings.toml` | `[interface] language` | die Sprache des Geräts |
| `settings.toml` | `[sms] language` | die Sprache der Texte, die das Gerät sendet |
| `settings.toml` | `[features] free_text`, `radio`, `parent_tools` | Freitext und beliebige Empfänger; der Funk; die Kacheln **WLAN** und **Status** |
| `settings.toml` | `[[quiet_hours]]` | die Schulzeit-Fenster |
| `settings.toml` | `[[tiles]]` | die Anrufknöpfe; der erste ist der SOS-Empfänger |
| `settings.toml` | `[time] timezone` | die Zeitzone, für Schulzeit und Uhr |
| `settings.toml` | `[battery] capacity_mah` | die Akkugröße, Voreinstellung 1500 |
| `contacts.toml` | `[[contact]] number`, `name`, `role` | wer anrufen und schreiben darf |
| `secrets.toml` | `sim_pin`, `web_password` | die SIM-PIN und das Passwort der Elternseite |

Jeder Schlüssel ist in [configuration-reference.md](../configuration-reference.md) aufgeführt (englisch). Auf der Seite **Status** sagen "Konfiguration: 1 Problem" und das erste ("settings.toml: child.name prüfen"), dass etwas auf der Karte nicht verstanden wurde; die ganze Liste steht im Protokoll. Was nicht verstanden wurde, wird übergangen, und die Voreinstellung gilt.

## Sprachen und wie man eine hinzufügt

Englisch, Deutsch und Französisch sind im Gerät. Die Sprache des Kindes wählt man auf der Seite **Sprache** oder mit `[interface] language`; die Sprache der Texte an die Eltern ist `[sms] language`; die Elternseite hat eine eigene Auswahl. Eine Sprache, die nicht im Gerät ist, lässt sich ohne Computerprogramm hinzufügen: `lang/en.toml` als `/lang/xx.toml` auf die Karte kopieren, `[meta]` ändern (den Code, die Namen), die Wörter übersetzen und die `{Platzhalter}` behalten; eine Datei darf höchstens 32.768 Byte haben. Die Wörter der Elternseite kommen genauso nach `/lang/web/xx.toml`. Dann `[interface] language = "xx"` setzen und neu starten. Wie man einen Katalog ohne das Gerät ausprobiert, steht in [translating.md](../translating.md) (englisch).

## Akku und Laden

Das Gerät wird über sein USB-Kabel geladen. Die Seite **Status** zeigt "Akku: 80%" (und ", lädt" oder ", voll am Kabel", solange ein Kabel steckt) und "Restzeit: 2 h 30 min". Die Restzeit ist "unbekannt", solange ein Kabel steckt.

Der Messchip des Akkus wird mit einer falschen Akkugröße geliefert. Bei jedem Start vergleicht das Gerät ihn mit `[battery] capacity_mah` (Voreinstellung **1500** mAh) und beschreibt den Chip nur, wenn beide sich unterscheiden. Eine Prozentzahl, der die Zellenspannung widerspricht, wird nicht gezeigt: Die Seite sagt "Akku: unbekannt" statt einer falschen Zahl. Ob das Schreiben am Chip wirklich greift, ist am Gerät nicht bestätigt ([device-power.md](../device-power.md), englisch).

## Kein Netz, keine SIM

- **Kein Netz** ("Kein Netz" auf dem Sperrbildschirm und unter **Netz**): Ein Anruf wird mit dem Hinweis "Kein Netz" abgelehnt; ein Text wartet in einer Warteschlange von acht ("Nachricht wartet auf Netz") und geht ab, wenn das Netz wieder da ist; ein SOS sagt "Nicht gesendet" und lässt sich wiederholen, und der Funk wird genutzt, wenn er an ist.
- **Keine SIM** (**Netz** sagt "SIM: keine Karte" und "Es steckt keine SIM-Karte im Gerät."): Das Gerät kann weder anrufen noch Texte senden. Eine SIM, die ihre PIN braucht, sagt "PIN fehlt": Die PIN ist `sim_pin` in `secrets.toml`, und das Gerät versucht sie einmal je Start. War die PIN falsch ("PIN falsch"), nicht neu starten, bevor sie stimmt: Drei Fehlversuche sperren die SIM, und nur die PUK des Anbieters kann sie dann retten ("gesperrt").
- **Nur SMS** und "Anrufe noch nicht bereit" heißen, dass das Netz dieser SIM keine Anrufe erlaubt oder sie noch nicht bereit gemacht hat; Texte funktionieren.

## Wenn der Bildschirm leer bleibt

Der Bildschirm behält sein letztes Bild ohne Strom, ein Bild, das sich nicht ändert, beweist also nicht, dass das Gerät läuft, und ein leerer Bildschirm heißt nicht immer, dass es tot ist.

1. Das Gerät kann **schlafen oder gesperrt** sein: eine Taste drücken; auf dem Sperrbildschirm zweimal die Eingabetaste.
2. Der Akku kann **leer** sein: das USB-Kabel einstecken und einige Minuten warten.
3. Das Gerät kann auf der Seite **Status** **ausgeschaltet** worden sein: das Kabel einstecken oder es wieder starten.
4. Ohne Karte oder mit einer Karte, die das Gerät nicht lesen kann, startet das Gerät trotzdem, mit den Voreinstellungen: keine Kontakte, keine Anrufknöpfe, also keine Kacheln **Telefon**, **Text** und **SOS**. Die Karte wieder einsetzen, oder das Protokoll lesen wie im nächsten Schritt.
5. Hilft nichts, das USB-Kabel an einen Computer anschließen: Die Konsole des Geräts zeigt das Protokoll und die Dateien der Karte (`scripts/deviceconfig.py`, [device-console.md](../device-console.md), englisch). Das Öffnen des Anschlusses startet das Gerät neu.
