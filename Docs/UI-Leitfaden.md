# UI-Leitfaden – der clou 3

Verbindliche Regeln für das Interface (Slate in C++), Stand 2026-11-07. Farben, Schriften und Bausteine
nur zentral in `DCItemWidgets.h/.cpp` anlegen (InfoPanel, RadialMenu, NoiseHud nutzen noch eigene Literale).

## 1. Leitprinzipien

1. **Von der Aufgabe aus denken:** Figur wählen, Schritt planen, abstimmen, abspielen, prüfen – jede Anzeige dient einer davon.
2. **Eine Sache, ein Ort:** jede Aktion hat genau einen Einstieg; Doppeltes (Titel, Hinweise, Infos) fliegt raus.
3. **Hierarchie statt Textwand:** pro Bereich eine hervorgehobene Hauptaktion, der Rest ruhiger, hinter „⋯“ oder im Tooltip.
4. **Erst sehen, dann lesen:** Zustand über Form, Platz und Symbol; Farbe nie als einziger Träger.
5. **Geführt, wo es knifflig ist:** Auswahl → Vorschau in einem Satz → „Übernehmen“ / „Abbrechen“.
6. **Sofortige Rückmeldung:** jede Eingabe zeigt sofort, was passiert ist – oder warum nicht und was hilft.
7. **Fehler verzeihen:** Zerstörendes nur mit Rückfrage oder Rückgängig; der Modus ist immer eindeutig sichtbar.
8. **Feste Plätze:** nichts springt bei Modus- oder Inhaltswechsel (ausgrauen statt ausblenden, feste Breiten).
9. **Lesbarkeit von oben:** die UI verdeckt nie die freie Spielfläche; Klickziele bleiben aus der Draufsicht klar.

## 2. Bildschirmzonen

```
+------------------------------------------------------------------------------+
| [1 Vic   ]                 [ Status: Alarm · Lärm · Misstrauen ]  [ Kontext ] |
| [2 Rosa  ] [Profil /                                                [Info |  ] |
| [3 …     ]  Inventar]                                          [Durchsuchen] |
|                             freie Spielfläche                                |
|                      [ Dialog – mittig, nur bei Bedarf ]                     |
|                      [ Meldung (Toast), 3 s ]                                |
| [Planen | Ausprobieren]  [▶ Abspielen] [Tempo] [Warten …] [⋯]      Plan 0:25  |
| Schrittliste der gewählten Figur   | Zeitleiste aller Figuren (füllt Breite) |
+------------------------------------------------------------------------------+
```

| Zone | Inhalt | Regeln |
|---|---|---|
| Links, Spalte | **Profilkarten** der Crew | feste Reihenfolge, Breite ca. 220, Rand 16; beginnt oben, endet über der Leiste |
| Neben den Karten | **Profil & Inventar** | klappt rechts an der Kartenspalte auf („Schublade“), nie über den Karten |
| Unten, volle Breite | **Planungsleiste** | feste Höhe (max. ca. 25 % des Bildschirms), feste Breite in allen Modi |
| Oben Mitte | **Status** | Alarm (rot, groß), letzter Lärm, Misstrauen der Zuhörer – immer hier, nicht in der Welt am Rand |
| Rechts oben | **Kontext**: Info oder Durchsuchen | genau ein Panel; mehrere Inhalte als Reiter, nie zwei Fenster im selben Platz |
| Mitte | **Dialoge** | Breite 480–560, Welt dahinter leicht abgedunkelt; Zeitleiste bleibt sichtbar, wenn der Dialog sie braucht |
| Über der Leiste, mittig | **Meldungen (Toasts)** | max. 2 gleichzeitig, verschwinden nach ca. 3 s, Fehler rot |
| Am Cursor | **Radialmenü**, Tooltips | am Bildschirmrand und an der Leiste nach innen schieben |
| Welt | Name, kurzer Zustand, Plan-Marker | siehe Abschnitt 5 „Weltbeschriftung“ |

Die Kamera rahmt den Raum in die freie Spielfläche (zwischen Kartenspalte, Kontextpanel und Leiste).

## 3. Typografie (Slate-Größen, `DCItemWidgets::Font`)

Bezug 1920×1080 (Slate: ca. 1,33 px je Größenpunkt, bei kleinerer Auflösung skaliert die Engine herunter).
Sätze nie unter 12 – das entspricht etwa dem PC-Mindestmaß aus XAG 101 (ca. 18 px Zeichenhöhe bei 1080p).

