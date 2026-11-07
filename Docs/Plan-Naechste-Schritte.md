# Nächste Schritte – Planung, Einbruch, Raster-Inventar

Stand 2026-11-07, mit dem User abgestimmt. Grundlage für die nächsten Sitzungen. Vor dem Bauen jeden
Schritt kurz mit dem User besprechen, wenn eine Spielregel offen ist (siehe „Offen“).

## Entscheidungen des Users

| Thema | Entscheidung |
|---|---|
| Ablauf | **Zwei Phasen:** Phase 1 Planung (grob), Phase 2 Einbruch (Plan läuft, Spieler reagiert) |
| Inventar | **Raster** – aber kein Tetris: nur Rechtecke (keine L-/Turm-Formen), Größen realistisch; Kleinkram (Schlüssel, Ring, ein paar Scheine) braucht 1 Feld |
| Aufheben | **„Wohin damit?“** – das Spiel schlägt einen Platz vor, der Spieler kann selbst wählen |
| Wissen | **Nebel je nach Aufklärung** – was die Crew nicht weiß, sieht man in der Planung nicht |
| Absichten auf Raumebene | **Claude entscheidet** (Vorschlag unten), der User korrigiert nach dem Testen |
| Auskundschaften | **später** – vorerst ist das Wissen im Level fest eingestellt |
| Entscheidungen im Einbruch | **Spiel hält an** (Handgriffe kosten trotzdem Spielzeit) |
| Einmalig oder wiederholbar | **offen** – der User will testen → als Umschalter bauen |
| Störungen | **Figur hält an und fragt** (die anderen laufen weiter) |

## Schritte (in dieser Reihenfolge)

### 1. Raster-Inventar – **gebaut 2026-11-07** (Details in CLAUDE.md, Schritt 12)
- Umgesetzt wie unten, mit drei Abweichungen: Taschen und Weste nehmen weiter nur „kleine“ Dinge
  (Größenklasse als zweite Grenze, sonst passt ein Laptop 3×2 genau in die Hosentasche 3×2);
  Kleidung und Weste als Gegenstand 2×2; Umzugskarton, Gemälde, Bronzefigur passen in kein Raster.
  Knöpfe stehen nicht mehr auf jeder Kachel, sondern unter „Ausgewählt“ (anklicken).
- **Daten:** `FDCItemDef` bekommt `GridSize` (B × H Felder). Behälter bekommen ein Raster (`GridW × GridH`)
  statt Liter. Gewicht und Kraft-Grenzen bleiben. Größenklassen bleiben für die Hände (groß = beide Hände,
  sperrig = allein unmöglich).
- **Vorschlag Größen:** Dietrich, Ring, Uhr, Kette, Brosche, Geldbündel, Brieftasche, Schlüssel 1×1 ·
  Stethoskop 2×1 · Taschenlampe, Porzellanfigur 1×2 · Brecheisen 1×4 · Schmuckschatulle 2×1 (fasst 2×1) ·
  Laptop 3×2 · Aktenordner 2×3 · Mantel 3×4 (Tasche 2×1) · Rucksack als Gegenstand 3×4.
  Raster: Arbeitskleidung 3×2, Werkzeugweste 4×2, Rucksack 5×4, Brieftasche 1×1, Schublade 6×3,
  Tresor 4×4, Vitrine 6×2, Umzugskarton 6×4, Kleiderschrank 8×6, Sofa-Versteck 3×1.
  Gemälde und Bronzefigur passen in kein Raster, nur in beide Hände.
- **Logik:** `UDCItem` merkt sich `GridPos` im Behälter. Neue Funktionen `CanPlaceAt`, `FindFreeSpot`
  (erste freie Stelle) und `PlaceAt`. `CheckCanHold` nutzt das Raster. `AddAnywhere` setzt auf die erste
  freie Stelle in der Reihenfolge Vorschlag → Taschen → Weste → Rucksack → Hand.
  Die Simulation kopiert die Positionen mit (`Clone`, `CopyFrom`, `BuildSimItem`).
