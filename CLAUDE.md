# CLAUDE.md – der clou 3

Taktisches Einbruchs-Planungsspiel in **Unreal Engine 5.8** (C++), eigenständiger geistiger Nachfolger
von „Der Clou!“. **Keine** Namen, Figuren, Texte, Grafiken oder Musik des Originals verwenden – nur
Genre und Mechaniken. Mit dem User **Deutsch** sprechen.

## Stand (2026-11-06)

Das Projekt wurde am 2026-11-05 bewusst auf ein leeres Gerüst zurückgesetzt. Wir bauen **Schritt für
Schritt**; jeder Schritt muss im Spiel sichtbar und bedienbar sein, bevor der nächste kommt.

**Fertig (Map `/Game/DerClou/Maps/L_Room01`, Startmap):**
- Raum 7 × 10 m (Wände als normale Cube-Actors im Outliner-Ordner „Raum“), Tür „Hintertür“ hinten.
- `ADCCrewCharacter` „Vic“: Character + AIController, NavMesh; Figur anklicken = auswählen, Boden = hingehen.
- `ADCDoor`: Radialmenü (`UDCRadialMenu`) mit Öffnen/Schließen, Durchgehen, Schloss knacken (wenn
  `bLocked`, rotes Vorhängeschloss), Info. Unsichtbarer `UDCNavBlockerComponent` sperrt den Weg, solange
  die Tür zu ist. Kleine Befehlskette (`QueueDoorAction`), Tätigkeit/Hinweise unter dem Namen.
- `IDCInteractable` + `UDCInfoPanel`: „Info“ im Radialmenü zeigt live alle Infos zum Objekt (oben rechts).
- `ADCCameraPawn`: WASD, Q/E bzw. rechte/mittlere Maustaste ziehen (drehen/neigen), Mausrad.
- **Inventar (Schritt 1)**, angelehnt an Arma, aber einfacher:
  - Katalog in C++ (`DCItemTypes.cpp`, später evtl. Data Assets): Gewicht, Volumen (L), Größenklasse
    klein/mittel/groß/sperrig, Wert, Trageplatz, Behälter-Fassung.
  - `UDCInventoryComponent` an der Figur: Kopf, Kleidung, Weste, Rücken, linke/rechte Hand. Behälter
    (Kleidungstaschen 3 L klein, Weste 6 L klein, Rucksack 25 L bis mittel). „Groß“ nur mit beiden
    Händen, „sperrig“ oder > 40 kg zu schwer für eine Person, max. 60 kg. Last ab 15 kg und beide
    Hände voll bremsen.
  - `ADCItemPickup` in der Welt (Ordner „Gegenstaende“, 9 Testgegenstände inkl. Klavier):
    Radialmenü Aufheben/Info, Name beim Überfahren. Vic startet mit Arbeitskleidung, Dietrich, Taschenlampe.
  - `UDCInventoryPanel` (Taste I / Knopf unten links): Drag & Drop, grün/rot beim Ziehen, Begründung
    bei Ablehnung, „ablegen“ bzw. aus dem Fenster ziehen = auf den Boden.
  - Selbsttest: `-game -DCSelfTest=Inventory` (Aufheben, Regeln, Screenshot nach
    `Saved/Screenshots/inventory_selftest.png`).
