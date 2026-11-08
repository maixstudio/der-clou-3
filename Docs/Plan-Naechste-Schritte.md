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
- **Gebaut 2026-11-07 (v1, nur Ansicht):** Profil zeigt in der Planung den Stand nach dem Plan, Umschalter
  „Jetzt | Nach dem Plan“, Geplantes blau markiert. Offen: Schrittnummer an geplanten Dingen, Umpacken der
  echten Ausrüstung direkt in dieser Ansicht.
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

### 5. Nebel im Haus – **entschieden 2026-11-08** (User), Bau in Teilschritten
**Entscheidungen des Users:**

| Thema | Entscheidung |
|---|---|
| Bauplan (Wissen vorher) | Räume mit Namen, Wände und Türen sind bekannt. Ob eine Tür versperrt ist und was in den Räumen steht, sieht man erst vor Ort. |
| Planen im Unbekannten | Plan-Schritt „Raum durchsuchen“ (Zeit nur geschätzt); vor Ort hält das Spiel bei Funden an und fragt bei Versperrtem (knacken / aufbrechen / auslassen). **Dazu live umplanen:** in der Pause neue Schritte anhängen, sobald etwas entdeckt wurde. |
| Sicht | **Sichtlinie:** gesehen ist nur, was eine Figur im Blick hat, auch durch offene Türen; hinter Möbeln bleibt es dunkel. |
| Testhaus | Garten (Start) → Hintertür (versperrt) → Flur (Kommode, Gemälde) → Büro (versperrt: Schreibtisch, Wandtresor) und Schlafzimmer (Kleiderschrank, Schmuckschatulle). Nachbar hinter der Wand bleibt. |
| Wiederholbar / einmalig | weiterhin als Umschalter (Wissen bleibt bei „wiederholbar“ nach „Zurück zur Planung“) |

**Bau-Reihenfolge (jeder Teil im Spiel sichtbar):**
- **N1 Haus-Level – gebaut 2026-11-08** (CLAUDE.md, Schritt 16). `L_House01` (per Python-Skript gebaut): Garten, Flur, Büro, Schlafzimmer, Türen, Möbel
  mit Inhalt, Nachbar, Licht; Räume als `ADCRoomVolume` (Name, Grenzen). Crew startet im Garten.
  `L_Room01` bleibt für die Selbsttests.
- **N2 Wissen + Nebel in der Planung:** Wissen je Objekt und je Bodenfeld (`UDCKnowledgeSubsystem` in der
  GameInstance). Unbekannte Bodenfelder dunkel (Nebelfelder ca. 50 cm), Raumnamen sichtbar, unbekannte
  Möbel/Gegenstände unsichtbar und nicht anklickbar, Türen mit „Schloss ?“ (Knacken: Spanne, „zeigt sich vor
  Ort“).
- **N3 Sichtlinie im Einbruch:** jede Figur deckt regelmäßig auf, was sie sieht (Linien von Augenhöhe zu
  Bodenfeldern und Objekt-Messpunkten, Wände/Möbel/geschlossene Türen verdecken). Entdecktes blendet ein,
  Meldung „Vic entdeckt: Wandtresor“.
- **N4 „Raum durchsuchen“:** Plan-Schritt für einen Raum; live arbeitet die Figur die entdeckten Möbel ab
  (nächstes zuerst), Funde → Fund-Pause, Versperrtes → Frage-Pause (knacken / aufbrechen / auslassen).
  Zeitleiste: unsichere Dauer schraffiert (Spanne).
- **N5 Live umplanen:** in jeder Pause (Leertaste, Fund, Frage) kann man mit den Planungs-Menüs Schritte an
  den Rest eines Plans anhängen; der Runner übernimmt sie.
- **N6 Umschalter „wiederholbar / einmalig“** und Wissen über „Zurück zur Planung“ hinweg.

### 6. Absichten bei vermuteten Dingen (Claudes Vorschlag, später – für Hinweise wie „im Haus ist ein Tresor“)
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

### Villa für die Demo – Grundriss-Entwurf 2026-11-08
Auftrag des Users: größere Villa mit Keller, EG, 1. Stock, Nebengebäude, Zaun, Bäumen, Hecke, Außenlicht, Fenstern,
verschiedenen Türen, Pool und Garten – zuerst nur der Grundriss. Entwurf: `Docs/Villa/Villa-Grundriss.html`
(Daten `Scripts/Levels/villa_layout.py`). Noch nicht gebaut; die offenen Fragen stehen am Ende der Seite.

### 7. Später
Auskundschaften als eigene Phase · Polizei und Zeitlimit · Wachen und Hunde · Alarmanlagen (Elektronik) ·
Überwältigen (Kampf; Schlüssel der Wache aufheben und verstauen) · Geld als Menge · zu zweit tragen ·
Übergeben zwischen Figuren · Zustände sichtbar machen (Tresortür offen …, gehört zum Blender-Chat).

## Offen

- Einmalig oder wiederholbar – nach dem Testen entscheiden.
- Nebel: ob Entdeckungen das Spiel automatisch anhalten sollen (bisher geplant: nur Meldung, anhalten mit
  Leertaste; Funde und Versperrtes halten ohnehin an) – nach dem Testen entscheiden.
- Ob die Absichten (Schritt 6) so verständlich sind – der User korrigiert nach.

## Zusammenarbeit

- Ein **anderer Chat** baut in Blender Modelle, Licht und Raum-Look: `Art/`, `DCVisuals::ItemModel/
  SetItemLook`, `/Game/DerClou/Items`. Diese Dateien nicht anfassen. Beim Committen nur eigene Dateien
  hinzufügen, nie `git add -A`.
- Für Interface-Arbeit den Agenten `.claude/agents/game-ui.md` und `Docs/UI-Leitfaden.md` nutzen.
- Bekannte Kleinigkeiten:
  - Am Tresor in der Ecke drückt sich die zweite Figur vorbei: gespielt ca. 4 s länger als geplant.
  - Der Radialmenü-Screenshot im Timeline-Test kommt zu früh.
