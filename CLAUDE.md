# CLAUDE.md – der clou 3

Taktisches Einbruchs-Planungsspiel in **Unreal Engine 5.8** (C++), eigenständiger geistiger Nachfolger
von „Der Clou!“. **Keine** Namen, Figuren, Texte, Grafiken oder Musik des Originals verwenden – nur
Genre und Mechaniken. Mit dem User **Deutsch** sprechen.

## Stand (2026-11-08)

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
- **Durchsuchen mit Fund-Pause (Schritt 13 = Schritt 3 aus dem Plan, vorgezogen auf Wunsch des Users):**
  - Planung: Radialmenü „Durchsuchen“ (`EDCPlanTaskType::Loot`, Label „… durchsuchen“) legt nur fest, *wer*
    *was* durchsucht. Die Simulation öffnet, was ohne Schlüssel aufgeht, nimmt aber **nichts** ins geplante
    Inventar; Dauer = 0,3 + 0,8 s je Öffnen + `DCItems::SearchSeconds(Zellen)` (1 s + 0,05 s je Feld des
    Möbels und seiner Schubladen). Anzeige „ca. 3 s · Inhalt unbekannt“.
  - Einbruch: `UDCPlanRunnerComponent` Phasen Öffnen → Suchen → Entscheiden → Handgriffe. Nichts da →
    „nichts gefunden“, weiter ohne Pause. Sonst `ADCPlayerController::RequestFindDecision` (Warteschlange,
    ein Fund nach dem anderen): Spiel pausiert, Figur ausgewählt, `UDCContainerPanel::OpenFind` – „Fund –
    Wandtresor“, „Rosa hat gefunden: …“, Raster neben dem Profil; nur schnelle Knöpfe (nehmen/öffnen/
    schließen), Ziehen in beide Richtungen, „Alles Wertvolle nehmen“, „Weiter“ (Enter). Jeder Handgriff
    kostet Spielzeit (nehmen 0,6 s, umpacken/dalassen 1 s; Profil meldet über `HandlingHook`); danach
    steht die Figur so lange dort (`ResolveFind`). Ergebnis als Meldung + Log („mitgenommen: … ·
    liegen gelassen: … · dagelassen: …“). Leertaste/Pause-Knopf („Fund offen“) wirken währenddessen nicht.
  - Selbsttests mit Abspielen setzen `UDCPlanSubsystem::bAutoDecideFinds` (nimmt alles Wertvolle, weiter).
    Neuer Test `-DCSelfTest=Find` (Screenshots `find_pause.png`, `find_decided.png`).
  - Radialmenü „… knacken und durchsuchen“ (bei versperrten Behältern und Schubladen) legt beide Schritte an
    (`IssueTasks`); Test-Hilfe `UDCRadialMenu::ChooseOption`.
  - Offen: Protokoll im Ergebnis-Bildschirm,
    Zeitleiste zeigt die Verschiebung durch Handgriffe noch nicht vorab.
- **Profil zeigt den Plan-Stand (Schritt 14, nach User-Bug „Statue: Hände nicht frei, dann ist schon weg“):**
  Ursache: der erste Klick hatte das Aufheben geplant, das Profil zeigte aber den Stand *jetzt*. Jetzt:
  `UDCInventoryPanel` zeigt in der Planung standardmäßig das geplante Inventar (`GetShownInventory`,
  `IsPlanView`, Umschalter „Jetzt | Nach dem Plan“ oben im Profil), Geplantes blau mit „· geplant“
  (`FDCItemTreeContext::IsPlanned`, Abgleich nach Anzahl je DefId), in dieser Ansicht nur lesen
  (Ziehen rot mit Grund, keine Knöpfe). Zusammenfassung „Nach dem Plan: 37,1 / 64 kg · Tempo 58 %“.
  Schon Geplantes sagt im Menü, wer es wann nimmt: „schon im Plan: Vic nimmt es in Schritt 2“.
  Menüklicks, „geht nicht“ und neue Plan-Schritte stehen jetzt im Log (`Menu:`, `Not possible:`,
  `Plan Vic: step …`) – auch im Editor. Selbsttest `-DCSelfTest=PlanView` (Screenshots `plan_view*.png`).
  Funde beim Durchsuchen erscheinen weiterhin erst im Einbruch (Inhalt ist in der Planung unbekannt).
- **Wege durch geschlossene Türen (Schritt 15, User-Bug „B läuft quer durchs Zimmer, geht nicht“):**
  Vorher schätzte die Planung ohne NavMesh-Weg eine gerade Linie (Faktor 1,4), live scheiterte der Gang
  sofort. Jetzt `WayThroughDoors` (DCHeistState.cpp): kein freier Weg → kürzester Weg über die
  Türseiten (`ADCDoor::GetStandPoint`, Teilstücke per NavMesh, „durch die Tür“ als Kante),
  `ADCDoor::CrossesOpening`, `DoorsOnPath`. Simulation: liegt eine im Plan-Zustand geschlossene Tür auf
  dem Weg, wartet die Figur **vor dem Losgehen** (`FDCScheduledTask::WalkStart`, Cursor `bWalkPending`/
  `WaitDoors`), bis jemand sie öffnet („wartet, bis Hintertür offen ist“, grauer Block vor dem Gehen,
  „wartet auf Vic“), sonst „geht nicht … passiert im Plan nie“. Live: `UDCPlanRunnerComponent::DoorOnWay`
  + `UDCHeistStateSubsystem::FindClosedDoorsOnWay` lassen die Figur warten (Ausprobieren: sofort
  „geht nicht“). Türen bleiben im NavMesh eine Null-Sperre (ein Versuch mit eigener NavArea ließ hinter
  der Öffnung eine Lücke). Selbsttest `-DCSelfTest=DoorWay`.
- **Haus für den Nebel (Schritt 16 = N1 aus `Docs/Plan-Naechste-Schritte.md`, Abschnitt 5):**
  - Map `/Game/DerClou/Maps/L_House01` (Editor-Startmap; `L_Room01` bleibt Spiel-Startmap für die
    Selbsttests). Gebaut von `Scripts/Levels/build_house.py` (Commandlet, Editor zu; vorher
    `Content/DerClou/Maps/L_House01.umap` löschen – das Skript überschreibt nicht). Es kopiert L_Room01
    (Himmel, Mond, Nachbearbeitung, Crew, Nachbar, NavMesh-Akteure), entfernt den Raum und baut: Garten
    (Start, Rucksack/Weste/Brecheisen), Flur (Kommode, Gemälde), Büro (Schreibtisch mit versperrter
    Schublade: Laptop, Akten; Wandtresor: Uhr, Kette, Geld), Schlafzimmer (Bett mit Geld unter der
    Matratze, Nachttisch mit versperrter Schatulle: Ring, Brosche; Kleiderschrank: Mantel > Brieftasche >
    Geld). Türen: Hintertür (versperrt, Schloss 3), Bürotür (versperrt, 5), Schlafzimmertür (zu).
    `Scripts/Levels/dump_level.py` listet die Akteure einer Map.
  - `ADCRoomVolume` (Raumname liegt lesbar auf dem Boden, `HalfSize`, `bOutdoors`, `FindAt`).
  - Neue Möbel im Katalog ohne Modell (farbige Kisten, bis der Blender-Chat sie baut): Kommode
    (`dresser`, 6×2), Bett (`bed`, Versteck 3×1), Nachttisch (`nightstand`, 3×2).
  - Selbsttest `-DCSelfTest=House` auf der Haus-Map (`UnrealEditor.exe DerClou3.uproject L_House01 -game
    -DCSelfTest=House`): Räume, Türen, Crew im Garten, Weg ins Büro wartet auf beide Türen, mit Knacken
    und Öffnen kommt Vic hin (Screenshots `house_plan.png`, `house_done.png`).
  - Noch kein Nebel – das ist N2.
- **Türregel „Wer durchgeht, öffnet“ + Profil flackert nicht mehr (Schritt 17 = Bauschritt 1 aus
  `Docs/Planung-Bedienung-Konzept.md`, nach User-Feedback 2026-11-08 „knacken öffnet nicht, Fehler erst am
  Schluss, Einfügen sperrig, Signal nicht auffindbar“):**
  - Neues Konzept für eine einfache Planung: `Docs/Planung-Bedienung-Konzept.md` (Analyse mit 4 Agenten,
    Design-Panel mit 3 Entwürfen, 12 Bauschritte, Varianten-Schalter zum Vergleichen). Spielregeln stehen dort.
  - Eine geschlossene, nicht verschlossene Tür auf dem Weg öffnet die Figur unterwegs selbst (+1,1 s,
    Ereignis „Tür offen“, Zeile „öffnet unterwegs Hintertür“). Knacken sperrt nur auf.
  - Verschlossene Tür auf dem Weg (User-Entscheidung „direkt an der Tür“): Die Figur geht bis zur Tür,
    wartet seitlich daneben (110 cm) und geht weiter, sobald die Tür im Plan aufgesperrt ist. Die Simulation
    hat dafür Teilstrecken (`FDCWalkLeg`, `FDCScheduledTask::Legs`, `WaitedOnTheWay`, `IsWaitingOnTheWay`).
    Zeitleiste, Vorschau und Stecknadeln folgen ihnen. Die rote Nummer steht dort, wo die Figur hängen bleibt.
  - Live: `UDCPlanRunnerComponent::DoorOnWay` öffnet über `InteractWithDoor(GoThrough)` (`PassingDoor`) und
    wartet an verschlossenen Türen daneben.
  - Profil flackerte, weil das geplante Inventar alle 0,5 s neu angelegt wurde. Jetzt wird es nur ersetzt,
    wenn sich der Inhalt ändert (`DescribeInventoryForCompare`, `PlannedInventorySignatures`). Nach einem
    NavMesh-Neubau rechnet die Planung neu (`OnNavigationRebuilt`).
  - Selbsttest `-DCSelfTest=AutoDoor` (L_House01) spielt genau den User-Fall durch: Vic knackt nur, Rosa
    wartet per „Warten …“, geht in den Flur, öffnet selbst und durchsucht die Kommode. DoorWay und House
    sind angepasst.