- **Gegenstände in Gegenständen (Schritt 2):**
  - Möbel sind auch Gegenstände (`UDCItem`), beliebig verschachtelt: Schreibtisch > Schublade
    (Einbauteil, `FixedParts`) > Schmuckschatulle > Ring. Eigenschaften im Katalog: beweglich/fest
    eingebaut, Behälter (Liter, max. Größe), mit Tür/Deckel, starr/weich (weich wächst mit Inhalt),
    Schloss (Knack-Dauer, aufzubrechen ja/nein). Zustände: zu/offen/versperrt/aufgebrochen.
  - Knacken braucht das Dietrich-Set (auch bei Türen), Aufbrechen das Brecheisen (Schloss danach
    kaputt). Tresor: fest eingebaut, nur knackbar (20 s).
  - Startinhalt pro platziertem `ADCItemPickup`: `Contents` (ItemId + Pfad `Inside`, z. B.
    „desk_drawer/jewelry_box“; ein Eintrag mit der Id eines Einbauteils stellt dessen Zustand ein).
  - „Durchsuchen“: Figur geht hin, `UDCContainerPanel` rechts zeigt den Baum (öffnen, knacken,
    aufbrechen, nehmen), Drag & Drop zwischen Behälter und Inventar, Verstecken in Möbeln (Sofa-Polster).
    Fenster schließt, wenn die Figur weggeht. Gemeinsame Slate-Bausteine in `DCItemWidgets`.
  - Raum: Schreibtisch + Stuhl, Wandtresor, Kleiderschrank (Mantel > Brieftasche > Geld), Sofa (Geld
    versteckt), Sessel, Esstisch + 2 Stühle, Umzugskarton, Schmuckschatulle (versperrt), Klavier u. a.
  - Selbsttest: `-game -DCSelfTest=Containers` (Screenshot `containers_selftest.png`).
- **Lärm (Schritt 3):**
  - `UDCNoiseSubsystem` (`DCNoise.h`): `Emit(Ort, Reichweite, Was, Quelle)`; sichtbar als Ring auf dem
    Boden, der auf die Reichweite anwächst (grau leise < 200 cm, gelb hörbar, orange laut, rot sehr laut
    ≥ 1200 cm). Oben in der Mitte `UDCNoiseHud`: letzter Lärm ab 300 cm und roter Alarm-Banner.
  - Reichweiten in `DCItems::Noise`: Schritte 110 cm (+ Last, + beide Hände voll), Knacken-Klicken
    140, Tür auf/zu 320/420, Aufbrechen 1300, Glas einschlagen 1700, Fallenlassen 25 cm/kg (max. 900).
  - Wände dämpfen auf 35 %, geschlossene Türen auf 50 % (Linientrace, Wände = `AStaticMeshActor`).
  - `ADCNoiseListener` „Nachbar“ (Outliner „Personen“, hinter der linken Wand): Misstrauen 0–100 %,
    Symbol zzz/?/!/!!, beruhigt sich nach 5 s Ruhe; bei 100 % Alarm („ruft die Polizei“). Anklicken = Info.
  - Vitrine (versperrt, nur knackbar, Glas einschlagen mit Brecheisen → „eingeschlagen“): Diamantbrosche,
    Porzellanfigur.
  - Getragenes sichtbar an der Figur: groß vor dem Körper, sonst rechte/linke Hand, Rucksack am Rücken.
  - Selbsttest: `-game -DCSelfTest=Noise` (Screenshots `noise_selftest*.png`).
- **Fähigkeiten (Schritt 4)**, angelehnt an die Bandenmitglieder des Genres (eigene Bezeichnungen):
  - `FDCSkills` (`DCSkills.h`) pro Crew-Mitglied, 0–10: Schlösser, Tresore, Elektronik, Sprengstoff,
    Fahren, Kampf, Aufpassen, Kraft. Benutzt werden bisher Schlösser, Tresore, Kraft (Rest gedimmt).
  - `DCSkills::Evaluate(Skill, Wert, Schwierigkeit, Grunddauer)`: mehr als 2 unter der Schwierigkeit
    = unmöglich; 1–2 darunter = Dauer ×1,5/×2, 25/50 % Fehlschlag, lauter; gleich = normal; darüber =
    schneller (bis ×0,4) und leiser (nur Klicken; Holz/Glas krachen immer). Fehlschlag: Zeit weg,
    Schloss bleibt zu.
  - Schlösser im Katalog: Fertigkeit, Schwierigkeit, Werkzeug (`Lock(...)`), Aufbrechen mit Kraft-
    Schwierigkeit. Tresor: Tresore 6 + Stethoskop, nicht aufzubrechen. Türen: `LockDifficulty`.
  - Voraussetzungen (`CheckItemAction`/`CheckDoorAction`): Werkzeug griffbereit (Hand, Kleidung,
    Weste – nicht tief im Rucksack), Knacken und Aufbrechen beide Hände frei (die Werkzeug-Hand zählt
    als frei), Öffnen/Schließen/Einschlagen eine Hand. Kraft bestimmt die Traglast (40 + 4 × Kraft kg).
  - Knöpfe: Tooltip mit Dauer, Erfolgschance und Fertigkeit; unmögliche ausgegraut, Klick sagt warum.
  - Crew: Vic (Schlösser 6, Kraft 6, Tresore 1, Dietrich-Set) und Rosa (Tresore 7, Elektronik 6,
    Schlösser 3, Kraft 3, Stethoskop, Outliner „Crew“). Anklicken wechselt, das Inventar zeigt die Werte.
