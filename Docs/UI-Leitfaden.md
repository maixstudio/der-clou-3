# UI-Leitfaden – der clou 3

Verbindliche Regeln für das Interface (Slate in C++), Stand 2026-11-08. Farben, Schriften und Bausteine
nur zentral in `DCItemWidgets.h/.cpp` anlegen.

**Look seit 2026-11-08 („noir dossier meets tactical HUD“):** Vorlage ist das Mockup
[`Docs/UI-Mockup/planung_v1.webp`](UI-Mockup/planung_v1.webp) – dunkle blaugraue Fenster, dünne Messinglinien,
abgerundete Ecken (3), Bernstein nur für Auswahl und Hauptknopf, Titel in schmalen Großbuchstaben.

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

**Umgesetzt (Stand 2026-11-07): fester Rahmen.** Die 3D-Szene wird nur innerhalb des Rahmens gezeichnet
(`UDCGameViewportClient` + `UDCFrame`, Größen als Konstanten in `DCFrame.h`): oben Status (44), links
Profilkarten (292), rechts Profil & Inventar (452, immer sichtbar), unten Planungsleiste (330, volle Breite).
Nur Dialoge, Meldungen, Radialmenü, Info und Durchsuchen liegen auf der Szene.

```
+------------------------------------------------------------------------------+
| [1 Vic   ]                 [ Status: Alarm · Lärm · Misstrauen ]  [ Kontext ] |
| [2 Rosa  ] [Profil /                                                [Info |  ] |
| [3 …     ]  Inventar]                                          [Durchsuchen] |
|                             freie Spielfläche                                |
|                      [ Dialog – mittig, nur bei Bedarf ]                     |
|                      [ Meldung (Toast), 3 s ]                                |
| [Letzten Schritt löschen]  [▶ Abspielen] [Tempo] [Warten …] [⋯]   Plan 0:25  |
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

## 3. Typografie (Slate-Größen, `DCItemWidgets::Font` / `HeadFont`)

Schriften: **Barlow** (Text, `Font`) und **Barlow Condensed Bold** (Titel, `HeadFont`, Laufweite +30), beide unter
SIL Open Font License in `Content/Slate/Fonts` (mit `OFL.txt`). Sie werden zur Laufzeit als Dateien geladen, nicht als
Assets: der Font-Import braucht eine Slate-App und stürzt im Commandlet ab. `Config/DefaultEditorPerProjectUserSettings.ini`
hält den Auto-Import von `Content/Slate` fern, `DefaultGame.ini` packt den Ordner mit. Fehlen die Dateien, nimmt
`Font` die Engine-Schrift. Pfeile (↑ ↓ →) hat Barlow nicht – sie kommen aus der Engine-Ersatzschrift.

Bezug 1920×1080 (Slate: ca. 1,33 px je Größenpunkt, bei kleinerer Auflösung skaliert die Engine herunter).
Sätze nie unter 12 – das entspricht etwa dem PC-Mindestmaß aus XAG 101 (ca. 18 px Zeichenhöhe bei 1080p).

| Rolle | Größe | Schrift | Wofür |
|---|---|---|---|
| Mikro | 10 | Barlow Regular | nur Zeitlineal, Einheiten, Nummern in Plaketten – **nie Sätze** |
| Text | 12–13 | Barlow Regular | Details, Tooltips, Hinweise, Listen-Nebenzeilen, Zusammenfassungen |
| Label | 12 | Barlow Bold | Knöpfe, Schrittnamen, Spaltenköpfe |
| Abschnitt | 14 | Condensed, Großbuchstaben | `MakeSectionTitle`: „FÄHIGKEITEN ———“ mit Messinglinie |
| Name | 16–20 | Condensed, Großbuchstaben | Kartennamen, Optionen im Radialmenü |
| Titel | 20–22 | Condensed, Großbuchstaben | Fenstertitel (`MakeTitle`), Modus oben links. Dialogtitel als Frage: Barlow Bold 18, normal geschrieben |
| Banner | 22 | Condensed, Großbuchstaben | Alarm |

Höchstens drei Größen pro Fenster. Weniger Text ist die Antwort auf Platzmangel, nicht kleinere Schrift.

## 4. Farbrollen

Jede Farbe hat **eine** Bedeutung. Konstanten in `DCItemWidgets.h/.cpp` – Werte sind *linear* (Slate rechnet
gammakorrekt), der Kommentar daneben nennt den sRGB-Hex-Wert aus dem Mockup. Neue Farben immer so anlegen
(Hex aus dem Mockup → linear umrechnen), nie sRGB-Werte direkt in `FLinearColor` schreiben.

| Rolle | Konstante | Hex | Verwendung – und nur diese |
|---|---|---|---|
| Rahmen um die Szene | `BackdropColor` | #12171D | feste Ränder hinter den Fenstern |
| Fenstergrund | `PanelColor` | #181F27 | Hintergrund aller Fenster, Leiste, Karten |
| Fläche | `ZoneColor` | #1F262E | Listen, Spuren, Ablagen, Plätze im Fenster |
| Kachel / Knopf | `TileColor` | #262D36 | Zeilen, normale Knöpfe |
| Messing | `BrassColor` / `BrassDimColor` | #7D6744 / #3A362E | Haarlinien: Fensterrahmen, Trenner, Linie hinter Abschnittstiteln |
| Text | `TextColor` | #ECE9E3 | Fließtext und **Titel** (warmes Weiß) |
| Nebentext | `MutedColor` | #8D939B | Einheiten, Zeiten, Erklärungen |
| **Fokus / Hauptaktion** | `TitleColor` | #F2A33A Bernstein | Auswahl (Kartenrahmen, gewählte Zeile, Segment), Hauptknopf, Modus „Planung“. Nie für Titel |
| Beute / Wert | `LootColor` | #EDC35B Gold | nur Beute-Namen und €-Werte, nie Rahmen oder Knöpfe |
| Problem | `MessageColor`, `LockedColor`, `StepBlockedColor` | #EE6A5D | „geht nicht“, versperrt, Ablehnung, Alarm – immer mit Wort/Symbol |
| Passt / erledigt | `OkColor`, `AcceptColor`, `DoneColor` | #7BC47F | Ablage erlaubt, Schritt erledigt, „Plan geht auf“ |
| Wartet | `WaitGroundColor` + `WaitStripeColor` (DCTimelinePanel.cpp) | #424449, Streifen #B8BCC3 | Wartezeit in der Zeitleiste, grau schraffiert |
| **Abstimmung** | `CoordinationColor` | #45D6DE Cyan | Warten auf …, Signal, Abhängigkeitspfeile, Geplantes |
| Lärmskala | `UDCNoiseSubsystem::ColorFor` | nur Lärmring und Lärmanzeige |
| Crew-Farben | `Crew->Color` | nur Identität (Kartenstreifen, Spur, Marker). Keine reservierten Töne (Gelb, Rot, Grün, Cyan, Grau) |

„Laut“ in der Zeitleiste nicht umfärben (Orange ≈ Vic), sondern Crew-Farbe plus roter Streifen/Symbol.

## 5. Bausteine

**Flächen** (alle in `DCItemWidgets`, abgerundet mit Radius 3; die Füllung ist weiß und wird mit
`BorderBackgroundColor` eingefärbt, die Randlinie behält ihre Farbe)
- `FramedBrush()` – Fenster, Karten, Plätze: dünne Messinglinie. Mit `PanelColor` = Fenster.
- `SoftFramedBrush()` – Zeilen und Kacheln im Fenster: schwache Linie.
- `FocusFramedBrush()` – das Gewählte: 2 px Bernstein (Karte, Zeile, Option).
- `CoordinationFramedBrush()`, `ProblemFramedBrush()` – Cyan bzw. Rot, 1 px.
- `RoundBrush()` – nur Füllung (Farbstreifen, Abzeichen, Plätze). `Brush()` bleibt das eckige Weiß.
- `MakeTitle`, `MakeSectionTitle`, `MakeRule` – Titel in Großbuchstaben, Abschnitt mit Messinglinie, Trennlinie.
- Großbuchstaben immer mit `Upper(Text)` (auch als Attribut), **nie** mit `.TransformPolicy(ETextTransformPolicy::ToUpper)`:
  „ß“ wird zu „SS“, dann stimmt die Textlänge in Slate nicht mehr (Ensure in `TextLayout.cpp`, Absturzbericht, Ruckler).
- `MakeSegments` – Umschalter aus Segmenten („Jetzt | Nach dem Plan“), das aktive in Bernstein.
- `MakePainter(Funktion)` – ein Widget, das nur zeichnet (Linien, Pfeile; `FSlateDrawElement`), ohne Klicks.
  `PaintHatch` / `MakeHatch(Grund, Streifen)` – schräge Streifen „/“, an der Bildschirmposition ausgerichtet, damit
  nebeneinanderliegende Flächen das Muster fortsetzen. Bedeutung: *hier steht jemand / passiert nichts*.

**Knöpfe**
- Drei Stufen: *Haupt* (`PrimaryButtonStyle`, gefüllt `TitleColor`, dunkler Text, eine pro Bereich), *Normal*
  (`NormalButtonStyle`, `TileColor` mit schwacher Linie, Messing beim Überfahren), *Gefährlich*
  (`DangerButtonStyle`, rote Linie, Text in Problem-Rot, löst Rückfrage oder Rückgängig-Toast aus).
  Jeder `SButton` bekommt einen dieser Stile (`.ButtonStyle(&…)`) – der Engine-Standardknopf passt nicht.
  `FlatButtonStyle` nur für Knöpfe auf einer eigenen Fläche (Segmente, Zeilen).
- Klickfläche mindestens 28 hoch, 32 breit, Abstand ≥ 4. Immer `.IsFocusable(false)`.
- Beschriftung: Verb, 1–2 Wörter. Tastenkürzel im Tooltip und als kleine Kappe („Leertaste“).
- Nicht möglich → ausgegraut, Tooltip und Klick sagen warum. Feste Plätze, auch wenn ausgegraut.
- Zeilen-Knöpfe (↑ ↓ ×, ablegen, nehmen) nur in der gewählten oder überfahrenen Zeile zeigen.

**Icons** – Linien-Icons von [Lucide](https://lucide.dev) (ISC-Lizenz, `LICENSE-Lucide.txt`), Version 1.70.0,
als SVG in `Content/Slate/Icons/<name>.svg`. Die Strichfarbe ist dort von `currentColor` auf Weiß gesetzt
(nanosvg kennt `currentColor` nicht), eingefärbt wird im Code. Slate rasterisiert jedes SVG in der Größe, in der
es gezeichnet wird, deshalb sind die Icons auch skaliert scharf. Wie die Schriften werden sie als Dateien geladen,
nicht als Assets.
- `Icon(Name)` gibt den Pinsel zurück, `MakeIcon(Name, Größe, Farbe)` das Bild. `MakeIconText(Name, Text, Schrift,
  Farbe)` setzt Icon und Text in eine Zeile, `IconSizeFor(Schriftgröße)` passt die Größe an (12 → 14, 14 → 17).
  In Knöpfen ist die Farbe `FSlateColor::UseForeground()`, dann dimmt das Icon mit, wenn der Knopf ausgegraut ist.
- Feste Zuordnung, ein Icon = eine Bedeutung: `IconForTask` (Schritt-Art) und `IconForSkill` (Fähigkeit).

| Bedeutung | Icon | | Bedeutung | Icon |
|---|---|---|---|---|
| Gehen | `footprints` | | Schlösser (Fähigkeit) | `key-round` |
| Tür öffnen / durchgehen / schließen | `door-open` / `door-closed` | | Tresore | `vault` |
| Schloss knacken | `lock-pick` (eigenes) | | Elektronik | `cpu` |
| Aufbrechen | `crowbar` (eigenes) | | Tresor knacken (Schild) | `vault` |
| Glas einschlagen | `glass-break` (eigenes) | | Tür aufgesperrt (Schild) | `lock-open` |
| Aufheben, Nehmen | `hand-grab` | | Sprengstoff | `bomb` |
| Durchsuchen, Fund | `search` | | Fahren | `car` |
| Ablegen | `arrow-down-to-line` | | Kampf | `swords` |
| Warten, Warten auf … | `hourglass` | | Aufpassen | `eye` |
| Zeichen geben | `radio` | | Kraft | `biceps-flexed` |
| Info | `info` | | Abspielen / Pause | `play` / `pause` |
| Geht nicht, Plan hängt | `triangle-exclamation-point` | | Plan geht auf, fertig | `circle-check` |
| Neuer Schritt (Meldung), Lösen | `crosshair` | | Rückgängig, Zurück zur Planung | `undo-2` |
| Schritt löschen | `trash` | | Schließen, Später | `x` |
| Nachbar / Misstrauen | `ear` | | Lärm | `volume-2` |
| Alarm | `siren` | | Ebene, höher / tiefer | `layers`, `chevron-up` / `chevron-down` |
| Menü | `menu` | | | |

Ein neues Icon: die SVG-Datei aus derselben Lucide-Version holen, `currentColor` durch `#ffffff` ersetzen, in die Tabelle
eintragen. Fehlt eins bei Lucide, selbst zeichnen im selben Stil (24 × 24, Strich 2, runde Enden, keine Füllung), wie
`lock-pick`, `crowbar`, `glass-break` – und bei 16 px prüfen, ob es noch lesbar ist. Icons stehen vor dem Text, nie allein (Ausnahme: Pfeile, Schließen – mit Tooltip). Farbe wie der Text daneben,
Problem rot, Abstimmung cyan; kein Bernstein, außer es ist die Auswahl.