- **Tür-Menü, klare Texte, Aufgeben, Rückgängig (Schritte 18–19 = Bauschritte 1 Rest, 2 und 3 aus
  `Docs/Planung-Bedienung-Konzept.md`):**
  - Bleibt eine Tür im Plan zu, nennt der Grund, wer helfen kann: „Hintertür bleibt verschlossen – Vic könnte
    sie knacken (ca. 2 s)“ (oder „… selbst knacken“, „niemand hier kann sie knacken“). Mit vollen Händen:
    „Schlafzimmertür: keine Hand frei – erst ablegen oder jemand öffnet sie“. Gewartet wird nur auf die
    passende Änderung (`FCursor::bWaitForLock`, Vorher-Stand in `ApplyEvent`).
  - `FDCScheduledTask::WaitForLabel`: Wartezeile „wartet 3 s auf Vic (1. Hintertür knacken)“
    (`UDCHeistStateSubsystem::DescribeWaited`), Vorschau „wartet auf Vic (Hintertür knacken)“.
  - Live: `DoorOnWay` nimmt die erste nicht offene Tür (`FindDoorsOnWay`, früher `FindClosedDoorsOnWay`),
    wartet kurz auf den NavMesh, gibt nach 3 vergeblichen Öffnungsversuchen auf. Wer ein Türschloss knackt und
    danach stehen bleibt, tritt zur Seite. Wartet jemand vergeblich (der Knacker gab auf oder niemand
    arbeitet mehr), kommt die rote Meldung „Rosa kommt nicht weiter: Bürotür bleibt verschlossen“.
  - Tür-Menü: verschlossen → zuerst „knacken und durchgehen“ (zwei Schritte mit einem Klick); geschlossen →
    „Durchgehen“, dann „Öffnen“. Titel „Hintertür · verschlossen / zu / offen“ nach dem Plan
    (`DescribePlannedDoor`). Gehen heißt nach dem Zielraum: „gehen (Flur)“.
  - Ausprobieren: `TryMoveTo` – geht der Weg durch eine Tür, läuft er über den Runner (öffnet unterwegs,
    an verschlossener Tür „Bürotür ist verschlossen“). Roter Ring nur, wenn das Ziel gar nicht erreichbar ist
    (`CanReach`).
  - Rückgängig/Wiederholen: `UDCPlanSubsystem` mit Verlauf (50 Einträge), `FDCPlanEditScope` fasst
    Änderungen zu einem Schritt zusammen (ein Klick mit zwei Schritten = einmal Rückgängig), neue
    `InsertTasks`, `MoveTaskTo`. Knöpfe „Rückgängig“/„Wiederholen“ neben „Planen | Ausprobieren“ (Tooltip
    sagt, was), Strg+Z, Strg+Y / Strg+Umschalt+Z. „Plan leeren“ ohne zweiten Klick.
  - Test-Schalter `ADCCrewCharacter::bTestFailTasks` (jede Probe misslingt), `GetNotice`,
    `UDCRadialMenu::GetTitle`. Neuer Selbsttest `-DCSelfTest=Undo`; AutoDoor prüft jetzt auch Menü,
    volle Hände, offene + geschlossene Tür hintereinander, vergebliches Warten und Ausprobieren.
- **Sofort-Warnung und Plan-Status (Schritt 20 = Bauschritt 4):** Fehler fallen beim Klick auf, nicht am Schluss.
  - Simulation: `FDCScheduledTask::bAfterBlocked` (kommt nur wegen eines früheren Schritts nicht dran),
    `ShortReason` („Bürotür bleibt verschlossen“), `FDCSchedule::FindProblems()` (erster hängender Schritt je
    Figur, `FDCPlanProblem` mit Ort und Grund).
  - Jede Planänderung über den Controller (Klick, Menü, „Warten …“, ↑/↓/×, Rücktaste, Rückgängig) vergleicht
    die Probleme vorher/nachher (`CollectProblems`, `CheckPlanAfterEdit`): Ein neues Problem öffnet die rote
    Hinweisleiste über der Planungsleiste („Geht so nicht: Rosa, Schritt 1 „gehen (Büro)“ – Hintertür bleibt
    verschlossen – Vic könnte sie knacken (ca. 2 s)“, Knöpfe „Rückgängig“ / „Später“) und färbt den Klick-Ring
    rot. Ein Schritt hinter einem bekannten Problem meldet nur „… – kommt erst dran, wenn Schritt 1 geht“.
    Ist alles gelöst: „Passt jetzt – Plan geht auf“.
  - Planungsleiste oben: „Plan geht auf · fertig 0:41“ bzw. rot „Plan hängt: Rosa ab Schritt 1 – Hintertür
    bleibt verschlossen“ mit „Lösen …“ (zeigt den Schritt, Kamera hin, Hinweis wieder auf). Hängt der Plan,
    heißt der Hauptknopf „Trotzdem abspielen“ (nicht mehr gelb).
  - Liste: nur der erste hängende Schritt rot (Grund in 12 pt), die danach grau „kommt nicht dran“. Profilkarte:
    „hängt bei 1: Hintertür bleibt verschlossen“. Menüeinträge sagen rot „kommt nicht dran – Rosa hängt bei
    Schritt 1“, wenn die Figur vorher schon hängt (`EDCOptionTone`).
  - Selbsttest `-DCSelfTest=PlanHelp` (L_House01, Screenshots `planhelp_bar.png`, `planhelp_fine.png`).
- **Türen zeigen den Plan im Raum (Schritt 21 = Bauschritt 5):**
  - In der Planung zeigen Türen den geplanten Zustand zu dem Moment, den der Raum zeigt. Oben links neben
    „Planung“ steht „Raum zeigt: nach dem Plan (0:06)“, „… nach Vics Schritt 1 (0:03)“ oder „… 0:20
    (Zeitleiste)“ (`GetViewTime`, `DescribeViewMoment`). Ist das Knacken geplant, ist das Vorhängeschloss weg.
    Geht jemand im Plan durch, steht das Türblatt offen, vom Öffnenden weg geschwenkt. Über Türen, die der Plan
    ändert, steht ein Schild („aufgesperrt 0:03 · Vic“, „offen 0:05 · Rosa“, „offen ab 0:05 · Rosa“).
  - Das ist nur das Aussehen (`ADCDoor::SetPlanLook`, `ShownAmount`, `PlanLabel`). Echter Zustand, NavMesh und
    Regeln bleiben. Beim Abspielen und Ausprobieren zeigen die Türen wieder ihren echten Zustand.
  - Die Simulation führt ein Tür-Protokoll: `FDCSchedule::StartDoors` und `DoorChanges` (`FDCDoorChange`: wer,
    wann, vorher/nachher; Schwenkrichtung in `FDCSimDoor::Swing`, `ADCDoor::SwingSignFor`), dazu `GetDoorAt`.
    Der Menütitel lautet dann z. B. „Hintertür · offen (Rosa öffnet 0:05)“.
  - Selbsttest `-DCSelfTest=PlanLook` (L_House01, Screenshots `plan_look_end.png`, `plan_look_step1.png`).
- **Warten am Schritt, Einfügen, Geist (Schritt 22 = Bauschritte 6–8, vereinfacht):**
  - Wählt man einen Schritt in der Liste, gibt es dort cyan „Rosa wartet hierauf“ (`ToggleWaitOn`). Rosa
    bekommt dann „warten, bis Vic „Bürotür knacken“ erledigt hat“ vor ihren ersten Schritt, der sonst zu früh
    anfinge. Eine Zeichen-Zeile bei Vic gibt es dafür nicht mehr (`Wait.Ref` = Schritt-Id). Ein zweiter Klick
    zeigt „… wartet nicht mehr“. Live sagt Vic „Zeichen an Rosa“. Der Dialog „Warten …“ bleibt wie bisher
    (mit Zeichen-Schritt).
  - Wer einen Schritt löscht, löscht das Warten darauf mit (`RemoveTask` gibt die Anzahl zurück). Die Meldung
    sagt das, und ein „Rückgängig“ holt beides zurück.
  - Einfügen: Ist ein Schritt gewählt, kommen neue Schritte direkt dahinter (`GetInsertIndex`). Das gilt für
    Boden, Tür, Gegenstand, Paare, „Warten …“ und „ablegen“. Der neue Schritt ist danach der gewählte. Eine
    gelbe Zeile zeigt „+ Neue Schritte kommen hierhin“ mit „ans Ende“. Esc hängt wieder ans Ende an, Entf und
    Rücktaste löschen den gewählten Schritt (sonst den letzten). Die Meldung lautet „Rosa, neuer Schritt 2:
    … (nach „gehen (Flur)“)“.
  - Ein Klick auf eine Zeile wählt den Schritt. Der Raum zeigt dann den Moment danach (Türen, Geist), ohne
    Stecknadeln.
  - Geist (`ADCPlanGhost`): Ohne Zeitleisten-Vorschau steht eine abgedunkelte, etwas kleinere Figur der
    gewählten Figur mit hellem Ring dort, wo ihr nächster Schritt beginnt („Rosa danach“, „Vic nach 1“). Hängt
    die Figur vorher, steht der Geist rot an dieser Stelle („Rosa hängt: Hintertür bleibt verschlossen“). Ein
    Klick auf ihn wählt die Figur. Der Geist ist nicht im NavMesh und blockiert nichts. Variante als Stecknadel
    über `UDCPlanSubsystem::bGhostAsPin` (Schalter kommt mit den Varianten).
  - Noch nicht: Menüs und „Hände danach“ rechnen weiter mit dem Stand am Planende statt an der Einfügestelle.
    Schritte mit der Maus ziehen (Bauschritt 9), Lösungen per Klick (10), Vorhersage im Menü (11) und das
    Varianten-Fenster (12) fehlen ebenfalls.
  - Selbsttest `-DCSelfTest=Insert` (L_House01). PlanLook prüft den Geist mit.
