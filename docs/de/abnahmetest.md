# Abnahmetest

Der Test, den der Eigentümer nach jedem Aufspielen einer neuen Version am Gerät durchgeht, immer in derselben Reihenfolge. In die Spalten kommen das Datum und das Ergebnis ("ok" oder was anders war). Was hier steht, ist geprüft, wenn es hier ein Datum hat, und sonst nicht. Was sich mit Version 4.1 geändert hat, steht in [../release-4.1.md](../release-4.1.md).

Vorbereitung: SIM-Karte und SD-Karte stecken, das Gerät am USB-Kabel. Zwei Kontakte sind in `contacts.toml` bei **SOS** angehakt, ein Elternteil hat ein zweites Telefon zur Hand. Die Elternseite wird mit dem eigenen Passwort geöffnet, über WLAN.

## 1. Start und Netz

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 1.1 | Neue Firmware aufspielen, Gerät startet | zuerst das Startbild "Phone OS, T-Deck Pro MAX, Version ...", dann der Startbildschirm; Statusseite (Kachel **Status**) zeigt die neue Version | | |
| 1.2 | Gleich nach dem Start auf die Statuszeile oben links sehen | "Verbinde ..." bis Netz und Anrufe bereit sind, kein Springen zwischen "Netz", "Kein Netz" und "Noch keine Anrufe" | | |
| 1.3 | Etwa eine Minute warten | oben links der Netzbetreiber ("Telekom"), Kachel **Netz**: "IMS: ja" | | |
| 1.4 | GPS-Anzeige oben | "GPS" nur in den ersten Minuten nach dem Start (Start-Fix) und während eines SOS, sonst nicht | | |

## 2. Startbildschirm und Tasten

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 2.1 | Startbildschirm ansehen | Seite 1: Telefon, Text, SOS, Eingang, (Funk), Licht; unten zwei breite Balken, keine Pfeile in der Tastenleiste | | |
| 2.2 | Auf die rechte Hälfte des Streifens unter den Kacheln tippen, dann auf die linke | Seite 2 erscheint, dann wieder Seite 1; nichts öffnet sich dabei | | |
| 2.3 | Nur mit den Tasten ▲ ▼ durch alle Kacheln gehen | jede Kachel wird erreicht, beide Seiten | | |
| 2.4 | **Licht** antippen, zweimal | Licht an, Licht aus | | |
| 2.5 | Mehrere Seiten öffnen (Netz, GPS, Optionen, Eingang) | **Zurück** steht immer links an derselben Stelle; die Tastenleiste ist gut zu treffen, nichts ist abgeschnitten | | |

## 3. Telefonieren

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 3.1 | Einen Elternteil anrufen, er nimmt ab | Sprache in beide Richtungen | | |
| 3.2 | Im Gespräch **Leiser** und **Lauter** antippen | Kästchen werden weniger / mehr, Lautstärke ändert sich; die Tasten ▲ ▼ tun dasselbe | | |
| 3.3 | **Auflegen** über die ganze Leiste antippen (auch rechts) | Gespräch endet | | |
| 3.4 | Vom Elternteil anrufen lassen, **Annehmen** | klingelt, Gespräch steht | | |

## 4. Schreiben

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 4.1 | Mit Freitext (`free_text = true`): **Text**, Empfänger wählen | Seite öffnet auf dem Text, rechte Taste heißt **Vorlagen** | | |
| 4.2 | Sofort etwas tippen, **Senden** | Text kommt beim Elternteil an; **Zurück** blieb beim Tippen an seiner Stelle | | |
| 4.3 | Neu öffnen, **Vorlagen**, eine fertige Nachricht wählen | sie geht ab, in der Sprache der Eltern | | |
| 4.4 | In der Liste der Vorlagen einen Buchstaben tippen | zurück im Text, der Buchstabe steht da | | |

## 5. SOS

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 5.1 | Draußen **SOS**, nichts wählen | nach 40 s geht der Notruf von selbst; beide angehakten Kontakte bekommen denselben Text mit Position und Genauigkeit | | |
| 5.2 | Protokoll lesen (Reiter **Protokoll**) | "SOS text to 2 of 2 recipients", "SOS ping sent" | | |
| 5.3 | Gerät aus und wieder an, gleich **SOS** | Notruf geht nach etwa 40 s mit Position, spätestens etwa 160 s nach dem Drücken (40 s Auswahl, dann bis zu 120 s Suche) | | |
| 5.4 | Erst draußen eine Position bekommen (GPS-Seite oder SOS), dann im Haus **SOS** | ohne aktuelle Position geht die **LETZTE BEKANNTE Position** mit, mit Alter ("vor 12 Min.") und Genauigkeit | | |

## 6. Optionen

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 6.1 | Kachel **Option**, ganz nach unten | letztes Kästchen **Ausschalten**, am Kabel "erst USB abziehen" und nichts passiert | | |
| 6.2 | Kabel ab, **Ausschalten** zweimal drücken | erst "noch einmal: aus", dann das Bild "Phone OS ... Ausgeschaltet", dann ist das Gerät aus (Licht aus, keine Reaktion auf Tasten) | | |
| 6.3 | Mit der seitlichen Taste einschalten | Gerät startet wieder | | |

## 7. Schulzeit

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 7.1 | Elternseite, Reiter **Schulzeit**: ein Fenster für heute, jetzt bis in 10 Minuten, Tag angehakt, **aktiv**, speichern, Neustart | Statuszeile "Schulzeit"; ein Anruf eines Elternteils klingelt nicht | | |
| 7.2 | Häkchen **aktiv** entfernen, speichern, Neustart | keine Schulzeit mehr | | |

## 8. Elternseite über WLAN

| Nr. | Schritt | Erwartung | Datum | Ergebnis |
|---|---|---|---|---|
| 8.1 | WLAN am Gerät an, Seite öffnen, anmelden | alle Reiter beschriftet (Status, Allgemein, Schulzeit, Erweitert, Kontakte, Zugangsdaten, Dateien, Protokoll, Sicherung); Protokoll zeigt "Web catalogs loaded" | | |
| 8.2 | **Allgemein**: SMS-Sprache ändern, speichern; **Erweitert**: Zeitzone ändern, speichern | im Reiter **Dateien** zeigt `settings.toml` beide Änderungen; nichts anderes ging verloren | | |
| 8.3 | In **Allgemein** etwas ändern, nicht speichern, zu **Erweitert** und zurück | "Ungespeicherte Änderungen", die Änderung ist noch da | | |
| 8.4 | **Erweitert**: Kachel "GPS" abhaken, speichern, Neustart | die Kachel GPS fehlt auf Seite 2; wieder anhaken | | |
| 8.5 | **Kontakte**: ein Kästchen **SOS** an- und abhaken, speichern | `contacts.toml` hat `sos = true` / `false` | | |
| 8.6 | **Dateien**: Karte vorher an einem Mac gehabt | `._`-Dateien und Mac-Ordner nicht zu sehen; mit **Versteckte Dateien zeigen** sichtbar; **Mac-Reste entfernen** löscht die `._`-Dateien, die Ordner werden genannt | | |
| 8.7 | **Sicherung**: exportieren, die ZIP-Datei am Computer öffnen | Ordner `config` mit `settings.toml` und `contacts.toml`, ohne `secrets.toml` | | |
| 8.8 | Dieselbe Datei wiederherstellen | "Wiederhergestellt."; Protokoll zeigt die geschriebenen Dateien | | |
| 8.9 | Elternseite über das USB-Kabel (`scripts/webbridge.py`) | dieselbe Seite, auch wenn die Kachel WLAN ausgeblendet ist | | |