**Dialoge** – nur für mehrteilige Entscheidungen oder Zerstörendes; keine Dialoge im Dialog.
- Titel als Frage („Worauf soll Vic warten?“), Optionen als Auswahlliste mit sinnvoller Vorauswahl.
- **Ein** Vorschau-Satz in Klartext („Vic wartet, bis Rosa ‚Wandtresor knacken‘ erledigt hat – ca. 19 s.“).
- Unten rechts „Abbrechen“ (normal), daneben „Übernehmen“ (Haupt). Enter = Übernehmen, Esc = Abbrechen.

**Listen** (Schrittliste, Inventar, Behälter)
- Eine Zeile pro Eintrag: Nummer · Zeit · Verb + Objekt · Zustandssymbole. Details nur in der
  gewählten Zeile oder im Tooltip. Feste Spalten, Zeiten gleich breit.
- Lange Namen mit „…“ kürzen und im Tooltip voll zeigen – nie hart abschneiden.
- Leerer Zustand: ein Satz mit der nächsten Handlung („Boden, Tür oder Gegenstand anklicken.“).

**Zeitleiste** (`UDCTimelinePanel::RebuildTracks`) – wie im Mockup, eine Spur je Figur.
- Lineal wie ein Maßband: Haarlinie unten, Strich je Sekunde (wenn Platz ist), längerer Strich und Zeit darüber alle 5 s
  (bei langen Plänen 10, 15, 30 s – Zeiten mindestens 46 Einheiten auseinander).