- **Zeitleiste (Schritt 5) – das Herzstück:**
  - Modi (`EDCPlanMode`): **Planung** (Start; Klicks hängen Schritte an den Plan der ausgewählten Figur),
    **Abspielen**, **Direkt** (Klicks wirken sofort, zum Ausprobieren). Plan in `UDCPlanSubsystem`
    (GameInstance) – Schritte `FDCPlanTask`: Gehen, Warten, Tür (knacken/öffnen/schließen/durch),
    Gegenstand (knacken/aufbrechen/einschlagen/öffnen/nehmen, auch Einbauteile), Aufheben, Ausräumen
    (öffnet Unverschlossenes, nimmt alles mit Wert). Ziele per Actor-Name, überleben das Neuladen.
  - **Zustandsmanager** `UDCHeistStateSubsystem` (`DCHeistState.h`): Momentaufnahme aller Türen und
    Gegenstandsbäume; `Simulate()` spielt alle Pläne ereignisgesteuert durch (Weg per NavMesh-Pfad,
    Dauer aus Fähigkeits-Prüfung, Ereignisse „Tür aufgesperrt“ …). Braucht ein Schritt einen Zustand,
    den erst jemand anderes herstellt, wartet die Figur (grau in der Zeitleiste); passiert das nie →
    rot „geht nicht“. Geplanter Endzustand bestimmt, was die Planungs-Menüs anbieten.
  - Abspielen: `UDCPlanRunnerComponent` je Figur – hingehen, am Ort warten bis der echte Zustand passt
    (bei Türen seitlich, damit der Knacker Platz hat), ausführen, Schloss-Fehlschläge bis zu 3× neu.
    „Zurück zur Planung“ / erneutes Abspielen lädt den Raum neu (Plan bleibt, `bPlayAfterLoad`).
  - Radialmenü: zweite Zeile mit Dauer · Erfolg · Einschätzung (Tooltip lang), Unmögliches ausgegraut.
  - `UDCTimelinePanel` unten (Abspielen/Pause/Tempo 1–4×/Warten +5 s/Schritt zurück/Plan leeren/Direkt/
    Inventar; Leertaste, Rücktaste), `ADCPlanMarker` = nummerierte Punkte + Linien in der Welt.
  - Test-Beispiel: Vic knackt Schublade, räumt aus, knackt Hintertür; Rosa wartet an der Tür (geplant
    10,2 s), geht durch und zurück, knackt Tresor. Geplant 41 s, gespielt 40 s.
- Offen für spätere Schritte: Polizei kommt wirklich (Zeitlimit), Wachen/Hunde als Zuhörer (Aufpassen),
  Alarmanlagen (Elektronik), Gegner überwältigen (Kampf), schleichen vs. rennen, zu zweit tragen /
  Sackkarre, Übergeben zwischen Crew-Mitgliedern, Plan-Schritte verschieben/einfügen, „warten auf
  Signal“, Lärm-Vorhersage in der Zeitleiste, sich im Schrank verstecken, Fähigkeiten verbessern.
  Katalog als Data Assets erst, wenn der User Werte selbst im Editor ändern will.

**Noch vom User zu bestätigen:** Kamera drehen per Maus/Q-E, Infofenster-Breite, Tür seitlich schließen,
Inventar und Durchsuchen per Maus (Drag & Drop wurde nur im Code-Selbsttest geprüft, nicht mit echter Maus),
Lärm-Balance (wie schnell der Nachbar misstrauisch wird), Fähigkeiten-Balance und Tooltips per Maus,
Zeitleiste per Maus und im PIE (Neuladen wurde nur im `-game`-Selbsttest geprüft).
- Command-line-Selbsttests: `-game -DCSelfTest=` `CloseFromSide`, `Inventory`, `Containers`, `Noise`,
  `Timeline`, `Reload`. Die Direkt-Tests schalten selbst in den Direkt-Modus.