- **UI:** neues Slate-Widget `SDCItemGrid` mit Feldern von ca. 26 px. Gegenstände sind Rechtecke mit Namen.
  Beim Ziehen zeigt es den Fußabdruck grün oder rot und sagt den Grund. Hände sind zwei große Plätze,
  Kopf/Kleidung/Weste/Rücken Tragplätze mit ihrem Raster darunter. Gilt für das Profil rechts und für das
  Durchsuchen-Fenster (Möbel-Raster).
- **Später, nicht in v1:** Drehen mit der Taste R; Geld als Menge mit „Wie viel nimmst du mit?“
  (1 Mio. in 5ern ≈ 200 kg).
- **Tests:** Selbsttests Inventory, Containers und PlanInventory auf das Raster umstellen. Neuer Test
  `-DCSelfTest=Grid`: Brecheisen passt nicht in die Taschen, aber in den Rucksack; Fußabdruck-Prüfung.

### 2. „Wohin damit?“ und das Profil in der Planung
> **User 2026-11-07:** In der Planung wird nur festgelegt, *wer was durchsucht* (Kasten, Tresor) –
> kein „Wohin“ pro Gegenstand. Im Einbruch (live) informiert das Spiel, was gefunden wurde (wenn
> überhaupt), und der Spieler entscheidet, wohin es kommt, ob es liegen bleibt oder ob er etwas
> anderes dalässt. → Schritt 3 vorgezogen; der Dialog „Wohin damit?“ beim Planen entfällt für
> Durchsuchen (beim Aufheben einzelner Dinge noch offen).
- Dialog beim Aufheben und Nehmen: Er zeigt das Raster der Figur. Der Vorschlag ist markiert
  (Werkzeug → griffbereit in Hand, Weste oder Taschen; Beute → Rucksack oder Taschen). Der Spieler klickt
  einen anderen Platz an oder bestätigt. „Liegen lassen“ bricht ab.
- **Planung:** `FDCPlanTask` speichert den Platz (Tragplatz oder Behälterpfad + `GridPos`). Simulation und
  Runner legen genau dort ab. Ist der Platz belegt, legen sie an die erste freie Stelle und melden das.
- Das **Profil rechts zeigt in der Planung den Stand nach dem Plan**. Geplante Gegenstände sind markiert
  („geplant, Schritt 1“) und nicht ziehbar. Echte Ausrüstung bleibt umpackbar (Vorbereitung vor dem
  Einbruch). Ein Umschalter im Profil wechselt zwischen „Jetzt“ und „Nach dem Plan“.
- Griffbereit: Hand sofort, Taschen und Weste 1 s, aus dem Rucksack 3 s. Das fließt in die Prüfungen ein.
- **Hinweis beim Wechsel Ausprobieren → Planen:** „Raum zurückgesetzt – beim Ausprobieren Aufgehobenes
  ist wieder weg“. Das war vermutlich der Grund, warum das Brecheisen im Test des Users „nicht ging“.

### 3. Phase 2: Durchsuchen mit Fund-Pause – **gebaut 2026-11-07** (CLAUDE.md, Schritt 13)
- Noch offen daraus: Protokoll im Ergebnis-Bildschirm (bisher Meldung + Log), sichtbare Verschiebung
  der Zeitleiste, wer auf ein Zeichen wartet, wartet bereits automatisch länger (live).
- „Ausräumen“ wird **„Durchsuchen“**. In der Planung ist der Inhalt unbekannt (Zeit als Schätzung).
- Im Einbruch hält das Spiel an, wenn eine Figur durchsucht. Dann öffnen sich das Durchsuchen-Fenster
  mit Raster und daneben das Profil. Der Spieler steckt ein, lässt liegen oder packt um, auch Dinge von
  vorher, die dann im Behälter oder am Boden bleiben.
- Jeder Handgriff kostet Spielzeit (Nehmen 0,6 s, Umpacken 1 s). Nach „Weiter“ steht die Figur so lange
  dort, und die Zeitleiste verschiebt sich sichtbar. Wer auf ein Zeichen wartet, wartet länger.