- **Varianten zum Vergleichen (Schritt 23 = Bauschritt 12, ohne 9–11):** In der Planungsleiste öffnet der
  Knopf „Varianten …“ ein kleines Fenster mit drei Umschaltern, je mit einem Satz dazu:
  - Geist als Figur oder als Stecknadel (`bGhostAsPin`).
  - Hinweis als Leiste oder als Menü an der Maus (`bHelpAsMenu`; Menü mit „Rückgängig“ und „Später“).
  - Einfügen hinter dem gewählten Schritt oder nur nach „+ danach einfügen“ (`bInsertOnlyWithButton`,
    `InsertAfterStep`, `InsertAtEnd`).
  Die Werte stehen in `UDCPlanSubsystem` und werden in GameUserSettings.ini gespeichert (Abschnitt
  `DerClou.Variants`). Selbsttests nutzen immer die Standardwerte (`-DCVariant=GhostPin,HelpMenu,InsertButton`
  setzt sie) und speichern nichts. Selbsttest `-DCSelfTest=Variants` (L_House01, Screenshots `ghost_figure.png`,
  `ghost_pin.png`).
- **Villa Lindenhof, Bauabschnitt 1 (Schritt 24, 2026-11-08):** Grundstück 60 × 48 m, Remise, Erdgeschoss, Keller.
  Plan + Entscheidungen: `Docs/Villa/Villa-Grundriss.html` (aus `python Scripts/Levels/villa_plan.py`, auch als Artifact),
  `Docs/Plan-Naechste-Schritte.md` (Abschnitt Villa). Daten (einzige Quelle für Zeichnung und Level):
  `Scripts/Levels/villa_layout.py` (Meter, x = Ost, y = Nord; Unreal X = y·100, Y = x·100).
  - Map `/Game/DerClou/Maps/L_Villa01` (jetzt Editor-Startmap), gebaut von `Scripts/Levels/build_villa.py` (Commandlet,
    Editor zu, **absoluter Skriptpfad**, vorher `Content/DerClou/Maps/L_Villa01.umap` löschen). Kopiert L_House01 (Himmel,
    Mond, Nachbearbeitung, Crew, Nachbar, NavMesh), baut Gelände mit Löchern (Haus, Außentreppe), Decke EG mit Treppenloch,
    Wände aus den Raumkanten (außen 30, innen 15 cm, Öffnungen mit Brüstung/Sturz, Glas mit Kollision), 34 `ADCDoor`
    (Typ-Farbe, Breite, Schloss aus den Daten; Garagen-/Einfahrtstor LockDifficulty 10 „elektrisch“; Hebeschiebetür =
    halbe Breite + Festglas), Kellertreppe und Kelleraußentreppe (Stufenblöcke), 29 `ADCRoomVolume`, 39 Gegenstände
    (Tresor + Vitrine im Tresorraum, Brecheisen in der Werkstatt, Rucksack auf dem Gehweg …), Möbel-Kisten, Hecken, Zaun
    mit drei Toren, 18 Bäume, Pool mit unsichtbarer Sperre, 30 Außen- + 13 Innenleuchten (Bewegungsmelder-Strahler aus,
    Tag `DCMotionLight`), Nachbarhaus mit `Nachbar`. Materialien `/Game/DerClou/Villa/Materials/MI_Villa_*`.
  - Crew startet auf dem Gehweg vor der Pforte. Türen mindestens 1,0 m, Tore 2,10 m hoch (sonst stößt die Figur an).
  - **Eine Ebene zeigen:** `UDCFloorSubsystem` (`DCFloors.h`): Hausteile tragen Tag `DCFloor_KG/EG/OG`; alles über der
    gezeigten Ebene wird ausgeblendet (`SetActorHiddenInGame`, Kollision und NavMesh bleiben), Figuren im Haus auf
    höheren Ebenen auch. Folgt der gewählten Figur, Bild ↑/↓ oder die Pfeile oben rechts („Ebene: Keller“) wählen selbst.
    Mausklicks gehen durch Ausgeblendetes (`UDCFloorSubsystem::HitUnderCursor`). Kamera gleitet auf die Ebenenhöhe
    (`ADCCameraPawn::SetFloorHeight`), Zoom bis 55 m.
  - `ADCRoomVolume`: Höhe zählt (ein Stockwerk), verschachtelte Räume → der kleinste gewinnt (Speisekammer in der Küche).
  - Fehler in der Wegsuche behoben: stand die Figur genau am Türstandpunkt (nach dem Knacken), wurde ein Umweg über
    eine andere Tür geplant (`WayThroughDoors`, Null-Meter-Stück).
  - Selbsttest `-DCSelfTest=Villa` (Map L_Villa01): Räume, Türen, Treppen begehbar, Zaun dicht, Vic plant Pforte →
    Haustür → Kellertür → Kellerflur, spielt ab, Kamera folgt in den Keller (Screenshots `villa_plan/keller/eg/overview.png`).
  - **Noch nicht gebaut:** Fenster als Einstieg (Regeln auf der Seite, vom User bestätigt), Bewegungsmelder, Alarm,
    Panzertür mit Fertigkeit Tresore (bisher Schlösser 8), 1. Stock. Modelle aus Blender:
    Schritt 26.
- **Bedienung entrümpelt, Wegenetz-Schleife behoben (Schritt 25, User-Feedback 2026-11-08):**
  - Planungsleiste: „Planen | Ausprobieren“ und „Rückgängig / Wiederholen“ sind weg. Neu ist der Knopf „Letzten Schritt
    löschen“; ist ein Schritt mitten im Plan gewählt, heißt er „Schritt N löschen“ und tut dasselbe wie Entf
    (`DeleteStep`, `DescribeDeleteStep`). Strg+Z geht noch ohne Knopf, „Wiederholen“ gibt es nicht mehr. Den
    Direkt-Modus nutzen nur noch die Selbsttests.
  - Figur wechseln (Karte, Taste 1–9, Klick auf Figur oder Geist) setzt den Zeitcursor ans Ende ihres Plans, ohne
    Plan an den Anfang (`ShowCrewMoment`). Neue Schritte schieben einen aktiven Zeitcursor mit (`FollowNewSteps`).
  - Klick ins Gelände: Das Ziel liegt auf dem begehbaren Boden (`ProjectPointToNavigation`), sonst kommt „Dort kann
    niemand stehen“. Unsichtbares (Zaun- und Pool-Sperren) fängt keine Klicks mehr ab (`UDCFloorSubsystem::HitUnderCursor`).
  - Türen zählen auf dem Weg nur auf ihrem eigenen Stockwerk (`CrossesOnSameFloor`); vorher lief ein Weg durch die
    Halle „durch“ die Panzertür im Keller darunter.
  - **Ursache der Wegfehler:** Weg-Markierungen, Geist, Klick-Ring und Lärm-Ringe meldeten sich beim NavMesh an. Jedes
    Neuzeichnen baute das NavMesh neu, danach rechnete die Planung neu und zeichnete wieder neu. Auf der Villa war das
    NavMesh bis zu 20 s nach dem Laden nicht fertig; daher kamen Linien quer durch den Zaun und „kommt nicht hin“
    beim Abspielen. Jetzt geben rein optische Actors `IsComponentRelevantForNavigation() = false` zurück, und das
    NavMesh der Villa ist nach etwa 0,5 s fertig. Neue optische Actors bitte genauso.
  - Abspielen wartet, bis das NavMesh fertig ist (`UDCHeistStateSubsystem::IsNavigationReady`, Meldung „Wege werden
    noch berechnet …“).
  - Selbsttest Villa erweitert: Rosa wartet auf Vics Schritt 1 und geht durch die Pforte in den Garten; Zeitsprung
    beim Wechseln; Löschen-Knopf; Türen pro Stockwerk; zweiter Durchgang nach dem Neuladen spielt sofort ab.
- **Villa-Modelle aus Blender (Schritt 26, 2026-11-08):** alle Objekte von Bauabschnitt 1 modelliert – 83 Objekte in
  17 Gruppen, je ein Agent mit höchstens 5 Objekten. Auftragsblatt, Maße, Konventionen und Stand:
  `Docs/Villa/Villa-Modelle.md` (dort auch die Katalog-Vorschläge der Agenten).
  - Pipeline: je Gruppe ein Modul `Art/Items/dc_villa_<gruppe>.py` (`dc_items` importiert alle automatisch, ein kaputtes
    Modul stoppt die anderen nicht). `build_items.py`: `--sheet <name>` (eigener Kontaktbogen je paralleler Lauf),
    `--ref` (graue 1,80-m-Figur), jede Vorschau zusätzlich steil wie die Spielkamera (`<id>_top.png`). Neue Materialien
    im Villa-Block von `dc_materials.py` (Außenmetall, Laub, Rinde, Beton, Milchglas, LEDs, `Steel_Satin` …).
  - `ADCDoor` zeigt Modelle: `LeafModel` (Standard `Door_inner`), `FrameModel` (`Door_frame` → `SM_Door_frame_post/_top`),
    `bShowFrame`, `bDoubleLeaf` (zweiter Flügel `Hinge2/Panel2`), `bMirrorLeaf` (Außenseite der Modelle ist −X),
    `Motion` Swing/Slide/TiltUp, `FrameDepth` (Wanddicke + 2), `SlideOut`. Die Modelle werden über ihre Bounding Box auf
    Breite × Höhe gestreckt (`DoorFitToBox`); fehlt ein Modell, bleibt der farbige Würfel. Gilt auch für L_Room01/L_House01.
  - `build_villa.py`, Abschnitt „Blender models“: `model()`, `place_model` (Anker center/back/origin), `place_row` (Module
    reihen, jedes zweite gespiegelt), Tabellen `DECO_MODELS`, `EXTRA_MODELS`, `DOOR_LOOK`, `WINDOW_MODEL`, `TREE_MODEL`,
    `WALL_MOUNT`. Jedes Modell ist optional (nicht importiert → alte Kiste). Treppen, Bäume, Laternen, Poller: Modell ohne
    Kollision, die alten Kisten/Zylinder bleiben unsichtbar als Hindernis. Teile an der Hausfassade tragen `DCFloor_EG`.
  - Ablauf nach Modelländerungen (Editor zu): `ue_import_items.py`, dann L_Villa01 neu bauen, dann `-DCSelfTest=Villa`.
  - Erkenntnis: Die Pfortenlaterne steht auf dem Gehweg; mit der Box-Kollision ihres Modells blieb Vic auf der Straße
    stehen. Lampen deshalb ohne eigene Kollision.
  - Offen: Katalogeinträge für `grand_piano` (im Level steht noch `piano`), neue Beute (`gold_bar`, `wine_bottle`,
    `silverware_case`, `watch_box`, `flour_tin`) und Werkzeug (`bolt_cutter`, `ladder`, `cordless_drill`, `key_ring`,
    `garage_remote`) – Werte mit dem User besprechen; Zustände sichtbar machen (Tresortür offen …); Möbel 1. Stock.
