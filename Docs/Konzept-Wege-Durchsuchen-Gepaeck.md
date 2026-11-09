# Konzept: Wege als Fußspuren, Kamera folgt, alles durchsuchbar, Gepäck statt Trageplätze

Stand 2026-11-09. **Vorschlag, vom User noch nicht bestätigt.** Er ersetzt „Professioneller Look,
Schritt 6“ (Punktlinien und Schilder) und, sobald bestätigt, das Raster-Inventar und „Wohin damit?“ aus
`Plan-Naechste-Schritte.md`.

## Was der User will (2026-11-09)

- Statt Linien sollen **Schritte** (Fußspuren) in der Farbe der Figur zu sehen sein. Bei einer Tätigkeit
  stehen **Nummer und Icon** (Schloss knacken = Schloss mit Dietrich, Durchsuchen = Lupe). Am Weg soll man
  erkennen, *was* die Figur *wo* macht.
- Die **Kamera folgt immer der ausgewählten Figur**. Beim Wechseln wechselt auch die Kamera.
- **Jeder Gegenstand ist in der Planung durchsuchbar**, auch einer, der nichts enthält. Ein Klick auf einen
  Gegenstand öffnet ein Menü wie „nach Wertvollem durchsuchen“. Ist er versperrt, kommt „öffnen mit …
  Dietrich, Brecheisen, anderes Werkzeug, das man dabeihat“.
- **Kein Inventar mit Zuordnung mehr** (welche Hand, welche Tasche). Es gibt ein Gesamtinventar.
  **Kleidung und Ausrüstung kommen extra**, weil es dort um etwas anderes geht: was Zeugen sehen und dass
  mitgenommenes Werkzeug Platz und Gewicht braucht.

---

## A. Wege als Fußspuren, Tätigkeiten als Schilder

**Fußspuren**
- Sie liegen entlang des geplanten Wegs, also des NavMesh-Pfads (`Path` + `WorkPath` je Schritt). Die
  Höhe kommt von einem Trace auf den Boden, damit es auch auf Treppen passt.
- Abdrücke liegen etwa alle 38 cm, abwechselnd links und rechts (±7 cm), und zeigen in Gehrichtung. Die
  Richtung ist damit ohne Pfeile klar.
- Form: ein flacher Fußabdruck von etwa 11 × 26 cm (Blender-Skript, Sohle und Zehenballen), unbeleuchtetes
  Material in der Crew-Farbe.
- Technik: ein `UInstancedStaticMeshComponent` je Figur, die Farbe kommt pro Abdruck als Custom Data.
  Hunderte Abdrücke kosten so fast nichts. Die Spuren sind nicht Teil des NavMesh, haben keine Kollision
  und werfen keinen Schatten.
- **Ausgewählte Figur** kräftig. **Andere Figuren** blass und etwas kleiner.
- **Zeitcursor** (Zeitleiste oder gewählter Schritt): Bis zu diesem Moment sind die Spuren voll, danach
  heller. So sieht man, wie weit jede Figur ist.
- **Warten:** Die Spur endet mit zwei Füßen nebeneinander, darüber hängt das Schild mit der Sanduhr
  („wartet 3 s“).
- **Tür unterwegs:** Öffnet die Figur eine Tür auf dem Weg, steht dort ein kleines Tür-Icon ohne Nummer.
  Das ist kein eigener Schritt.
- **Plan hängt:** Die Spur endet dort, wo die Figur hängen bleibt. Dort steht ein rotes Schild mit
  Warnzeichen, dahinter kommt nichts mehr.

**Schilder (Nummer + Tätigkeit)**
- Jedes Schild ist eine Pille in der Crew-Farbe mit der Nummer im Kreis und dem Icon der Tätigkeit. Bei der
  ausgewählten Figur steht das Verb dabei („knacken“, „durchsuchen“), bei den anderen nur Nummer und Icon.