- Links ein Farbbalken in der Crew-Farbe und der Name (Vic, Rosa); die gewählte Figur hell mit schwacher Linie um ihre Spur.
- Spur: schwach schraffiert, wo nichts geplant ist. Gehen = Crew-Farbe gedämpft, Arbeiten = Crew-Farbe kräftig mit Nummer
  (fett) und – ab 80 Einheiten – Name des Schritts, Warten = graue Schraffur mit „wartet“ auf einer Lücke in den
  Streifen, geht nicht = rot mit „geht nicht“, Zeichen = Cyan.
- Wer auf wen wartet: Pfeil in Cyan vom Rand des Schritts, der den Weg frei macht (Punkt in dessen Crew-Farbe), zum
  Anfang des wartenden Schritts.
- Zeitmarke: Linie durch alle Spuren und darüber ein Reiter mit der Zeit („0:21“) – Bernstein für die Vorschau in der
  Planung, Weiß beim Abspielen.

**Plan im Raum** (`ADCPlanTrail`, `UDCWorldBadges`) – statt Linien Fußspuren und Schilder.
- Fußspuren in der Crew-Farbe, links/rechts im Wechsel, in Gehrichtung (unbeleuchtet, `M_PlanMark`). Die gewählte Figur
  kräftig, die anderen blass; nach dem gezeigten Moment (Vorschau, gewählter Schritt) heller.
