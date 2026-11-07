# CLAUDE.md – der clou 3

Taktisches Einbruchs-Planungsspiel in **Unreal Engine 5.8** (C++), eigenständiger geistiger Nachfolger
von „Der Clou!“. **Keine** Namen, Figuren, Texte, Grafiken oder Musik des Originals verwenden – nur
Genre und Mechaniken. Mit dem User **Deutsch** sprechen.

## Stand (2026-11-07)

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
    (seit Schritt 12 Raster: Kleidung 3×2 klein, Weste 4×2 klein, Rucksack 5×4 bis mittel). „Groß“ nur mit beiden
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
    eingebaut, Behälter (Raster, max. Größe), mit Tür/Deckel,
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
  - Test-Beispiel: Vic knackt Schublade, räumt aus, knackt Hintertür; Rosa wartet an der Tür (geplant
    10,2 s), geht durch und zurück, knackt Tresor. Geplant 41 s, gespielt 40 s.
- **Plan übersichtlich + geplantes Inventar (Schritt 6)**, nach User-Feedback „unübersichtlich, wer was
  wann macht“ und „aufgehobenes Brecheisen muss ich später einsetzen können“:
  - Simulation führt pro Figur ein **geplantes Inventar** (Wegwerf-Kopie: `UDCInventoryComponent::CopyFrom`,
    `UDCItem::Clone/CreateBare`, `UDCItem::FMuteScope` gegen UI-Refresh). Aufheben/Nehmen/Ausräumen legen
    hinein (gleiche Regeln wie live, Ausräumen nimmt nur, was passt), Ablegen nimmt heraus; Prüfungen
    (`PlanCheckItem/PlanCheckDoor(…, With)`) und Gehtempo nutzen es. Planungs-Menüs prüfen mit dem
    Inventar am Ende des Plans der Figur (`GetPlannedInventory`); im Plan schon Mitgenommenes ist ausgegraut.
  - **Kurz ablegen:** Was in *einer* Hand liegt (nicht das Werkzeug, nicht „groß“), legt die Figur für
    Knacken/Aufbrechen/Einschlagen kurz ab (+1 s, „legt Brecheisen kurz ab“) – in Planung und live gleich,
    weil beide die Dauer aus derselben Prüfung nehmen. Große Dinge in beiden Händen: selbst ablegen.
  - Neuer Plan-Schritt **Ablegen** (`EDCPlanTaskType::Drop`, Knopf „ablegen“ bei „Hände danach“).
  - Wartet jemand, merkt sich die Simulation **auf wen und welchen Schritt** (`WaitFor`, `WaitForIndex`).
  - `UDCTimelinePanel` „Plan & Zeitleiste“: links **Ablaufliste** der ausgewählten Figur in Sätzen (Nummer
    wie am Boden, Uhrzeit, Dauer/Chance, „→ Rechte Hand“, „nimmt Brillantring …“, rot mit Grund wenn es
    nicht geht, eigene Zeile „wartet 10,2 s – bis Vic mit „Hintertür knacken“ fertig ist“, bei der anderen
    „→ Rosa wartet darauf“), Knöpfe ↑ ↓ × je Schritt (`MoveTask`, `RemoveTask`), „Hände danach“.
    Rechts Zeitleiste aller Figuren in einem Canvas: Nummern in den Blöcken, weiße Linie vom Schritt, auf
    den gewartet wird, zur wartenden Figur, Name anklicken = deren Plan.
  - **Zeit-Vorschau:** Klick/Ziehen in der Zeitleiste oder Klick auf einen Schritt setzt eine Vorschauzeit:
    Stecknadeln im Raum zeigen, wo jede Figur dann steht und was sie tut („Rosa: 1. wartet auf Vic“),
    Zeile darunter fasst es zusammen; Esc oder „Vorschau aus“ beendet sie (`SetPreviewTime`,
    `GetPositionAt`, `DescribeActivityAt`). Wege am Boden folgen dem NavMesh-Pfad; nur die ausgewählte
    Figur hat Nummern, die anderen sind klein und dunkel, der gewählte Schritt groß (`EDCMarkerLook`).
  - **Aufeinander abstimmen (Schritt 7):** Plan-Schritte haben eine feste `Id` (`FGuid`, bleibt beim
    Verschieben). `EDCPlanTaskType::WaitFor` (`Target` = Figur, `Ref` = Schritt-Id; ohne `Ref` = „bis
    Rosa ganz fertig ist“) und `Signal` (`Action` = Nummer, 0,5 s). Simulation: jeder fertige Schritt
    ist ein Ereignis (`DoneTasks`, `DoneCrews` im Snapshot); live fragt der Runner den Runner der
    anderen Figur (fertig = erledigt oder aufgegeben). Automatisches Warten (Ausräumen wartet von
    selbst, bis der Tresor offen ist) bleibt zusätzlich.
  - **Warten-Dialog** `UDCWaitDialog` (Knopf „Warten …“ neben „Plan von Vic“): „Worauf soll Vic warten?“
    – feste Zeit (5/10/20/30 s), „bis Rosa Schritt N erledigt hat“ (legt bei Rosa direkt danach „Vic
    das Zeichen geben“ an, ein vorhandenes Zeichen wird geteilt) oder „bis Rosa mit allem fertig ist“;
    Vorschau-Satz mit Uhrzeiten und Wartezeit, „Übernehmen“ (Enter) / „Abbrechen“ (Esc), Doppelklick =
    übernehmen. `FDCWaitChoice`, `ApplyWaitChoice`. Warten und Zeichen gehören zusammen: Zeichen löschen
    löscht die Waits darauf, letzten Wait löschen löscht das Zeichen (`RemoveTask`).
  - Zwei Figuren am selben Gegenstand: die zweite sucht sich einen freien Platz daneben (±40°/80°/120°).