- Ein kurzer Stiel führt zu einem Ring am Boden, wo die Figur steht und arbeitet.
- Liegen mehrere Schritte am selben Ort, werden sie zu einem Schild zusammengefasst, z. B.
  „2 Schloss · 3 Lupe“ am Tresor.
- **Maus darüber** zeigt Zeit, Dauer und Chance („3. Hintertür knacken · 0:12–0:16 · 90 %“).
  **Klick** wählt den Schritt wie in der Liste. **Rot** heißt: geht nicht.
- Technik: Die Schilder sind 2D und werden in *einem* Overlay über der Szene gezeichnet
  (`UDCWorldBadges`). Es projiziert die Weltpunkte jedes Frame und schneidet am Szenen-Rechteck ab. Die
  Schilder sind immer gleich groß, scharf und haben dieselben Icons und Schriften wie die Leiste. Nur die
  Schilder fangen Klicks, alles andere geht an die Szene durch.
- **Türschilder** („aufgesperrt 0:03 · Vic“) und die **Stecknadeln der Zeitvorschau** bekommen denselben
  Stil.
- **Icons** (Lucide, wo es passt; sonst eigene im gleichen Stil, 24 × 24, Strich 2):

  | Tätigkeit | Icon |
  |---|---|
  | Schloss knacken | Vorhängeschloss mit Dietrich (neu) |
  | Tresor knacken | `vault` |
  | Aufbrechen | Brecheisen (neu) |
  | Glas einschlagen | Hammer mit Splittern (neu) |
  | Durchsuchen | `search` (Lupe) |
  | Mitnehmen / Aufheben | `hand-grab` |
  | Ablegen | `arrow-down-to-line` |
  | Tür öffnen / schließen / durchgehen | `door-open` / `door-closed` |
  | Warten | `hourglass` |
  | Zeichen geben | `radio` |

**Fällt weg:** die Linien (Segmente) und die Scheiben mit Stange und Zahl (`ADCPlanMarker`). Der Geist der
gewählten Figur bleibt.

## B. Kamera folgt der ausgewählten Figur

- Die Kamera folgt **immer** sanft der ausgewählten Figur. In der Bildmitte liegt eine **Totzone** (etwa
  40 % der Szene): Solange die Figur darin bleibt, steht das Bild still und wackelt nicht bei jedem kleinen
  Schritt.
- **Planung:** Die Kamera folgt dem Planungsstand der Figur, also dem Geist. Das ist das Ende ihres Plans,
  der gewählte Schritt oder die Stelle am Zeitcursor. Ein neuer Schritt am Rand des Bildes zieht die Kamera
  mit.
- **Abspielen / Einbruch:** Die Kamera folgt der echten Figur.
- **Wechseln** (Karte, Taste 1–9, Klick auf Figur, Geist oder Schild): Die Kamera gleitet zur neuen Figur.
- **Selbst schieben** (WASD oder Ziehen) löst das Folgen. Oben steht dann „Kamera frei · F folgt wieder“.
  F oder ein Klick auf die Karte schaltet das Folgen wieder ein. Drehen und Zoomen lösen es nicht.
- Das Stockwerk folgt der Figur schon heute.

## C. Jeder Gegenstand ist durchsuchbar – Menü „Öffnen mit …“

**Regeln**
- **„Durchsuchen“ gibt es bei jedem Möbelstück und Behälter**, auch ohne Inhalt: Sessel, Stuhl, Esstisch,
  Gemälde (dahinter), Bett (unter der Matratze). Damit verrät das Menü nicht mehr, wo etwas drin ist. Das
  passt zum Nebel: In der Planung ist der Inhalt unbekannt.
- In den Daten bekommt jedes Möbelstück ein Versteck. Meist ist es leer, im Level kann dort etwas liegen
  (Schlüssel unter dem Sessel). Die Dauer hängt von der Größe ab: kleines 1 s, Möbel 2–5 s, mit
  Schubladen mehr.