| Rolle | Größe | Schnitt | Wofür |
|---|---|---|---|
| Mikro | 10 | Regular | nur Zeitlineal, Einheiten, Nummern in Plaketten – **nie Sätze** |
| Text | 12 | Regular | Details, Tooltips, Hinweise, Listen-Nebenzeilen |
| Label | 12 | Bold | Knöpfe, Schrittnamen, Spaltenköpfe |
| Name | 14 | Bold | Kartennamen, Abschnittstitel, Name in der Zeitleiste |
| Titel | 16 | Bold | Fenster- und Dialogtitel (Dialogtitel als Frage) |
| Banner | 18 | Bold | Alarm, Moduswechsel |

Höchstens drei Größen pro Fenster. Weniger Text ist die Antwort auf Platzmangel, nicht kleinere Schrift.

## 4. Farbrollen

Jede Farbe hat **eine** Bedeutung. Bestehende Konstanten in `DCItemWidgets.cpp`/`DCTimelinePanel.cpp`:

| Rolle | Konstante (heute) | Verwendung – und nur diese |
|---|---|---|
| Fenstergrund | `PanelColor` (0.03, 0.035, 0.05, 0.92) | Hintergrund aller Fenster, Leiste, Karten |
| Fläche | `ZoneColor` (0.07, 0.075, 0.095) | Listen, Spuren, Ablagen im Fenster |
| Kachel / Knopf | `TileColor` (0.14, 0.15, 0.19) | Zeilen, normale Knöpfe |
| Text | `TextColor` (0.93, 0.94, 0.96) | Fließtext und **Titel** |
| Nebentext | `MutedColor` (0.6, 0.63, 0.68) | Einheiten, Zeiten, Erklärungen |
| **Fokus / Hauptaktion** | `TitleColor` (1, 0.85, 0.25) | Auswahl (Ring, Kartenrahmen, gewählte Zeile), Hauptknopf. Titel künftig weiß statt gelb |
| Beute / Wert | `LootColor` (1, 0.78, 0.3) | nur Beute-Namen und €-Werte, nie Rahmen oder Knöpfe |
| Problem | `MessageColor`, `LockedColor`, `StepBlockedColor` | „geht nicht“, versperrt, Ablehnung, Alarm – immer mit Wort/Symbol |
| Passt / erledigt | `AcceptColor`, `DoneColor` | Ablage erlaubt, Schritt erledigt |
| Wartet | `WaitBlockColor` | Wartezeit in der Zeitleiste |
| **Abstimmung** | `SignalColor` – **neu: Cyan** statt Gelb, z. B. (0.35, 0.85, 1) | Warten auf …, Signal, Abhängigkeitspfeile |
| Lärmskala | `UDCNoiseSubsystem::ColorFor` | nur Lärmring und Lärmanzeige |
| Crew-Farben | `Crew->Color` | nur Identität (Kartenstreifen, Spur, Marker). Keine reservierten Töne (Gelb, Rot, Grün, Cyan, Grau) |

„Laut“ in der Zeitleiste nicht umfärben (Orange ≈ Vic), sondern Crew-Farbe plus roter Streifen/Symbol.

## 5. Bausteine

**Knöpfe**
- Drei Stufen: *Haupt* (gefüllt `TitleColor`, dunkler Text, eine pro Bereich), *Normal* (`TileColor`),
  *Gefährlich* (Text in Problem-Rot, löst Rückfrage oder Rückgängig-Toast aus).
- Klickfläche mindestens 28 hoch, 32 breit, Abstand ≥ 4. Immer `.IsFocusable(false)`.
- Beschriftung: Verb, 1–2 Wörter. Tastenkürzel im Tooltip und als kleine Kappe („Leertaste“).
- Nicht möglich → ausgegraut, Tooltip und Klick sagen warum. Feste Plätze, auch wenn ausgegraut.
- Zeilen-Knöpfe (↑ ↓ ×, ablegen, nehmen) nur in der gewählten oder überfahrenen Zeile zeigen.

**Dialoge** – nur für mehrteilige Entscheidungen oder Zerstörendes; keine Dialoge im Dialog.
- Titel als Frage („Worauf soll Vic warten?“), Optionen als Auswahlliste mit sinnvoller Vorauswahl.
- **Ein** Vorschau-Satz in Klartext („Vic wartet, bis Rosa ‚Wandtresor knacken‘ erledigt hat – ca. 19 s.“).
- Unten rechts „Abbrechen“ (normal), daneben „Übernehmen“ (Haupt). Enter = Übernehmen, Esc = Abbrechen.

**Listen** (Schrittliste, Inventar, Behälter)
- Eine Zeile pro Eintrag: Nummer · Zeit · Verb + Objekt · Zustandssymbole. Details nur in der
  gewählten Zeile oder im Tooltip. Feste Spalten, Zeiten gleich breit.
- Lange Namen mit „…“ kürzen und im Tooltip voll zeigen – nie hart abschneiden.
- Leerer Zustand: ein Satz mit der nächsten Handlung („Boden, Tür oder Gegenstand anklicken.“).