**Wichtige Erkenntnisse:**
- MCP über HTTP ist langsam (~7 s pro Aufruf), weil der Editor im Hintergrund gedrosselt läuft.
- NavMesh: `AgentRadius=30`, 10-cm-Raster (DefaultEngine.ini) – sonst passt die Wegfindung nicht durch
  100-cm-Türen. Türblatt/Rahmen beeinflussen die Navigation nicht, nur der Sperrblock.
- Slate-Buttons mit `IsFocusable(false)`, sonst kommen Tastatureingaben danach nicht mehr im Spiel an.
- Grundformen der Engine nutzen `DefaultMaterial`; Farbe nur über `BasicShapeMaterial` (`DCVisuals::Paint`).
- Selbsttest: Haken `bDebugWalkThroughDoorOnStart` an der Figur spielt „Schloss knacken → Durchgehen“
  beim Spielstart ab; Ablauf steht im Log (`LogDerClou`, Phasenwechsel).
- Selbsttest ohne Klicken: Spiel mit `-game -DCSelfTest=CloseFromSide` starten (Durchgehen, hinters
  offene Türblatt stellen, von dort schließen, beendet sich selbst; Log: `PASSED`/`FAILED`).
- Offenes Türblatt: eigener Sperrblock `OpenPanelBlocker` (erst 70 cm ab Scharnier, weil die Engine
  Sperrblöcke um den Figurenradius vergrößert – sonst wird der Durchgang zu eng). Türblatt ignoriert
  Pawn-Kollision. Schließen von der Aufschwing-Seite: Figur stellt sich neben die freie Kante, außerhalb
  des Schwenkbereichs.

Der alte Stand (Juwelier-Planer, Raum-Prototyp mit eigener Simulation, Konzept-Doku) liegt im
Git-Archiv: Tag und Branch **`archiv/prototyp-1`**. Nur bei Bedarf daraus Ideen holen, nichts
ungefragt zurückkopieren.

## Technik

- **Unreal-Bordmittel zuerst:** `ACharacter` mit NavMesh-Wegfindung (`AIController`/`SimpleMoveTo`),
  Interaktionsobjekte als eigene Actors, UI mit UMG oder Slate in C++.
- Ein Spielmodul: `Source/DerClou3` (Log-Kategorie `LogDerClou`). Weitere Module erst, wenn nötig.
- Assets unter `/Game/DerClou/…` mit Präfixen `SM_`, `BP_`, `M_`, `MI_`, `DA_`, `WBP_`.
- Lesbarkeit vor Atmosphäre: Figuren, Türen und Klickziele müssen von oben klar erkennbar sein.

## Bauen und Editor

- Bauen nur bei geschlossenem Editor (Live Coding blockiert sonst):
  ```
  "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" DerClou3Editor Win64 Development -Project="C:\Dev\der-clou-3\DerClou3.uproject" -WaitMutex
  ```
- Ich darf den Editor jederzeit selbst öffnen, schließen und neu starten. Vorher Play beenden und
  eine ungespeicherte Map speichern, damit nichts verloren geht.
- **MCP:** Der Server startet mit dem Editor (lokal eingestellt) auf `localhost:8000/mcp`, Client-
  Konfiguration in `.mcp.json`. Vor längeren MCP-Sitzungen committen. Keine Assets ohne Rückfrage löschen.
- `DerClou3.uproject` enthält lokal eine rechnerspezifische Engine-GUID – diese Änderung **nie**
  committen (im Repo steht `"5.8"`).
- `.umap`/`.uasset` laufen über Git LFS (`lockable`); lokal ist `lfs.setlockablereadonly=false` gesetzt.

## Git

- Branch `claude/sharp-rubin-uqa487`, Commit-Nachrichten Deutsch, nach jedem fertigen Schritt
  committen und pushen. Keinen PR ohne Auftrag.

## Sprache im Code

- Code, Bezeichner und Code-Kommentare Englisch; UI-Texte und Doku Deutsch.