- Leer heißt beim Einbruch „nichts gefunden“, und es geht ohne Pause weiter.
- Lose kleine Beute (Ring auf dem Tisch) bekommt nur „Mitnehmen“, weil Durchsuchen dort nichts bedeutet.

**Menü (Radialmenü, Planung)**
1. **Nach Wertvollem durchsuchen** (Lupe), z. B. „ca. 3 s · Inhalt unbekannt“.
   - Geschlossen, aber nicht versperrt: Die Figur öffnet selbst (wie bei Türen).
   - Versperrt: Es öffnet sich ein **zweiter Ring** „Schublade öffnen mit …“. Die Wahl legt zwei Schritte an
     (öffnen + durchsuchen), ein Klick und einmal Rückgängig.
2. **Öffnen mit …** (nur wenn versperrt), führt zum selben zweiten Ring, legt aber nur den Öffnen-Schritt
   an. Damit kann z. B. Vic aufsperren und Rosa durchsuchen.
3. **Mitnehmen**, wenn der Gegenstand beweglich und tragbar ist.
4. **Info**.

**Zweiter Ring „… öffnen mit“:** ein Eintrag je Methode, mit Icon, Dauer, Chance und Lärm:
- „mit Dietrich-Set knacken · ca. 4 s · 90 % · leise“
- „mit Brecheisen aufbrechen · 2 s · laut · Schloss kaputt“
- „Glas einschlagen · 1 s · sehr laut“ (Vitrine)
- „mit Stethoskop knacken · ca. 20 s“ (Tresor)
- Später: Schlüssel (sofort, leise), Akkubohrer, Bolzenschneider, Sprengstoff.
- Was nicht geht, ist ausgegraut und nennt den Grund: „kein Brecheisen dabei“, „Schlösser 6 nötig – Rosa
  hat 3“, „Tresor lässt sich nicht aufbrechen“.
- In der Mitte des Rings führt „zurück“ zum ersten Ring.

**Daten:** Statt fest eingebauter Felder (Knacken, Aufbrechen, Glas) hat jedes Schloss eine Liste von
**Öffnen-Methoden** `FDCOpenMethod`: Werkzeug, Fertigkeit, Schwierigkeit, Grunddauer, Lärm und Folge
(Schloss bleibt heil / ist kaputt / Glas ist kaputt). Türen nutzen dieselbe Liste und dasselbe Menü,
Holztüren lassen sich dann auch aufhebeln (laut).

**Möbel mit Schubladen:** Ist eine Schublade versperrt, fragt „Durchsuchen“ im zweiten Ring: „Schublade
ist versperrt – öffnen mit …“ oder „ohne die Schublade durchsuchen“.

**Beim Einbruch (Vorschlag):** „Nach Wertvollem durchsuchen“ packt Wertvolles von selbst ein, das
Wertvollste pro Platz zuerst. Das Spiel **hält nur an**, wenn nicht alles passt oder etwas Besonderes
dabei ist (Schlüssel, Dokument, großes Stück). Das spart viele Pausen. Als Schalter in den Optionen bleibt
„bei jedem Fund anhalten“ (wie bisher) zum Vergleichen.

## D. Gepäck statt Trageplätze, Kleidung extra

Jede Figur hat zwei getrennte Bereiche.

### 1. Kleidung & Taschen: was man anhat
- Feste Plätze: **Kopf, Körper, Hände, Füße, Weste, Rücken**, je ein Stück (Strickmütze, Sturmhaube ·
  Arbeitskleidung, Anzug · Handschuhe · Turnschuhe, Arbeitsschuhe · Werkzeugweste · Rucksack, Sporttasche).
- Jedes Stück hat Eigenschaften für später: **Auffälligkeit** (Sturmhaube nachts auf der Straße = sehr
  auffällig), **Gesicht verdeckt**, **keine Fingerabdrücke** (Handschuhe), **leise Sohlen** (Schrittlärm
  × 0,7, wirkt sofort, weil es Lärm schon gibt) und **Platz** (Taschen).