- **Interface aufgeräumt (Schritt 8)** nach `Docs/UI-Leitfaden.md` (vom Interface-Agenten
  `.claude/agents/game-ui.md` angelegt – bei UI-Arbeit zuerst lesen):
  - `UDCCrewBar`: Profilkarten links (Farbe + Initiale, Taste 1–9, beste Fähigkeiten, Plan-Stand bzw.
    Tätigkeit, rot wenn ein Schritt nicht geht). Klick = wählen + Kamera hin (`ADCCameraPawn::FocusOn`),
    „Profil“ = Profil & Inventar (das Inventarfenster heißt jetzt „Profil – Vic“ und öffnet rechts neben
    den Karten).
  - Planungsleiste mit fester Breite: Umschalter „Planen | Ausprobieren“ (orangefarbener Rahmen beim
    Ausprobieren), Hauptknopf „Abspielen“ gelb gefüllt (`DCItemWidgets::PrimaryButtonStyle`), Tempo,
    „Plan leeren“ rot mit zweitem Klick zur Bestätigung. Schrittliste: eine Zeile pro Schritt (Nummer,
    Zeit, Name, rechts „17 s · 100 %“ / „→ Vic wartet“ / „geht nicht“); Klick klappt Details und
    „↑ früher / ↓ später / × löschen“ auf. Abstimmung in Cyan (`CoordinationColor`), Schrift ≥ 11–12.
  - Rückmeldungen als Meldung über der Leiste (`ShowToast`): „Vic, Schritt 3: …“, rot „Geht nicht: …“.
  - Status oben Mitte: „Nachbar: Misstrauen 0 %“; im Raum steht beim Nachbarn nur noch der Name.