- **„Dort kann niemand stehen“ im Editor-Spiel behoben (Schritt 27, User-Bug 2026-11-08):** Nach „Rosa wartet auf Vic“ bzw.
  „Plan leeren“ ging kein Bodenklick mehr. Ursache: PIE kopiert das NavMesh des Editors statt ein eigenes zu bauen. Der
  Editor rechnet die Villa nach dem Laden neu (im Hintergrund über 30 s). Wer vorher Play drückt, hat im Spiel ein halbes
  oder leeres Wegenetz, und zwar für immer. Nachgestellt mit `Scripts/Tests/pie_nav_probe.py` (Editor starten, sofort Play,
  Punkte projizieren, beendet sich selbst): vorher 0/7 Punkte auf dem NavMesh, jetzt 7/7. Jetzt baut die PIE-Welt ihr NavMesh beim Start selbst
  (`UDCHeistStateSubsystem::OnWorldBeginPlay`, `bRebuildingAtStart`, ~1,5 s), `IsNavigationReady` wartet darauf. Log:
  „Navigation: play in editor builds its own nav mesh“ / „nav mesh built … s after start“; ein misslungener Bodenklick
  schreibt „No ground: click at … on …“. Klicks gehen über `HandleClickAt(Bildschirmpunkt)`, Selbsttests klicken mit
  `ClickWorldPoint(Weltpunkt)` echt (Trace, Bodencheck). Selbsttest Villa klickt jetzt den User-Fall durch.
- **Startmenü, Ladebildschirm, Spielstände, Optionen (Schritt 28, 2026-11-08):** Vom User entschieden: ein Spielstand ist der
  Plan je Einbruch (kein laufender Einbruch), „Neues Spiel“ wählt den Einbruch, hinter dem Menü die Villa bei Nacht,
  Arbeitstitel „Lindenhof“ (`UDCGameFlowSubsystem::GetGameTitle`, nie „der clou“).
  - `UDCGameFlowSubsystem` (`DCGameFlow.h`, GameInstance): Einbrüche (`GetHeists`: Villa Lindenhof; Übung: Testhaus, Testraum),
    `StartHeist`, `LoadSave`, `OpenMainMenu`, `ReloadCurrentMap`, `QuitGame`. Das Startmenü ist ein Modus der Villa-Map
    (Option `?menu`): `ADCGameMode::InitGame` fragt `DecideMenu` und setzt dann `ADCMenuPlayerController` (Kamera fährt langsam
    die Straße entlang, Crew/Nachbar/Raumnamen ausgeblendet, ganze Fläche ohne Rahmen). Die erste Map einer Sitzung zeigt das
    Menü (`GameDefaultMap` bleibt L_Room01 für die Selbsttests und leitet weiter; PIE auf der Villa zeigt es direkt).
  - Ladebildschirm `SDCLoadingScreen` (Slate direkt im GameViewport, überlebt den Kartenwechsel): erst ein paar Frames zeichnen,
    dann `OpenLevel`; nach dem Laden bleibt er, bis die Welt spielt, das NavMesh fertig ist, nichts mehr asynchron lädt und
    (Editor) keine Shader mehr kompilieren – mind. 1 s, höchstens 90 s, dann 0,35 s Ausblenden. Zeigt Einbruch, Arbeitsschritt
    („Berechne Wege …“), Balken, Tipp. Solange er steht, nimmt das Spiel keine Eingaben (`IsLoading`). Auch „Abspielen“ nach
    Änderungen und „Zurück zur Planung“ laden dahinter neu.
  - Spielstände (`UDCSaveGame`, Saved/SaveGames): Autosave `auto_<einbruch>` 1 s nach jeder Planänderung (nur wenn sich Schritte
    geändert haben, Signatur aus Schritt-Ids), eigene `save_<zeit>` mit Namen. „Fortsetzen“ = neuester Stand. `UDCPlanSubsystem::
    ResetForNewHeist` / `SetPlans` (ohne Rückgängig-Verlauf). Im Menü ist kein Plan geladen.
  - `UDCMenuWidget` (Startmenü und Pausemenü): Spalte links (Titel, Hauptknöpfe), Seite rechts (Einbruch wählen mit „Neu beginnen /
    Plan fortsetzen“, Laden mit „Löschen“ → „Wirklich löschen?“, Plan speichern mit Namen, Optionen). Pausemenü über Esc (Ende der
    Esc-Kette) oder Knopf „Menü“ oben rechts; hält einen laufenden Plan an.
  - Optionen (`DCOptions`): Qualität, Fenster/Randlos/Vollbild, Auflösung, Kameratempo (`CameraSpeedFactor`), die drei Varianten
    (das Fenster „Varianten …“ in der Planungsleiste ist weg). „Übernehmen“ speichert in GameUserSettings.ini, Selbsttests nie.
  - Selbsttests ohne Menü, Ladebildschirm und Autosave – außer `-DCSelfTest=Menu` (ohne Map starten): Start → Menü → Testhaus →
    Autosave → Pausemenü → „Test-Stand“ speichern → Hauptmenü → Fortsetzen/Laden/Optionen → Stand laden → Neuladen. Eigene Slots
    `DCSelfTest_*`, die er wieder löscht. Screenshots `menu_loading/main/heists/continue/load/options/heists_saved.png`,
    `pause_menu.png`, `pause_save.png`.
- **Professioneller Look, Schritt 1 von 6 (Schritt 29, 2026-11-08):** Vorlage ist ein vom User mit einer Bild-KI erzeugtes
  Mockup, `Docs/UI-Mockup/planung_v1.webp` („noir dossier meets tactical HUD“). Regeln stehen in `Docs/UI-Leitfaden.md`
  (Abschnitte 3–5).
  - Schriften Barlow / Barlow Condensed (OFL) in `Content/Slate/Fonts`, zur Laufzeit als Dateien geladen
    (`DCItemWidgets::Font`, neu `HeadFont`). Ein Import als Font-Asset stürzt im Commandlet ab (braucht eine Slate-App).
    `Config/DefaultEditorPerProjectUserSettings.ini` hält den Auto-Import von `Content/Slate` fern, `DefaultGame.ini` packt
    den Ordner mit.
  - Palette aus dem Mockup als lineare Werte, Hex-Wert im Kommentar. Neu: `BackdropColor`, `BrassColor`/`BrassDimColor`,
    `TileColor`, `OkColor`. Bernstein (`TitleColor`) gilt nur für Auswahl und Hauptknopf, Titel sind warmweiß.
  - Bausteine: abgerundete Flächen `RoundBrush`/`FramedBrush` (Messinglinie)/`SoftFramedBrush`/`FocusFramedBrush` usw.
    Die Randlinie behält ihre Farbe, nur die Füllung wird eingefärbt. Dazu Knopfstile `NormalButtonStyle`/`DangerButtonStyle`/
    `FlatButtonStyle` und die Bausteine `MakeTitle`, `MakeSectionTitle`, `MakeRule`, `MakeSegments`.
  - Umgestellt sind Rahmen (Modus in Großbuchstaben, Trenner, Linie unter der Kopfzeile), Profilkarten (Farbstreifen, Abzeichen,
    Bernstein-Rahmen), Profil (Segment-Umschalter, „FÄHIGKEITEN“/„AUSRÜSTUNG“), Radialmenü (Pillen mit Messingrand),
    Status oben (Misstrauen mit 5 Segmenten), Info, Durchsuchen/Fund, Warten-Dialog, Planungsleiste (Meldung mit Farbbalken,
    „▶ Abspielen“, „Plan geht auf“ grün, gerahmte Schrittzeilen, runde Blöcke in der Zeitleiste).
    Die Menüs aus Schritt 28 übernehmen den Stil automatisch.
  - Geprüft mit den Selbsttests Villa, Signals, PlanHelp, Find, Timeline, Menu und PlanView (alle bestanden, Screenshots angesehen).
  - Als Nächstes (mit dem User vereinbart): 2 Icons (Lucide-SVG), 3 Porträts (der User erzeugt sie mit der Bild-KI),
    4 Gegenstands-Icons aus Blender, 5 Feinschliff der Zeitleiste (schraffiertes Warten, Zeitmarke), 6 Punktlinien und Schilder
    in der Welt.
