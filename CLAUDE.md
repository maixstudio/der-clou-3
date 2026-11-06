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
- Offen für spätere Schritte: Gegenstände in der Hand sichtbar an der Figur, zu zweit tragen /
  Sackkarre, Übergeben zwischen Crew-Mitgliedern, sich im Schrank verstecken, Lärm beim Aufbrechen,
  Dinge kaputt machen (Vitrine einschlagen), Katalog als Data Assets.

**Noch vom User zu bestätigen:** Kamera drehen per Maus/Q-E, Infofenster-Breite, Tür seitlich schließen,
Inventar und Durchsuchen per Maus (Drag & Drop wurde nur im Code-Selbsttest geprüft, nicht mit echter Maus).

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