- **Rahmen um die Szene (Schritt 9):** `UDCGameViewportClient` (in DefaultEngine.ini als
  `GameViewportClientClassName`) zeichnet die 3D-Szene nur in ein Teilrechteck (`ULocalPlayer::Origin/
  Size`, eigenes `LayoutPlayers`). `UDCFrame` (Z 0) füllt die Ränder fest und fängt dort Klicks ab:
  oben 44 (Modus „Planung/Plan läuft/Ausprobieren“ links, Status Mitte), links 292 (Profilkarten),
  rechts 452 (Profil & Inventar der gewählten Figur, immer sichtbar, ohne Schließen-Knopf), unten 330
  (Planungsleiste über die volle Breite, Zeitleiste wächst mit). Linie um die Szene in der Modusfarbe
  (bronze Planung, grün läuft, orange Ausprobieren). Info- und Durchsuchen-Fenster oben rechts in der
  Szene neben dem Profil, Meldungen und Dialoge in der Szene. Größen: `UDCFrame::TopHeight` usw.
- **Modelle für alle Gegenstände (Schritt 10)** – in Blender per Skript modelliert, kein Würfel mehr:
  - Pipeline in `Art/Items/`: `dc_mesh.py` (Grundformen, Drechseln, Sweeps, Polster, Fasen mit gehärteten
    Normalen, Box-UVs 1 UV = 1 m), `dc_items.py` (Möbel) + `dc_items_small.py` (Kleidung, Werkzeug, Beute),
    `dc_textures.py` (kachelbare PBR-Texturen aus FFT-Rauschen: Holz ×3, Stoff, Leder, Metall, Lack, Karton,
    Papier, Geldschein, Gemälde, Marmor, Bronze; `_BC`, `_N` DirectX, `_ORM`), `dc_materials.py` (eine Tabelle
    für Blender-Vorschau und Unreal: Textur, Tönung, Kachelung, Rauheit, Metall).
  - Blender headless: `blender -b --factory-startup -P Art/Items/build_items.py -- [--textures] [--items a,b]
    [--preview] [--export]` (Vorschau + Kontaktbogen in `Saved/ItemPreviews/`, FBX nach `Art/Items/Export/`).
    Das Blender-MCP-Addon ist nicht installiert (Installation wurde blockiert) – Skripte reichen.
  - Unreal (Editor zu): `UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="Art/Items/ue_import_items.py"`
    → `/Game/DerClou/Items/{Textures,Materials,Meshes}`: `M_ItemSurface` (Textur × Tönung, Rauheit, Metall),
    `M_ItemGlass` (durchsichtig), `MI_Item_<Name>` je Tabelleneintrag, Slots nach Namen, Box-Kollision.
    Python-Plugin und Editor Scripting Utilities sind dafür im .uproject aktiv.
  - Konvention: Ursprung unten Mitte, echte cm (Maße = `ShapeSize` im Katalog), Vorderseite Blender −Y =
    **Unreal +Y**. Gemälde: Leinwand nach −X. Im Level gedreht: Sofa/Klavier 180°, Kleiderschrank −90°,
    Sessel −140° (`Art/Items/ue_face_room.py`).
  - Code: `DCVisuals::ItemModel(Id)` lädt `SM_<Id>` (fehlt es, bleibt der farbige Würfel), `SetItemLook` für
    Pickups und getragene Gegenstände. `/Game/DerClou/Items` steht in `DirectoriesToAlwaysCook`.
  - Offen: Zustände sichtbar machen (Tresortür/Schublade offen, Vitrine eingeschlagen), Werte im Editor statt
    in Python, wenn der User selbst ändern will.