- **Professioneller Look, Schritt 2 von 6: Icons (Schritt 30, 2026-11-08).** 33 Linien-Icons von Lucide 1.70.0 (ISC) als SVG in
  `Content/Slate/Icons` (Strich weiß statt `currentColor`, weil nanosvg das nicht kennt), zur Laufzeit geladen wie die Schriften.
  - `DCItemWidgets::Icon(Name)` (ein `FSlateVectorImageBrush` je Datei, Slate rasterisiert ihn in der gezeichneten Größe),
    `MakeIcon`, `MakeIconText`, `IconSizeFor`, `IconForTask` (Schritt-Art), `IconForSkill`. Die Zuordnung steht als Tabelle in
    `Docs/UI-Leitfaden.md` (Abschnitt 5).
  - `FDCRadialOption::Icon`: Radialmenü mit Icon vor jeder Aktion (`MakeTaskOption` nimmt `IconForTask`, Info/Durchsuchen/
    Rückgängig/Später von Hand). Dabei behoben: die seitlichen Einträge verdeckten den Titel in der Mitte (Abstand jetzt
    mindestens halbe Titel- plus halbe Knopfbreite), der Titel ist zweizeilig (Objekt groß, „plant: Rosa“ klein darunter).
    `UDCRadialMenu::bOpenAtCentreForTests` öffnet das Menü im Test in der Bildmitte (sonst am Mauszeiger außerhalb des
    Fensters); der Timeline-Test zeigt es so in `radial_safe_rosa/vic.png`.
  - Planungsleiste: `PanelButton(…, IconName)` (auch wechselnd: Pause/Weiter/Fund), Icons bei Löschen, Abspielen, Lösen,
    Zurück, Warten …, ablegen, Rückgängig. Status mit Häkchen bzw. Warnzeichen, Meldungen mit Fadenkreuz bzw. Warnzeichen,
    jede Schrittzeile mit dem Icon ihrer Art.
  - Profilkarten: Plan-Stand direkt unter dem Namen (bei Problem rot mit Warnzeichen), dann zwei Fähigkeiten als Zeilen mit
    Icon und Wert (wie im Mockup). Profil: Icon vor jeder Fähigkeit.
  - Oben: Ohr vor „Nachbar: Misstrauen“ (färbt sich mit), Sirene im Alarm, Lautsprecher vor dem Lärm; Ebene mit Ebenen-Icon
    und Pfeilknöpfen, „Menü“ mit Icon. Info und Durchsuchen/Fund mit Icon im Titel und x zum Schließen, Warten-Dialog mit
    Sanduhr.
- **Professioneller Look, Schritt 3 von 6: Porträts (Schritt 31, 2026-11-09)** – Vic und Rosa sind drin (vom User mit der
  Bild-KI erzeugt, Originale `Art/Portraits/vic.webp`, `rosa.webp`, 1086 × 1448; Kurt fehlt noch):
  - Prompts für die Bild-KI (Stil-Block + je Figur, Vic zuerst, dann als Referenzbild für Rosa/Kurt):
    `Docs/UI-Mockup/Portraet-Prompts.md`. Das Spiel spielt heute, also zeitlose Kleidung statt der 1920er aus dem Mockup.
  - Ablauf: Originale nach `Art/Portraits/<name>.png|jpg|webp`, dann `python Scripts/Tools/prepare_portraits.py`
    (3:4, dann 75 % Ausschnitt um das Gesicht – `ZOOM`, `FOCUS_X`, `TOP` oben im Skript –, 132 × 176)
    → `Content/Slate/Portraits/<name>.png`. Geladen zur Laufzeit wie Icons und Schriften.
  - `DCItemWidgets::Portrait(CrewName)` (`FSlateDynamicImageBrush` – ein normaler `FSlateImageBrush` aus einer Datei wird
    nur in registrierten Styles geladen und bleibt sonst weiß; schneidet andere Seitenverhältnisse per UV auf 3:4),
    `MakePortrait(Name, Farbe, Breite)` (Messingrahmen; ohne Bild Initiale in der Crew-Farbe auf dunklem Grund).
  - Profilkarte: Porträt 66 × 88 statt des 52er-Quadrats. Profil rechts: Porträt 60 × 80 links, daneben Titel, darunter
    Umschalter „Jetzt | Nach dem Plan“, darunter die Zusammenfassung (sonst wurde „PROFIL – ROSA“ gekürzt).
  - Erst mit Platzhalterbildern, dann mit den echten Bildern geprüft (Villa, Signals, PlanHelp bestanden, Screenshots
    `crew_profile.png`, `villa_plan.png`, `planhelp_bar.png`).
- **Editor-Hänger beim Abspielen behoben (Schritt 32, User-Bug 2026-11-09):** Villa, Vic knackt Pforte und Haustür,
  Rosa wartet davor. Sobald Vic die Haustür aufsperrte, stand das Spiel (Task-Manager nötig). Davor gab es schon Ruckler
  von 19 s und 10 s an der Gartenpforte.
  - Ursache: `UNavigationSystemV1::FindPathToLocationSynchronously` hält jeden Weg bis zur nächsten Garbage Collection
    am Leben, und jeder Weg ist beim NavMesh als „aktiv, bei Änderung neu berechnen“ angemeldet. Die wartende Rosa fragte
    jeden Frame ~70 Wege ab (`DoorOnWay` → `WayThroughDoors`). Nach 25 s waren es ~100 000 Wege. Jede Türänderung baut
    NavMesh-Kacheln neu, und `ARecastNavMesh::InvalidateAffectedPaths` will dann alle betroffenen Wege neu planen
    (`RepathRequests.AddUnique`, quadratisch) – minutenlang.
  - Jetzt: `DCNav::FindPath` (DCHeistState.h) gibt nur Punkte zurück, das Wegobjekt ist danach weg. **Nie mehr
    `FindPathToLocationSynchronously` verwenden.** Wartende Figuren prüfen den Weg nur bei einer Türänderung
    (`ADCDoor::GetStateVersion`) oder alle 0,5 s neu. Aufsperren allein baut das NavMesh nicht mehr um.
  - Wachhund (`DCWatchdog`): Läuft eine Spielwelt und kommt 5 s lang kein neuer Frame, schreibt ein eigener Thread die
    Aufrufkette des Game-Threads ins Log und nach `Saved/Logs/DerClou3-Haenger.txt` (auch nach 20 s und 60 s). Die Datei
    gibt es auch dann, wenn der Editor abgeschossen wird. Engine-Funktionen ohne PDB lassen sich mit `dumpbin /exports`
    + Modul-Basisadresse grob zuordnen. Abschalten mit `-DCNoWatchdog`.
  - Selbsttest `-DCSelfTest=Replay [-DCSave=auto_villa]` (Map L_Villa01): lädt einen Spielstand, spielt ihn ab und meldet
    langsame Frames (> 0,5 s). Damit lässt sich ein User-Plan nachspielen. `WayThroughDoors` loggt Aufrufe über 50 ms.
  - Dabei gefunden: Rosa durchsuchte das Wohnzimmersofa aus dem Keller darunter und kam danach nicht mehr weiter. Grund:
    `ProjectPointToNavigation` wählt den von oben gesehen nächsten Punkt (`findNearestPoly2D`); mit ±3 m Höhe gewann der
    Kellerboden unter der Sofamitte. Jetzt `DCNav::ProjectOnStorey` (2,4 m nach unten, 0,4 m nach oben) für Ziele,
    Stehplätze und Wegprüfung. Die Stehplatzsuche am Möbel probiert mehr Richtungen und einen zweiten Abstand.
  - Ebenfalls: Slate-Ensure „LineModel.Text->Len() == InNewText.Len()“ bei Großschrift mit „ß“ („Straße“). Jetzt
    `DCItemWidgets::Upper` statt `TransformPolicy(ToUpper)`.
  - Der User-Plan lief danach bis auf Rosas letzten Schritt durch (44,0 s gespielt, 43,7 s geplant, längster Frame
    0,03 s); der Rest war die Geheimtür, behoben in Schritt 36.
- **Professioneller Look, Schritt 4 von 6: Gegenstands-Icons aus Blender (Schritt 33, 2026-11-09).** 34 Bilder, eines je
  Katalog-Gegenstand (nur die Schublade hat keins), in `Content/Slate/ItemIcons/<id>.png`, zur Laufzeit geladen.
  - Blender: `build_items.py --icons` rendert orthografisch schräg von oben (`VIEW_ICON`, Ausnahmen in `ICON_VIEWS`:
    Gemälde von vorn, Dietrich-Set steiler), transparent, AgX, warmes Führungslicht, kühles Gegenlicht, nach
    `Saved/ItemIcons/raw`, Kontaktbogen `Saved/ItemPreviews/_sheet_icons.png`. Nicht zusammen mit `--preview`.
  - `Scripts/Tools/prepare_item_icons.py` liest die Rastergrößen aus `DCItemTypes.cpp` (eine Quelle), schneidet auf die
    sichtbaren Pixel, dreht nur deutlich längliche Dinge (`TURN_RATIO` 1,6 – Brecheisen ja, Mantel nicht) und setzt sie
    mit 8 % Rand auf 64 px je Feld (ohne Raster 128 × 128). `--ids` gibt die Ids für `--items` aus.
  - Code: `DCItemWidgets::ItemIcon(DefId)`, `MakeItemIcon(DefId, MaxBreite, MaxHöhe)` (`SScaleBox`, Seitenverhältnis bleibt).
    Rasterkachel mit Bild auf neutralem Grund, Kurzname nur ab 2 × 2 Feldern; Kopfzeilen (`MakeHeader`) mit Bild
    22 × 20, Hände 34 × 30, Zieh-Anzeige 44 × 44. Ohne Bild bleibt die alte Kachel.
  - Geprüft mit Grid, Find, Containers, Signals, Inventory und Villa (alle bestanden, Screenshots angesehen).