- Ein Protokoll im Ergebnis hält fest, was gefunden, mitgenommen und zurückgelassen wurde.

### 4. Störungen: anhalten und fragen
- Störungen sind: Schritt nicht geschafft (z. B. drei Fehlversuche), Ziel nicht da, Hände voll, Weg
  versperrt, Warten dauert viel länger als geplant.
- Das Spiel hält an. Die Karte der Figur blinkt. Ein Dialog fragt „Vic: Hintertür geht nicht auf – was
  tun?“ mit: Nochmal versuchen · Schritt überspringen · Selbst steuern (danach auf der Karte „Plan
  fortsetzen“). Die anderen laufen nach dem Dialog weiter.

### 5. Wissensstufen und Nebel
- Jedes Objekt (Tür, Möbel, Gegenstand, Inhalt) hat eine Wissensstufe, im Level eingestellt:
  **unbekannt** (in der Planung unsichtbar) · **vermutet** („im Raum ist ein Tresor“, Symbol ohne genaue
  Lage) · **bekannt** (Position) · **genau** (Schloss, Alarm, Chance).
- Planung: Unbekanntes wird nicht gezeichnet oder ist abgedunkelt. Der Raum ist nur so weit sichtbar, wie
  ein Bauplan reicht. Im Einbruch deckt auf, was eine Figur sieht (Sichtlinie, Raum betreten).
- **Umschalter „wiederholbar / einmalig“** zum Testen. Wiederholbar: Wissen aus einem Durchlauf bleibt.
  Einmalig: kein „Zurück zur Planung“ nach dem Start.

### 6. Absichten bei vermuteten Dingen (Claudes Vorschlag)
- Ein **vermutetes** Objekt erscheint als Symbol an der vermuteten Stelle (z. B. Raummitte: „Tresor?“).
  Das Radialmenü bietet die üblichen Aktionen mit **Spannen** an („knacken: 15–30 s, Chance erst bekannt,
  wenn das Schloss gesehen wurde“).
- Der Plan-Schritt heißt dann z. B. „Tresor finden und knacken“. Intern zeigt er auf das echte Objekt,
  die UI verrät aber nur, was bekannt ist.
- Im Einbruch geht die Figur zur vermuteten Stelle und **sucht** (5–15 s, bis sie das Objekt sieht).
  Dann wird es „bekannt“, und die Figur macht weiter. Ist es nicht da oder findet sie es nicht in der Zeit,
  ist das eine Störung (Schritt 4).
- Die Zeitleiste zeigt unsichere Teile schraffiert (min–max). Zeichen und Warten bleiben genau, weil sie
  auf Ereignisse warten.
- Später **Zeitbudget** je Suche („höchstens 45 s“) und Räume als eigene Bereiche (`ADCRoomVolume`).

### 7. Später
Auskundschaften als eigene Phase · Polizei und Zeitlimit · Wachen und Hunde · Alarmanlagen (Elektronik) ·
Überwältigen (Kampf; Schlüssel der Wache aufheben und verstauen) · Geld als Menge · zu zweit tragen ·
Übergeben zwischen Figuren · Zustände sichtbar machen (Tresortür offen …, gehört zum Blender-Chat).

## Offen

- Einmalig oder wiederholbar – nach dem Testen entscheiden.
- Ob die Absichten (Schritt 6) so verständlich sind – der User korrigiert nach.

## Zusammenarbeit

- Ein **anderer Chat** baut in Blender Modelle, Licht und Raum-Look: `Art/`, `DCVisuals::ItemModel/
  SetItemLook`, `/Game/DerClou/Items`. Diese Dateien nicht anfassen. Beim Committen nur eigene Dateien
  hinzufügen, nie `git add -A`.
- Für Interface-Arbeit den Agenten `.claude/agents/game-ui.md` und `Docs/UI-Leitfaden.md` nutzen.
- Bekannte Kleinigkeiten:
  - Am Tresor in der Ecke drückt sich die zweite Figur vorbei: gespielt ca. 4 s länger als geplant.
  - Der Radialmenü-Screenshot im Timeline-Test kommt zu früh.