- **Nachtlicht im Raum (Schritt 11)**, Lesbarkeit zuerst – `Art/Items/ue_room_look.py` (nach dem Import laufen
  lassen, baut die Outliner-Ordner „Licht“ und „Deko“ jedes Mal neu; alle Werte oben im Skript):
  - Kühles Mondlicht (`Mondlicht`, 4,2 lx, steil von oben) füllt den ganzen Raum; warme Inseln (2700 K) kommen
    aus echten Lampen: Bankerlampe auf dem Schreibtisch (Spot), Stehlampen in Sofa- und Klavierecke. Schirme
    leuchten selbst (`EmissiveStrength` im Master-Material). Tageslicht-Teile der Vorlage (Wolken, Himmelskuppel)
    im Spiel aus, Himmelslicht aus fester Cubemap, blau getönt.
  - `Licht_Nachbearbeitung` (unbegrenzt): feste Belichtung (EV 1,2, pumpt nicht beim Kameraschwenk), Bloom
    0,35, Vignette, kühle Schatten / warme Lichter, lokale Belichtung, kein Bewegungsunschärfe.
  - Oberflächen: `M_RoomSurface` projiziert Texturen nach Weltposition (für skalierte Würfel): Holzdielen,
    Streifentapete, Beton im Hof; weiße Fußleisten (`SM_Deco_skirting`, nach Länge skaliert). Deko ohne
    Kollision. `DC_SKIP_MESHES=1` beim Import erneuert nur Texturen und Materialien.
  - Noch offen: vom User mit der Maus/im Editor ansehen; Wandoberkanten zeigen auch Tapete.
  - Texte über Figuren/Stecknadeln laufen durch `DCVisuals::PlainText` (TextRender-Schrift kennt kein
    „ “ – → …).
  - Selbsttest `-DCSelfTest=Signals`: alles über den Warten-Dialog – Rosa knackt den Tresor und gibt
    Vic das Zeichen, Vic wartet und räumt aus; geteiltes Zeichen, „bis Rosa fertig“, feste Zeit,
    gekoppeltes Löschen. Screenshots `signal_vic/rosa.png`, `wait_dialog.png`, `crew_profile.png`.
  - Selbsttest `-DCSelfTest=PlanInventory`: Brecheisen aufheben → Schublade aufbrechen → ausräumen,
    Verschieben/Löschen/Ablegen, Rosa wartet auf Vics Schritt 4, Vorschau, Abspielen (geplant 14,1 s,
    gespielt 13,5 s). Screenshots `plan_list_vic/rosa/done.png`.
- **Raster-Inventar (Schritt 12 = Schritt 1 aus `Docs/Plan-Naechste-Schritte.md`):**
  - Katalog: `GridSize` (Fußabdruck B×H, `{0,0}` = passt in kein Raster, nur Hände/gar nicht), `Grid`
    (Raster eines Behälters statt Liter), `ShortName` für die Kachel. `MaxContentSize` bleibt zusätzlich
    (Taschen und Weste nur „klein“ – sonst läge ein Laptop 3×2 in der Hosentasche). Volumen/starr/weich weg.
  - `UDCItem::GridPos` (Zelle im Elternbehälter, `DCItems::AnyCell` = keine), `CheckPlaceAt`,
    `FindFreeSpot` (zeilenweise von oben links), `GetItemAt`, `GetUsedCells`; `AddContent(Item, Cell)`
    setzt auf die Zelle oder die erste freie; `CheckCanHold(Item, Cell)` mit Gründen („Dort liegt schon
    Brecheisen“, „Ragt über den Rand“, „zu voll – kein freier Platz für …“). Einbauteile haben keine Zelle.
    Inventar: `CheckInto/MoveInto(…, Cell)`, auch Verschieben im selben Behälter. `Clone`, `CopyFrom`
    und `BuildSimItem` (`FDCSimItem::GridPos`) behalten die Zellen.
  - UI (`DCItemWidgets`): `SDCItemGrid` (Zellen 36 Einheiten, Gegenstände als Rechtecke mit Kurznamen,
    hochkant beschriftet bei 1×N), beim Ziehen Fußabdruck grün/rot und Begründung am gezogenen Gegenstand;
    über einem Behälter im Raster = hineinlegen. Anklicken wählt aus: „Ausgewählt“ zeigt Knöpfe und die
    Raster der Behälter, in denen er liegt (Schrank › Mantel › Brieftasche). Profil: zwei Spalten
    (Kleidung + Rücken | Kopf, Weste, Hände), Durchsuchen: Raster des Möbels und seiner Schubladen.
  - Selbsttest `-DCSelfTest=Grid` (Regeln, Fußabdrücke, Kopie, Aufheben in den Rucksack, Verschieben,
    Schrank/Schreibtisch; Screenshots `grid_profile/wardrobe/desk.png`).
  - Noch nicht: Drehen (R), Geld als Menge, „Wohin damit?“ (Schritt 2), Drag & Drop mit echter Maus ungetestet.