- **Rosa als 3D-Figur mit Skelett und Animationen (Schritt 34, 2026-11-09, nur Blender, noch nicht im Spiel).** Vorlage:
  Referenzblatt des Users (T-Pose, vorn/hinten/beide Seiten/zwei Schrägansichten) und Kopf-Nahaufnahme (vorn + Profil
  nebeneinander, `rosa_head.png`) in `Art/Characters/Rosa/ref/`.
  - `blender -b --factory-startup -P Art/Characters/build_character.py -- --char Rosa --stage mesh|rig|anim|all [--preview]
    [--export]` (bis Schritt 37 hieß es `build_rosa.py`). Alles, was nur Rosa betrifft (Bilddateien, Messpunkte, Höhen,
    Ohrumriss, Locken, Dutt), steht in `Art/Characters/Rosa/character.py`; Bilder für neue Figuren nach
    `Docs/Figuren-Referenzbilder.md`,
    `--stage showcase` rendert `Saved/Characters/Rosa/rosa_showcase.mp4` und `sheet_game.png` (Spielkamera-Abstand).
    Zwischenstände `Saved/Characters/Rosa/rosa_<stage>.blend`, fertig `Art/Characters/Rosa/rosa.blend`.
  - Form: Umriss vorn (x–z) und Seite (y–z) → je Höhe eine Superellipse in beide Umrisse eingeschrieben (Voxel 3 mm),
    Arme in T-Pose quer geschnitten (rund), Voxel-Remesh, glätten, Körper ~24 000 + Kopf ~9 000 Dreiecke (`decimate_zones`).
    Locken: im Kopfbereich wird der Umriss geöffnet (≈1,7 cm, am Kiefer ≈3,7 cm) und pro Zeile nur der breiteste
    Abschnitt behalten – sonst werden Strähnen zu dünnen Scheiben.
  - Kopf ab 1,45 m (`Z_HEAD`) aus der Nahaufnahme (`HeadView`, 0,38 mm/px): eingepasst über Augen- und Kinnhöhe des
    Ganzkörperfotos (Haaransatz landet dann bei 1,70 m), Profil über die Gesichtskante Kinn–Stirn (Median). Gesicht
    (`FaceModel`): Mittellinie aus dem Profil (Stirn, Nase, Lippen, Kinn), seitlich Superellipse bis zur Ebene vor den
    Ohren, Augenhöhlen; die Haare neben den Wangen werden bis dahin zurückgeschnitten, zur Schläfe hin weich mehr erlaubt.
    Messpunkte als Konstanten `HEAD_FRONT`/`HEAD_SIDE`/`BODY_EYE_ROW` (Pixel in den Bildern).
  - Hände modelliert (`hand_layout`, `hand_object`): Handfläche, Ballen, 4 Finger à 3 Glieder (leicht flach, Gelenkwülste,
    Nagelplatten), Daumen, auf die gemessene Handlänge skaliert, je ~5 000 Dreiecke, als eigene Inseln in den Ärmel
    gesteckt. Farbe = Hautton der Wangen (Attribut `skin`), danach malt `paint_hand_details` Nägel (Halbmond, Nagelhaut,
    heller Rand), Falten über den Gelenken, Beugefalten, Handlinien (`PALM_LINES`), rötliche Knöchel und Kuppen direkt in
    die Textur (`texels_of` rastert die UV-Dreiecke). Gewichte nach Abstand zu Hand-/Fingerknochen (`hand_weights`).
  - Ohren (`EarModel`, `ear_object`): Umriss im Profil nachgezogen (`EAR_OUTLINE`, Muschel `EAR_CONCHA`), gewölbte Schale
    mit Rand, Gegenleiste und Mulde, 30° vom Kopf geneigt, Solidify + Subsurf. Hinter dem Ohr läuft das Haar flach auf
    Schädelbreite aus (`clearance`, nach hinten 1,8-mal länger auslaufend), damit es frei steht.
  - Haare: `hair_relief` drückt die Haarfläche nach der Helligkeit der Fotos ein und aus (Hochpass, ±2,8 mm) und gibt
    ihr leichte Beulen; 14 lose Korkenzieherlocken (`CURLS`, `curl_object`) an Schläfen, Stirn, Nacken und Dutt,
    Farbe dunkelbraun mit Kupfer (Attribut `curl`). Ohren und Locken hängen fest am Kopfknochen (`rigid_weights`).
  - Hinterkopf und Oberseite ohne eigenes Foto (der User hat sich gegen eine weitere Ansicht entschieden):
    - Form: Der Umriss zwischen Scheitel und Dutt wird über den Winkel um die Kopfmitte geglättet
      (`round_back_outline`; vorher eine Stufe von 4 cm, wo die Strähnen im Profil enden). Neben dem Dutt wird der
      Hinterkopf bis 3,5 cm eingezogen, der Dutt sitzt als eigenes Ellipsoid darauf und reicht hinten bis zum Profil
      (`BUN`: Mitte, Halbachsen, Einzug).
    - Strähnen (`HairDetail`): ein reines Haarstück aus dem Profil der Nahaufnahme (`HAIR_SRC`, 12,7 × 8,2 cm) wird in
      runden Flecken auf den Kopf gestempelt (Raster 1,6 cm mit Zufallsversatz, Mischung mit erhaltenem Kontrast). Jeder
      Fleck ist so gedreht, dass die Strähnen zum Dutt laufen und auf ihm in zwei Spiralen umlaufen. Übernommen wird nur das
      Detail (Verhältnis zum Mittel über 6 mm). Die Farbe im Zentimeterbereich bleibt die der Fotos (`HAIR_BASE_CELL`).
    - Dasselbe Feld treibt das Relief (Band 3,5–10 mm) und die Textur (`paint_hair`, nur wo die Fotos schlecht hinsehen:
      Normale nach hinten oder oben, `coverage`; Haut wird ausgespart). Der Dutt bekommt Spiralwülste (±6,5 mm), deren
      Rillen in der Textur dunkler sind.
  - Bone Heat scheitert am ganzen Netz, sobald Ohren und Locken dabei sind; `body_heat_weights` gewichtet deshalb eine
    Kopie nur mit dem Körper und kopiert über den Vertex-Index zurück.
  - Farbe: die Fotos werden je nach Flächennormale auf die Form projiziert und in `T_Rosa_BC.png` (4096², Kopf mit
    1,7-fachem UV-Platz) gebacken. Gesicht nur vom Frontfoto, Wangen/Ohren/Haar auch vom Profil, Arme nur vorn/hinten.
  - Skelett mit UE-Mannequin-Namen, 53 Knochen (root, pelvis, spine_01–03, neck_01, head, clavicle/upperarm/lowerarm/
    hand, thumb/index/middle/ring/pinky_01–03, thigh/calf/foot/ball je _l/_r), Gelenke aus den Umrissen gemessen,
    Körpergewichte automatisch (Bone Heat). Fingerhaltungen `RELAXED`, `LOOSE_FIST`, `GRIP`, `OPEN`, `GRAB`.
  - Animationen (30 fps, auf der Stelle, Schleifen enden auf dem ersten Bild): `Rosa_Idle` (3 s), `Rosa_Walk` (1,4 m/s),
    `Rosa_Jog` (2,5 m/s = `BaseWalkSpeed`), `Rosa_Work` (hockt, Hände auf ~0,95 m – Schloss/Schublade, Schleife),
    `Rosa_Pickup` (1,6 s, greift vor sich auf den Boden). Beine über Zwei-Knochen-IK (Füße stehen, rollen über Ferse und
    Ballen), Arme bei Arbeiten/Aufheben ebenfalls IK (`Poser.two_bone`, `arm_ik`).
  - Export `Art/Characters/Rosa/Export/SK_Rosa.fbx` (Armature heißt „Armature“, damit Unreal `root` als Wurzel nimmt,
    `FBX_SCALE_ALL`, keine Leaf Bones, alle Aktionen als Takes) + `T_Rosa_BC.png`. Gegenprobe `check_fbx()` liest sie wieder ein.
  - ~65 000 Dreiecke (Körper 24k, Kopf 20k, Hände 10k, Ohren 4,6k, Locken 3k). Grenzen: Die Strähnen hinten sind aus dem
    Profil übertragen, nicht fotografiert. Um das Ohr liegt etwas Hautfarbe auf dem Haar. Schräg von hinten sitzt das Ohr
    in einer Mulde im Haar.
  - Im Spiel seit Schritt 37. Offen: Vic und Kurt genauso.
- **Professioneller Look, Schritt 5 von 6: Zeitleiste wie im Mockup (Schritt 35, 2026-11-09).** Nur `UDCTimelinePanel::RebuildTracks`
  und eine Zeichenhilfe, keine Spielregeln:
  - `DCItemWidgets::MakePainter(Funktion)` (Widget, das nur mit `FSlateDrawElement` zeichnet, ohne Klicks), `PaintHatch` /
    `MakeHatch(Grund, Streifen)` (schräge Streifen, an der Bildschirmposition ausgerichtet, damit Nachbarflächen weiterlaufen).
  - Lineal als Maßband (Strich je Sekunde, Zeit alle 5 s bzw. 10/15/30 s bei langen Plänen), links Farbbalken + Name statt
    „> VIC“, Spuren schwach schraffiert (nichts geplant), Warten grau schraffiert mit „wartet“ auf einer Lücke, Blöcke mit
    fetter Nummer und Schrittname, „geht nicht“ breiter. Abstimmung als Pfeil in Cyan vom Rand des auslösenden Schritts
    (Punkt in dessen Crew-Farbe) zum Anfang des wartenden (`RowPitch` 32 für Platz). Zeitmarke mit Reiter „0:21“
    (`GetMarkerTime`; Bernstein = Vorschau, Weiß = Abspielen).
  - Geprüft mit Signals, Timeline, PlanHelp, AutoDoor (bestanden, Screenshots angesehen).