- Das Profil zeigt einen Satz für Zeugen: „Zeugen sehen: Mann, dunkle Arbeitskleidung, Strickmütze,
  grüner Rucksack“. Ausgewertet wird das erst mit Zeugen und Polizei.
- Angezogen wird in der Vorbereitung. In der Planung gibt es keine Umzieh-Schritte. Ausnahme: Hebt man eine
  Weste oder einen Rucksack auf und der Platz ist frei, zieht die Figur sie an.

### 2. Gepäck: alles, was man dabeihat
- **Eine Liste** für alles: Werkzeug und Beute zusammen, automatisch sortiert (Werkzeug oben, dann Beute
  nach Wert, dann Sonstiges). Jede Zeile hat Icon, Name, Platz, kg und Wert.
- Oben stehen zwei Balken: **Platz 9 / 34** und **Gewicht 12,4 / 64 kg**, dazu „Beute: 23.500 €“.
- **Platz** ist eine Zahl je Gegenstand, die alte Rasterfläche B × H: Ring, Kette, Uhr, Geld, Brosche,
  Dietrich 1 · Stethoskop, Lampe, Porzellanfigur, Schatulle 2 · Brecheisen 4 · Laptop, Aktenordner 6.
- **Platz kommt von Kleidung und Taschen:** Arbeitskleidung 6, Werkzeugweste +8, Rucksack +20 (die alten
  Rastergrößen 3×2, 4×2, 5×4). Vic mit Kleidung allein hat also 6 Platz, mit Weste und Rucksack 34.
- **Gewicht** wie bisher: Grenze 40 + 4 × Kraft kg, ab 15 kg langsamer.
- **Groß** (Gemälde, Bronzefigur) braucht keinen Platz, sondern wird **in den Armen** getragen: höchstens ein
  großes Stück pro Person, und man geht langsamer. Für Arbeiten und Türen legt die Figur es automatisch ab
  und nimmt es wieder auf (+2 s bzw. +1 s, steht im Schritt). Man sieht es vor dem Körper.
- **Sperrig** (Klavier) geht allein nicht. Später kommen „zu zweit tragen“ und die Sackkarre.
- **Werkzeug ist immer griffbereit**, sobald es im Gepäck ist. Es gibt keine Hände-Regeln mehr, kein „tief
  im Rucksack“ und kein „kurz ablegen“ für eine Hand. Beim Arbeiten sieht man das Werkzeug in der Hand,
  aber nur als Optik.
- Behälter, die man mitnimmt (versperrte Schatulle, Brieftasche), stehen als *eine* Zeile im Gepäck:
  „Schmuckschatulle (versperrt, Inhalt unbekannt)“. Aufmachen geht später.
- **Ablegen:** in der Planung ein Schritt „Brecheisen ablegen“, live sofort. Später kommt „Übergeben an …“,
  indem man einen Gegenstand auf eine andere Karte zieht.

### 3. Fund-Fenster und Möbel ohne Raster
- Gefundenes erscheint als Kacheln (Icon, Name, Platz, kg, Wert) ohne Positionen. Dazu gibt es „nehmen“,
  „Alles Wertvolle“ und „liegen lassen“.
- Daneben zeigt das Gepäck der Figur die zwei Balken. Passt etwas nicht, steht dort: „Laptop braucht 6 –
  noch 4 frei: etwas dalassen?“, und man kann eigene Dinge zurücklegen.
- Möbelteile bleiben geschachtelt, weil Schlösser zählen: „Schreibtisch › Schublade (versperrt)“.

### Was wegfällt
- Linke und rechte Hand, die Raster von Taschen, Weste, Rucksack und Möbeln, Drag & Drop zwischen Taschen.
- „Werkzeug griffbereit“ (Hand/Weste vs. Rucksack), „beide Hände frei“, „kurz ablegen“ für eine Hand, die
  Anzeige „Hände danach“.