**Profilkarten** – pro Figur eine, feste Reihenfolge, Taste 1–n auf der Karte.
- Höchstens drei Zeilen: Farbstreifen + Name + Rolle · „jetzt/als Nächstes …“ · Symbole (Hände, Last, Problem „!“).
- Gewählt: dicker Rahmen in Fokus-Gelb und hellerer Grund. Klick = wählen, Doppelklick = Kamera hin,
  Knopf „Profil“ = Profil & Inventar aufklappen.

**Tooltips**: nach ca. 0,4 s; höchstens vier Zeilen: was es tut · Kürzel · warum nicht · Zahlen.
Nichts, was man zum Spielen *braucht*, steht nur im Tooltip.

**Weltbeschriftung** (`UTextRenderComponent`, nur Latin-1 über `DCVisuals::PlainText`)
- Über Figuren nur Name und Zustand in höchstens drei Wörtern („knackt Tür“, „wartet“).
- Hinweise und Fehler nie als Weltschrift, sondern als Toast.
- Plan-Marker zeigen nur die Nummer; Text erst beim Überfahren oder für die gewählte Figur.
- Heller Text braucht dunklen Rand oder Unterlage, sonst verschwindet er auf Boden und Wand.

## 6. Wortwahl

- Spieltexte ohne Anrede: „Figur anklicken“, nicht „Klicke …“ oder „Sie …“.
- Knöpfe = Verben („Abspielen“, „Übernehmen“, „Knacken“); Bereiche = Substantive („Plan“, „Profil“).
- Ein Begriff pro Sache: *Schritt* (nicht Aufgabe/Aktion), *Signal*, *Figur*, *Plan*, *Zeitleiste*.
- Hinweise höchstens ein Satz. Zeitpunkte „0:21“, Dauern „4 s“ in ganzen Sekunden.
- Deutsche Anführungszeichen „…“, Gedankenstrich –, Ausrufezeichen nur beim Alarm.
- Fehler sagen, was fehlt und was hilft: „Geht nicht: Dietrich-Set fehlt – erst aufheben.“
- Keine Namen oder Texte aus „Der Clou!“ oder anderen Spielen.

## 7. Rückmeldungen

| Ereignis | Rückmeldung |
|---|---|
| Schritt angehängt | neue Zeile kurz hervorgehoben + Marker in der Welt + Toast „Schritt 3: Schublade knacken“ |
| Aktion unmöglich | Toast in Rot mit Grund und Abhilfe; Knopf bleibt ausgegraut |
| Abstimmung angelegt | Pfeil in Cyan in der Zeitleiste von der auslösenden zur wartenden Spur |
| Zerstörendes (Plan leeren) | Rückfrage-Dialog oder Toast „Plan geleert – Rückgängig“ |
| Moduswechsel | Banner kurz oben, Leiste zeigt den Modus dauerhaft; „Ausprobieren“ mit orangem Rahmen |
| Lärm / Alarm | Statuszone oben Mitte; Alarm übertönt alles andere |
| Ziehen (Drag & Drop) | Ziel grün/rot, Grund direkt am Ziel bzw. am Cursor |

## 8. Checkliste für neue UI-Elemente

- [ ] Welche Spieleraufgabe bedient es – und hat die Aktion keinen zweiten Einstieg?
- [ ] Richtige Zone, verdeckt nichts aus einer anderen? Genau eine hervorgehobene Hauptaktion?
- [ ] Schrift ≥ 12 für Sätze, höchstens drei Größen, Farben nur aus Abschnitt 4, nie nur Farbe?
- [ ] Knöpfe ≥ 28 hoch, `.IsFocusable(false)`, Kürzel im Tooltip, „warum nicht“ bei ausgegraut?
- [ ] Springt nichts, wenn Modus oder Inhalt wechseln? Lange Texte gekürzt statt abgeschnitten?
- [ ] Rückmeldung sofort sichtbar? Zerstörendes mit Rückfrage oder Rückgängig?
- [ ] Texte kurz, ohne Anrede, Begriffe aus Abschnitt 6?
- [ ] Inhalt per Signatur/`bDirty` im nächsten Tick neu gebaut, nicht im Klick-Handler?
- [ ] Bei 1600×900 und 1920×1080 im Selbsttest-Screenshot geprüft?

## 9. Grundlagen

Xbox Accessibility Guidelines (XAG 101/102: Textgröße, Kontrast) · gameaccessibilityguidelines.com ·
Celia Hodent, „The Gamer's Brain“ (Usability: Rückmeldung, Klarheit, Konsistenz, geringe Last,
Fehlervermeidung) · Nielsens Heuristiken · gameuidatabase.com als Bildreferenz · Genre-Muster
(Porträtleisten, „Go-Codes“ in Door Kickers) – nur Muster, nichts übernehmen.