- **Geheimtür in der Villa begehbar (Schritt 36, 2026-11-09, gefunden mit dem Replay aus Schritt 32):** Rosa öffnete das
  Bücherregal, kam aber nicht in die Geheimkammer, und „durch Geheimtür“ zählte trotzdem als erledigt.
  - Ursache 1, Durchgang zu eng: Das NavMesh rechnet mit 35 cm Figurradius (auf dem 10-cm-Raster 40 cm), eine 1,0-m-Tür
    lässt also nur 20 cm Gang. Das Deko-Regal daneben ragte 5 cm in die Öffnung, danach gab es keine Verbindung mehr.
    Jetzt ist die Geheimtür 1,2 m breit, das Scharnier sitzt im Norden (das offene Regal steht nicht mehr vor dem Gemälde
    an der Südwand), das Deko-Regal steht neben der Öffnung (`villa_layout.py`, `build_villa.py`, L_Villa01 neu gebaut).
  - Ursache 2, Stehplatz hinter der Wand: Beim Gemälde an der Außenwand lag der nächste begehbare Punkt draußen vor dem
    Haus, Rosa lief deshalb zur Haustür. Jetzt `DCNav::FindStandSpot`: Stehplatz im Raum des Gegenstands, auf seinem
    Stockwerk, rundherum gesucht. Planung, Abspielen (`WalkToPickup`) und Wegprüfung (`DoorOnWay`) nehmen denselben Punkt.
  - „Durchgehen“ zählt nur, wenn die Figur danach auf der anderen Seite steht (`PassStart`; sonst bis zu 3 Versuche,
    dann „gibt auf“).
  - Selbsttest `-DCSelfTest=SecretDoor` (L_Villa01): Rosa steht in der Bibliothek, geht durch die Geheimtür und hebt das
    Gemälde auf. Er prüft das NavMesh an der Tür vor und nach dem Öffnen und macht Bilder mit `show Navigation`
    (`secret_door_closed/open.png`). Replay des User-Plans: 17 von 17 Schritten, 41,2 s gespielt (43,7 s geplant).
- **Rosa im Spiel (Schritt 37, 2026-11-09):** Rosa läuft mit ihrem Modell aus Schritt 34 statt als grauer Zylinder.
  - Import (Editor zu, absoluter Pfad, Name als Argument): `UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript
    -script="C:/Dev/der-clou-3/Art/Characters/ue_import_character.py Rosa" -unattended` → `/Game/DerClou/Characters/Rosa/`
    `SK_Rosa`, `SK_Rosa_Skeleton`, `A_Rosa_Idle/Walk/Jog/Work/Pickup`, `T_Rosa_BC`, `MI_Rosa`, gemeinsamer Master
    `/Game/DerClou/Characters/M_Character` (Textur, Rauheit 0,78, für Skeletal Meshes). Ordner steht in
    `DirectoriesToAlwaysCook`. Der Commandlet bleibt oft nach „Python script executed“ hängen, dann beenden.
    Fallen: Materialslots einer SkeletalMesh als neue `SkeletalMaterial` setzen (Elemente der gelesenen Liste sind
    Kopien), sonst bleibt `WorldGridMaterial`. Das Skelett erst mit `save_directory` gespeichert.
  - `DCVisuals::CrewModel(Name)` findet Modell und Animationen nach diesen Namen. Gibt es eins, zeigt `ADCCrewCharacter`
    (`ApplyModel`, in `OnConstruction` und `BeginPlay`) es statt Körper/Kopf/Nase. Fuß auf dem Kapselboden, um −90°
    gedreht, keine Kollision. Figuren ohne Modell (Vic) bleiben grau. Der Geist der Planung bleibt die abstrakte Figur.
  - `UDCCrewAnimInstance` (`DCCrewAnim.h`) ohne Anim Blueprint: ein eigener `FAnimInstanceProxy` tastet die Sequenzen
    ab und mischt sie (wie `UAnimSingleNodeInstance`). Stehen ↔ Gehen ↔ Laufen nach Tempo (Gehen für 1,4 m/s, Laufen für
    2,5 m/s gemacht). Der Schrittzyklus läuft mit der zurückgelegten Strecke, also rutschen die Füße nicht, auch mit Last.
    Darüber die Tätigkeit (`ADCCrewCharacter::GetAnimPose`):
    - Arbeiten (hocken): Schloss knacken, Gegenstand knacken/aufbrechen/einschlagen, Durchsuchen im Plan
      (`UDCPlanRunnerComponent::IsWorkingAtContainer`).
    - Aufheben: einmal in 1 s.
    - Einblenden 0,35 s, Aufheben 0,12 s.
  - Selbsttest `-DCSelfTest=CrewLook` (L_Room01): Rosa von nah im Stehen, Laufen, Arbeiten, Aufheben und von oben.
    Prüft Modell, Skelett (53 Knochen, Wurzel `root`), hängende Arme, Armschwung, Hocken, Bücken, Aufstehen. Bilder
    `crew_idle/walk/work/pickup/top.png`. Testschalter `ADCCrewCharacter::bTestAnimPose`/`TestAnimPose`.
  - Getragenes hängt noch fest an der Kapsel, nicht an den Händen (Ausrichtung der Handknochen erst klären).
- **Kamera folgt, Fußspuren und Schilder statt Linien (Schritt 38, 2026-11-09 = Bauschritte 1–2 aus
  `Docs/Konzept-Wege-Durchsuchen-Gepaeck.md`, vom User bestätigt):**
  - Kamera (`ADCCameraPawn::SetFollowPoint`, `ResumeFollow`, `IsFollowing`): folgt immer der gewählten Figur. Planung: ihrem
    Plan-Stand (`UDCHeistStateSubsystem::GetPlanSpot` = Vorschauzeit oder Geist, `FindGhostSpot`), erst wenn der Punkt die
    ruhige Mitte (0,2 × Armlänge) verlässt. Abspielen/Ausprobieren: der Figur selbst. Figur wechseln und „Abspielen“ holen
    die Kamera zurück; WASD oder `FocusOn` lösen sie, oben rechts steht dann „Kamera frei · F folgt“ (Knopf, Taste F).
  - Fußspuren (`ADCPlanTrail`, ein Actor für alle): `SM_Plan_footprint` (Blender, `Art/Items/dc_items_plan.py`, 26 cm,
    Zehen nach Unreal +Y) als Instanzen je Stockwerk, alle 56 cm abwechselnd links/rechts (gespiegelt) in Gehrichtung, auf
    den Boden gesetzt (Trace auf WorldStatic), 1,6-fach vergrößert (andere Figuren 1,3) – in echter Größe sah man aus der
    Spielkamera nur Punkte. Material `M_PlanMark` (unbeleuchtet, durchscheinend, Farbe und Deckkraft je
    Instanz als Custom Data, `Glow` 2), gebaut von `ue_import_items.py` (`build_plan_mark_master`). Gewählte Figur kräftig,
    andere blass, nach dem gezeigten Moment heller. Ring `SM_Plan_ring` am Ort jedes Schilds. Stockwerke über dem
    gezeigten sind ausgeblendet.
  - Schilder (`UDCWorldBadges`, ein Overlay unter dem Rahmen, Z −1, nimmt keine Klicks): `FDCWorldBadge` (Step/Door/Moment)
    aus `UDCHeistStateSubsystem::GetStepBadges/GetDoorBadges/GetMomentBadges`. Pro Ort eine Pille: Nummer im Kreis, Icon
    (`IconForTask`, Tresor-Schloss = `vault`), Verb (`DCItemWidgets::VerbForTask`) bei der gewählten Figur; andere dunkel
    mit Rand. Rot mit Warnzeichen, wenn ein Schritt nicht geht. Warten unterwegs = Sanduhr ohne Nummer. Schilder weichen
    sich nach oben aus. Maus darüber: Kasten mit Zeit, Dauer, Chance, Wartezeit. Klick: `HandleClickAt` fragt zuerst
    `FindStepAt` und wählt den Schritt. Türschilder (statt TextRender an `ADCDoor`, `GetSignLocation`) und Zeitvorschau
    (statt Stecknadeln) im selben Stil. `ADCPlanMarker` ist gelöscht. Der Text am Geist ist ausgeblendet (das Schild
    an derselben Stelle sagt es; `GetText` bleibt für Tests).
  - Neue Icons im Lucide-Stil (selbst gezeichnet): `lock-pick`, `crowbar`, `glass-break`; dazu `lock-open` (Lucide).
  - `M_ItemSurface`/`M_RoomSurface` bekommen `used_with_nanite` (Log meldete „MI_Item_* missing usage flag Nanite“).
  - Selbsttest `-DCSelfTest=Trail` (L_Villa01): Vic knackt die Gartenpforte, geht in den Garten, knackt die Haustür; Rosa
    wartet auf Vics Schritt 1. Prüft Fußspuren, Schild „1 knacken“ mit `lock-pick`, Rosas leise Schilder, Türschild,
    Klick auf Rosas Schild, Kamera folgt/frei/wieder, rotes Schild ohne Vics Knacken, Vorschau. Bilder
    `trail_vic/rosa/problem/preview.png`.