- Wo jemand etwas tut: Ring am Boden und darüber ein Schild am Stiel. Gewählte Figur: Pille in ihrer Farbe mit Nummer im
  hellen Kreis, Icon und Verb („1 knacken“). Andere Figuren: dunkle Pille mit Rand in ihrer Farbe, nur Nummer und Icon.
  Mehrere Schritte am selben Ort teilen sich ein Schild; geht einer nicht, ist das Schild rot mit Warnzeichen.
- Schilder sind 2D, immer gleich groß, liegen unter Rahmen und Fenstern und weichen sich nach oben aus. Maus darüber:
  Kasten mit Zeit, Dauer, Chance, Wartezeit. Klick: wählt den Schritt wie in der Liste.
- Türen, die der Plan ändert, und die Zeitvorschau („Rosa: wartet auf Vic“) nutzen dasselbe Schild in dunkel mit Rand.
- Neue optische Actors: nie Teil des NavMesh (`IsComponentRelevantForNavigation` = false), keine Kollision, kein Schatten.

**Kamera** – folgt der gewählten Figur (Planung: ihrem Plan-Stand, mit ruhiger Mitte; Abspielen: der Figur). Selbst
schieben löst das; oben rechts erscheint „Kamera frei · F folgt“.

**Profilkarten** – pro Figur eine, feste Reihenfolge, Taste 1–n auf der Karte.
- Farbstreifen links (Crew-Farbe = Identität), Porträt 66 × 88 im Messingrahmen (ohne Bild: Initiale in der
  Crew-Farbe auf dunklem Grund), Tastennummer als Abzeichen vor dem Namen, Name in Großbuchstaben, darunter der
  Plan-Stand (hängt der Plan: rot mit Warnzeichen) und die zwei besten Fähigkeiten als Zeilen mit Icon, Name und Wert.