- **NÄCHSTE SCHRITTE – entschieden, noch nicht gebaut: siehe `Docs/Plan-Naechste-Schritte.md`.** Kurz:
  zwei Phasen (Planung grob → Einbruch, bei Funden und Störungen hält das Spiel an und fragt),
  Raster-Inventar nur mit Rechtecken, „Wohin damit?“ mit Vorschlag, Wissensstufen mit Nebel, Absichten
  bei nur vermuteten Objekten, Umschalter einmalig/wiederholbar. Reihenfolge und Details stehen dort.
  Spielregeln vor dem Bauen mit dem User besprechen.
- Offen für spätere Schritte: Polizei kommt wirklich (Zeitlimit), Wachen/Hunde als Zuhörer (Aufpassen),
  Alarmanlagen (Elektronik), Gegner überwältigen (Kampf), schleichen vs. rennen, zu zweit tragen /
  Sackkarre, Übergeben zwischen Crew-Mitgliedern, Schritte mitten im Plan einfügen (bisher: anhängen und
  mit ↑ verschieben), Abgelegtes im Plan wieder aufheben, Lärm-Vorhersage in der
  Zeitleiste, sich im Schrank verstecken, Fähigkeiten verbessern.
  Katalog als Data Assets erst, wenn der User Werte selbst im Editor ändern will.

**Noch vom User zu bestätigen:** Kamera drehen per Maus/Q-E, Infofenster-Breite, Tür seitlich schließen,
Inventar und Durchsuchen per Maus (Drag & Drop wurde nur im Code-Selbsttest geprüft, nicht mit echter Maus),
Lärm-Balance (wie schnell der Nachbar misstrauisch wird), Fähigkeiten-Balance und Tooltips per Maus,
Zeitleiste per Maus und im PIE (Neuladen wurde nur im `-game`-Selbsttest geprüft), ob die neue Ablaufliste
und Zeit-Vorschau übersichtlich genug sind, ob „kurz ablegen“ (+1 s) als Regel passt, Warten-Dialog,
Profilkarten und die neue Planungsleiste mit der Maus; Farbvorschläge aus dem UI-Leitfaden (Titel weiß);
Rahmen um die Szene (auch im Editor-PIE – der eigene Viewport-Client wurde nur im `-game`-Test geprüft).
- Command-line-Selbsttests: `-game -DCSelfTest=` `CloseFromSide`, `Inventory`, `Containers`, `Noise`,
  `Timeline`, `Reload`, `PlanInventory`, `Signals`, `Grid`. Die Direkt-Tests schalten selbst in den Direkt-Modus.

**Wichtige Erkenntnisse:**
- MCP über HTTP ist langsam (~7 s pro Aufruf), weil der Editor im Hintergrund gedrosselt läuft.
- NavMesh: `AgentRadius=30`, 10-cm-Raster (DefaultEngine.ini) – sonst passt die Wegfindung nicht durch
  100-cm-Türen. Türblatt/Rahmen beeinflussen die Navigation nicht, nur der Sperrblock.
- Slate-Buttons mit `IsFocusable(false)`, sonst kommen Tastatureingaben danach nicht mehr im Spiel an.
- Unity-Build: neue .cpp-Dateien mischen die Gruppen neu – Konstanten in anonymen Namespaces müssen
  projektweit eindeutig heißen (z. B. `SimItemReach`, `RunnerLootStepSeconds`).
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