- **Durchsuchen für alles, „Öffnen mit …“, Funde selbst einpacken (Schritt 39, 2026-11-09 = Bauschritt 3 des Konzepts):**
  - Gegenstands-Menü (`OpenPickupMenu(Pickup, bSameSpot)`): „Durchsuchen“ bei jedem Möbelstück und Behälter, auch leer
  (Bedingung: Behälter, Inhalt, groß oder fest eingebaut – kleine lose Dinge haben nur „Mitnehmen“), „ca. 3 s · Inhalt
  unbekannt“. Ist es oder ein Einbauteil versperrt, öffnet „Durchsuchen“ den zweiten Ring „Schublade öffnen mit …“
  (`OpenOpenWithMenu(Pickup, bThenSearch)`): „mit Dietrich-Set knacken“, „mit Brecheisen aufbrechen“ (ausgegraut mit Grund,
  wenn nicht aufzubrechen), „Glas einschlagen“, „ohne Schublade durchsuchen“, „zurück“; eine Wahl legt Öffnen + Durchsuchen
  an. „Öffnen mit …“ im ersten Ring legt nur das Öffnen an. „Aufheben“ heißt jetzt „Mitnehmen“ (Menü, Schild, Schrittname).
  - Hilfen: `GoneInPlanReason`, `GetPlannedItemState`, `CheckPickupAction`. Radialmenü: `OpenSub` baut einen Ring an derselben
  Stelle im nächsten Tick (nie im eigenen Klick), `ChooseOption` baut ihn sofort, `HasOption` für Tests; Titelfeld 260 breit.
  - Verstecke: Sessel und Esstisch 2×1, Stuhl 1×1 (klein).
  - Einbruch: Findet jemand etwas, packt die Figur Wertvolles selbst ein, das Wertvollste pro Feld zuerst
  (`UDCContainerPanel::DecideAlone`, `TakeAllValuable` gibt zurück, ob alles passte). Das Spiel hält nur an, wenn nicht alles
  passt oder etwas ohne Wert oder Großes dabei ist – oder immer mit Optionen › Planung › Funde „immer fragen“
  (`UDCPlanSubsystem::bPauseOnEveryFind`, GameUserSettings `PauseOnEveryFind`, `-DCVariant=FindPause`). Der Find-Test setzt
  ihn. `Select` schließt ein Fund-Fenster nicht mehr.
  - Noch nicht: Türen mit demselben Ring (wartet auf die Frage „Türen aufhebeln?“), eigene Methodenliste im Katalog.
  - Selbsttest `-DCSelfTest=Search` (L_Room01): Sessel, kleines loses Ding, Schreibtisch-Ring, Tresor für Vic, Vitrine mit
    „zurück“, Rosa knackt und räumt den Tresor ohne Pause aus. Bilder `search_desk_ring/safe_vic/packed.png`.
- **Konzept 2026-11-09:** `Docs/Konzept-Wege-Durchsuchen-Gepaeck.md` – Bauschritte 1–3 gebaut (Schritte 38–39). Offen:
  4–5 Gepäck (Platz + Gewicht) statt Hände/Taschen/Raster, 6 Kleidung extra (Zeugen, Spuren). Offene Fragen am Ende der Datei.
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
Neu seit 2026-11-08: Türregel (öffnet unterwegs, wartet an verschlossener Tür daneben), roter Hinweis beim Klick und
„Plan hängt“, Türen im geplanten Zustand mit Schild, Geist der gewählten Figur, „Rosa wartet hierauf“, Einfügen hinter
dem gewählten Schritt, Rückgängig/Strg+Z – und welche der drei Varianten besser ist (Geist, Hinweis, Einfügen).
Neu (Schritt 28): Startmenü, Ladebildschirm, Pausemenü, Spielstände und Optionen mit der Maus – vor allem Auflösung und
Fenstermodus (im Selbsttest nicht umgeschaltet) und das Namensfeld beim Speichern.
Neu (Schritt 29): der neue Look im Ganzen, also Schriften, Farben, Rahmen, Knöpfe und Hover-Zustände mit der Maus
(die Selbsttests zeigen nur Standbilder).
Neu (Schritt 30): die Icons – passen sie, sind sie groß genug, fehlt irgendwo eins oder ist eins zu viel?
Neu (Schritt 31): die Porträts im Spiel – Ausschnitt (75 %) und Größe auf Karte und Profil.
Neu (Schritt 33): die Gegenstands-Icons – erkennt man alles (Dietrich-Set ist klein und grau), Kurzname nur ab 2 × 2,
Ziehen mit Bild mit der echten Maus.
Neu (Schritt 35): die Zeitleiste – Schraffur, Pfeile und Zeitmarke; lesbar genug, Streifen zu stark oder zu schwach?
Neu (Schritt 38): Kamera folgt (ruhige Mitte groß genug?), Fußspuren und Schilder im Raum (Größe, Farben, Ausweichen,
Maus-Kasten, Klick), die drei selbst gezeichneten Icons.
Neu (Schritt 39): zweiter Ring „… öffnen mit“, „Durchsuchen“ bei allem, Funde selbst einpacken (oder Option „immer fragen“).
- Command-line-Selbsttests: `-game -DCSelfTest=` `CloseFromSide`, `Inventory`, `Containers`, `Noise`,
  `Timeline`, `Reload`, `PlanInventory`, `Signals`, `Grid`, `Find`, `PlanView`, `DoorWay`, `House` und `AutoDoor`
  (beide Map L_House01), `Undo`, `PlanHelp`, `PlanLook`, `Insert`, `Variants` (die vier L_House01), `Villa` (L_Villa01),
  `Menu` (ohne Map: Startmenü, Spielstände, Ladebildschirm), `Replay` (L_Villa01, spielt `-DCSave=<Slot>` ab), `SecretDoor` (L_Villa01),
  `CrewLook` (L_Room01, Rosas Modell und Animationen von nah), `Trail` (L_Villa01, Fußspuren, Schilder, Kamera), `Search` (L_Room01, Durchsuchen und „Öffnen mit …“).
  Die Direkt-Tests schalten selbst in den Direkt-Modus.

**Wichtige Erkenntnisse:**
- MCP über HTTP ist langsam (~7 s pro Aufruf), weil der Editor im Hintergrund gedrosselt läuft.
- NavMesh: `AgentRadius=30`, 10-cm-Raster (DefaultEngine.ini) – sonst passt die Wegfindung nicht durch
  100-cm-Türen. Türblatt/Rahmen beeinflussen die Navigation nicht, nur der Sperrblock. Die Debug-Anzeige (`show Navigation`)
  zeigt auf den Maps Radius 35, auf dem Raster also 40 cm: Eine 1,0-m-Tür lässt nur 20 cm Gang. Nichts mit Kollision
  näher als 40 cm an eine Türöffnung stellen, enge Stellen lieber mit 1,2-m-Türen (Schritt 36).
- PIE übernimmt das NavMesh des Editors, so wie es gerade ist, und baut es nicht selbst. Darum baut
  `UDCHeistStateSubsystem` es in PIE beim Start neu. `-game` baut ohnehin neu, weil die Maps kein passendes NavMesh gespeichert
  haben. Editor-Prüfungen ohne Klicken: `UnrealEditor.exe DerClou3.uproject -ExecCmds="py <absoluter Pfad>.py"` mit
  `unreal.register_slate_post_tick_callback` (`-ExecutePythonScript` beendet den Editor gleich nach dem PIE-Start).
- Wege nur über `DCNav::FindPath`, Projektionen von Zielen über `DCNav::ProjectOnStorey` (siehe Schritt 32): Wegobjekte
  der Engine bleiben sonst bis zur GC aktiv und frieren bei jeder Türänderung das Spiel ein, und die Projektion der Engine
  wählt den nächsten Punkt nur von oben gesehen (auch ein Stockwerk tiefer).
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
- **Spielbare Demo für andere PCs** (Editor zu, seit 2026-11-09):
  `powershell -ExecutionPolicy Bypass -File Scripts\Tools\package_demo.ps1` → `Dist\Lindenhof-Demo-<Datum>\`
  (`Lindenhof.exe`, `Lies mich.txt` aus `Scripts\Tools\demo_readme.txt`) und dieselbe Fassung als `.zip`, Shipping, ~560 MB.
  Log in `Saved\Packaging\package_<Config>.log`; Kochen beim ersten Mal ~30 min, danach schneller. `Dist/` ist nicht im Git.
  - `-Config Development -SkipCook -NoZip` baut aus demselben Cook eine Fassung mit Log (`DerClou3\Saved\Logs` neben der
    exe). Damit laufen die Selbsttests im gepackten Spiel: `Lindenhof.exe /Game/DerClou/Maps/L_Villa01 -DCSelfTest=Villa`.
    Die Shipping-Fassung schreibt kein Log. Spielstände, Einstellungen und Screenshots liegen dort unter
    `%LOCALAPPDATA%\DerClou3\Saved`.
  - Gekocht werden nur die drei Maps (`MapsToCook` in DefaultGame.ini) und die Ordner aus `DirectoriesToAlwaysCook`.
    Neue Maps und Assets, die per Pfad geladen werden, müssen dort eingetragen werden. `Content/Slate` kommt als Datei mit
    in die .pak. Shader gibt es für DX12 (SM6) und DX11 (SM5). Das Fenster heißt „Lindenhof“ (`ProjectDisplayedTitle`).
  - Beigelegt ist `Engine\Extras\Redist\en-us\vc_redist.x64.exe`. Die Raum- und Figurennamen in den Maps erzeugen beim
    Kochen „FText initialized from FString“-Warnungen. Die sind harmlos, solange es keine Übersetzung gibt.
  - Die Development-Fassung meldet beim Start ein Ensure „Failed to load class /Script/GameFeatures.GameFeatureData“
    (der Eintrag in `AssetManagerSettings` ist nur für die MCP-Toolsets im Editor da). Das ist harmlos, in Shipping gibt
    es keine Ensures. Abhilfe wäre `bIsEditorOnly=True`; das bleibt aus, solange die Toolsets nicht damit geprüft sind.
  - Aus Git Bash Kartenpfade mit `MSYS_NO_PATHCONV=1` übergeben, sonst wird `/Game/...` zu `C:/Program Files/Git/Game/...`
    und das Spiel stürzt beim Laden ab.

## Git

- Branch `claude/sharp-rubin-uqa487`, Commit-Nachrichten Deutsch, nach jedem fertigen Schritt
  committen und pushen. Keinen PR ohne Auftrag.

## Sprache im Code

- Code, Bezeichner und Code-Kommentare Englisch; UI-Texte und Doku Deutsch.