- Gewählt: 2 px Bernstein-Rahmen und hellerer Grund. Klick = wählen + Kamera hin.
- Das Profil rechts zeigt dasselbe Porträt (60 × 80) links neben Titel, Umschalter und Zusammenfassung.

**Porträts** – gemalt, Kopf und Schultern, dunkler Hintergrund, warmes Licht von links, kühles von rechts; heute
spielend, zeitlose dunkle Kleidung. Prompts und Ablauf: `Docs/UI-Mockup/Portraet-Prompts.md`.
- Originale in `Art/Portraits/<name>.png|jpg|webp`, `python Scripts/Tools/prepare_portraits.py` schneidet auf 3:4,
  geht auf 75 % ans Gesicht heran (auf der Karte ist es sonst zu klein) und schreibt 132 × 176 nach
  `Content/Slate/Portraits/<name>.png` (Name der Figur in Kleinbuchstaben).
- Das Spiel lädt sie zur Laufzeit (`DCItemWidgets::Portrait`, `MakePortrait`, dynamischer Slate-Pinsel; andere
  Seitenverhältnisse schneidet es selbst zu: Mitte bei breiten, oberer Teil bei hohen Bildern).

**Gegenstands-Icons** – aus den Blender-Modellen gerendert, alle gleich: orthografisch schräg von oben, transparent,
warmes Licht von links, kühles Gegenlicht von rechts. Jedes Bild hat das Seitenverhältnis seines Rasters (64 px je Feld;
Brecheisen 1 × 4 hochkant, Laptop 3 × 2 quer), Dinge ohne Raster (Möbel, Gemälde) sind quadratisch.
- Raster: das Bild füllt die Kachel auf neutralem Grund; ab 2 × 2 Feldern steht der Kurzname klein unten links, kleinere
  zeigen nur das Bild (Name im Tooltip).
- Kopfzeilen (getragene Kleidung, Weste, Rucksack, Möbel und Behälter beim Durchsuchen, „Ausgewählt“): Bild 22 × 20 vor
  dem Namen. Hände: Bild 34 × 30 vor dem Namen. Beim Ziehen: Bild 44 × 44 neben Name und Hinweis.
- Fehlt ein Bild (z. B. Schublade), bleibt die alte Kachel mit Kurzname und Katalogfarbe.
- Ablauf: `blender -b --factory-startup -P Art/Items/build_items.py -- --icons --items <ids>` (Rohbilder nach
  `Saved/ItemIcons/raw`), dann `python Scripts/Tools/prepare_item_icons.py` (liest die Rastergrößen aus
  `DCItemTypes.cpp`, schneidet zu, dreht nur deutlich längliche Dinge, schreibt `Content/Slate/ItemIcons/<id>.png`).
  `--ids` gibt die Katalog-Ids für `--items` aus. Andere Blickrichtung je Gegenstand: `ICON_VIEWS` in `build_items.py`.
- Code: `DCItemWidgets::ItemIcon(DefId)` (dynamischer Slate-Pinsel, Pixelgröße = Wunschgröße), `MakeItemIcon(DefId,
  MaxBreite, MaxHöhe)` (Seitenverhältnis bleibt).

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
| Moduswechsel | Banner kurz oben, Leiste zeigt den Modus dauerhaft (Planung / Plan läuft); „Ausprobieren“ ist seit 2026-11-08 entfernt |
| Lärm / Alarm | Statuszone oben Mitte; Alarm übertönt alles andere |
| Ziehen (Drag & Drop) | Ziel grün/rot, Grund direkt am Ziel bzw. am Cursor |

## 8. Checkliste für neue UI-Elemente

- [ ] Welche Spieleraufgabe bedient es – und hat die Aktion keinen zweiten Einstieg?
- [ ] Richtige Zone, verdeckt nichts aus einer anderen? Genau eine hervorgehobene Hauptaktion?
- [ ] Schrift ≥ 12 für Sätze, höchstens drei Größen, Farben nur aus Abschnitt 4, nie nur Farbe?
- [ ] Icons nur aus der Tabelle in Abschnitt 5 (gleiche Bedeutung = gleiches Icon), immer mit Text oder Tooltip?
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