- „Wohin damit?“ (Schritt 2 aus `Plan-Naechste-Schritte.md`).
- Im Code: `SDCItemGrid`, `GridPos`, `CheckPlaceAt`, `FindFreeSpot`, `GetUsedCells`, `EDCEquipSlot::LeftHand/
  RightHand`, `CountFreeHands`, `FindReadyTool`. `GridSize` wird zu `Space` (eine Zahl).

---

## Reihenfolge (jeder Schritt im Spiel sichtbar und getestet, dann Commit)

1. **Gebaut 2026-11-09 (Schritt 38).** **Kamera folgt** (klein): Folgen mit Totzone, Lösen beim Schieben, F / Karte, Hinweis oben.
   Selbsttests Villa und Signals prüfen den Wechsel.
2. **Gebaut 2026-11-09 (Schritt 38).** **Fußspuren und Schilder:** Fußabdruck aus Blender, ISM je Figur, Schilder-Overlay mit Hover und Klick,
   neue Icons, Tür- und Vorschau-Schilder im gleichen Stil, alte Marker raus. Neuer Selbsttest
   `-DCSelfTest=Trail` (Villa: Vic knackt und durchsucht, Rosa wartet; Screenshots aus der Spielkamera).
3. **Gebaut 2026-11-09 (Schritt 39), mit zwei Abweichungen.** **Durchsuchen für alles, „Öffnen mit …“:**
   Öffnen-Methoden als Daten, zweiter Ring im Radialmenü, Versteck für jedes Möbelstück, Türen mit demselben
   Menü. Live: Wertvolles von selbst einpacken, Pause nur bei Platzmangel oder Besonderem (Schalter für
   „immer anhalten“).
   - Abweichung 1: Die Methoden werden aus den bisherigen Katalogfeldern abgeleitet (Knacken, Aufbrechen, Glas)
     statt aus einer eigenen Liste. Eine Liste kommt, wenn neue Werkzeuge dazukommen (Schlüssel, Bohrer).
   - Abweichung 2: Das Tür-Menü bleibt wie es ist, bis Frage 4 (Türen aufhebeln) entschieden ist – mit nur
     „knacken“ wäre ein zweiter Ring leer.
4. **Gepäck (Regeln):** neues `UDCInventoryComponent` (Kleidung + Gepäck), Prüfungen ohne Hände,
   großes Stück in den Armen, Simulation und geplantes Inventar angepasst.
5. **Gepäck (Oberfläche):** Profil rechts (Kleidung-Kacheln, Zeugen-Satz, Gepäckliste mit Balken,
   „Jetzt | Nach dem Plan“ bleibt), Fund-Fenster ohne Raster.
6. **Kleidung mit Wirkung:** neue Stücke im Katalog (Sturmhaube, Handschuhe, Turnschuhe, Sporttasche),
   leise Sohlen wirken auf den Lärm, Zeugen-Satz.

Neu zu schreiben bzw. anzupassen sind die Selbsttests Inventory, Grid (wird `Pack`), Containers,
PlanInventory, PlanView, Find, Villa und AutoDoor.

## Offene Fragen an den User

1. **Platz als Zahl** (Gepäck 9 / 34) statt Raster – einverstanden?
2. **Beim Einbruch:** Wertvolles von selbst einpacken und nur bei Platzmangel oder Besonderem anhalten,
   oder weiter bei jedem Fund anhalten?
3. **Kamera in der Planung:** mit Totzone mitfahren (Vorschlag) oder nur beim Wechseln der Figur springen?
4. **Türen aufhebeln** mit dem Brecheisen (laut, Schloss kaputt) – soll das gehen?
5. **Werte:** Platz von Kleidung, Weste, Rucksack; welche Kleidungsstücke es geben soll.
