# Villa Lindenhof – Modelle aus Blender (Bauabschnitt 1)

Stand 2026-11-08. Auftrag des Users: alle Objekte aus dem Villa-Bauplan (`Scripts/Levels/villa_layout.py`,
Zeichnung `Docs/Villa/Villa-Grundriss.html`) in Blender modellieren – Bauabschnitt 1 (Grundstück, Remise, EG, Keller),
inklusive Türen, Fenster, Treppen, Zaun und neuer Beute/Werkzeuge. Je Agent höchstens 5 Objekte.

Entscheidungen des Users: Türen und Fenster werden echte Modelle und in `ADCDoor` eingebaut; das Villa-Level
(`Scripts/Levels/build_villa.py`, gehört dem Level-Chat) nimmt die Modelle automatisch statt der Würfel;
neue Beute/Werkzeuge bekommen jetzt nur Modelle, Katalogwerte (Gewicht, Raster, Wert) werden später besprochen.

## Pipeline

- Jede Gruppe hat ihre **eigene Datei** `Art/Items/dc_villa_<gruppe>.py`. Sie registriert ihre Objekte mit
  `@item("<id>")` aus `dc_items` (siehe `dc_items_deco.py` als Vorbild). `dc_items.py` importiert alle
  `dc_villa_*.py` automatisch; eine kaputte Datei stoppt die anderen nicht (Fehler steht im Log).
- Bauen und ansehen (Blender headless, mehrere gleichzeitig möglich):
  ```
  "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup -P Art/Items/build_items.py -- --items a,b --preview --ref --sheet <gruppe>
  ```
  Vorschau: `Saved/ItemPreviews/<id>.png` (schräg), `<id>_top.png` (steil wie die Spielkamera),
  `_sheet_<gruppe>.png`. `--ref` stellt eine graue 1,80-m-Figur daneben. `--export` schreibt
  `Art/Items/Export/SM_<id>.fbx`.
- Unreal-Import macht der Haupt-Chat am Schluss für alle (`ue_import_items.py`, Editor zu).

## Regeln für alle Modelle

- Maße in **echten cm**. Ursprung **unten Mitte** (Ausnahmen unten bei Türen, Wandteilen, Treppen).
- **Vorderseite = Blender −Y** (wird in Unreal +Y). Breite entlang X, Tiefe entlang Y, Höhe Z.
- Stil wie die vorhandenen Modelle (`dc_items.py`: desk, safe, wardrobe, piano): Grundformen mit Fasen
  (`bevel`), Drechselteile (`lathe`), Profile (`sweep`), Polster (`puff`), Box-UVs. Glaubwürdige Details, aber
  **von oben lesbar** – das Spiel schaut steil von oben. Die `_top.png` ist die wichtigere Vorschau.
- Umfang: Möbel ca. 1–10 k Faces, Kleinteile < 3 k, Autos/Bäume/Treppen bis ca. 25 k.
- Nur Materialien aus `dc_materials.py` (Namen exakt). Fehlt eines, das nächstliegende nehmen und den Wunsch im
  Bericht nennen. **`dc_materials.py`, `dc_mesh.py`, `build_items.py`, `dc_items*.py` nicht ändern.**
  Eigene Hilfsfunktionen gehören in die eigene Datei.
- Keine echten Marken, Logos, Typenschilder oder Nachbauten bestimmter Produkte (Autos: eigene, generische Form).
  Texte nur deutsch und generisch („MEHL“, „ALARM“).
- Wiederholte Module (Regal, Küchenzeile, Hecke, Zaun) müssen **nahtlos aneinander passen**: an den Enden glatt
  abschließen, nichts darf über die Modulbreite hinausragen.

## Materialien (Auswahl, alle in `dc_materials.py`)

Holz: `Wood_Walnut`, `Wood_Oak`, `Wood_Mahogany`, `Wood_Light`, `Wood_Dark`, `Wood_Weathered`, `Bark` ·
Lack/Farbe: `Paint_White`, `Paint_Anthracite`, `Paint_Grey`, `Paint_Green`, `Paint_Yellow`, `Paint_Red`, `Paint_Safe`,
`Lacquer_Black/Red/Cream/White` · Metall: `Steel`, `Aluminium`, `Chrome`, `Brass`, `Gold`, `Silver`, `Gilt`,
`Iron_Dark`, `Bronze`, `Copper` · Kunststoff: `Plastic_Black/White/Grey/Red/Yellow`, `Rubber_Black`, `Screen_Black` ·
Stoff/Leder: `Fabric_*` (Red, Blue, Navy, Olive, Black, Grey, Cream), `Linen_White`, `Wool_Brown`, `Velvet_Red`,
`Felt_Red`, `Felt_Green`, `Leather_Brown/Black/Green` · Stein: `Marble`, `Stone_Sand`, `Concrete`, `Porcelain`,
`Porcelain_Blue`, `Terracotta`, `Enamel_Cream`, `Ivory` · Pflanzen: `Leaves_Green`, `Leaves_Dark`, `Leaves_Fresh`,
`Fruit_Red` · Leuchtend: `Lamp_Glass`, `Bulb`, `Lampshade_Cream`, `LED_Red`, `LED_Green`, `Display_Blue` ·
Durchsichtig: `Glass`, `Glass_Frosted`, `Glass_Bottle`, `Glass_Tinted`, `Water` · Papier: `Paper`, `Cardboard`,
`Binder_Blue`, `Banknote`, `Canvas`.

## Sonderregeln Architektur

**Türblatt** (alle `Door_*` außer Zargen): Ursprung = **Scharnier unten**. Das Blatt läuft in Blender von
`y = 0` bis `y = −B` (B = Blattbreite), Dicke entlang X um `x = 0` zentriert, `z = 0 … H`. Bänder (Scharniere) bei
`y ≈ 0`, Griff/Schloss bei `y ≈ −(B − 8)` auf **beiden** Seiten. Außenseite bzw. Schauseite = **−X**.
Der Code skaliert das Blatt auf die echte Türgröße (meist ±10 %), also keine Details, die beim Strecken stören.
Standard-Blattdicke 4–6 cm (Panzertür dicker, siehe dort).

**Zarge** (`Door_frame_*`): wie der bisherige 100-cm-Würfel – Ursprung in der **Mitte**, Pfosten 16 (X) × 8 (Y) ×
100 (Z), Sturz 16 × 100 × 8. Der Code skaliert entlang der Länge. Pfosten in Y symmetrisch, Bekleidung auf beiden
Wandseiten (±X).

**Fenster** (`Window_*`): Ursprung = **Mitte unten in der Öffnung**, auf der Wandmitte. Breite entlang X (um 0
zentriert), Wandnormale = Y, **außen = −Y**, Außenwand 30 cm dick (y −15 … +15). Rahmen sitzt im äußeren Drittel,
Fensterbank außen (Anthrazit, steht 4 cm vor) und innen. Der Code skaliert X auf die echte Breite.

**Wandteile** (Wandleuchte, Sicherungskasten, Kamera, Vordach …): Rückseite liegt an der Wand bei `y = 0` (bzw.
`+Tiefe/2`, siehe Tabelle), das Teil ragt nach −Y. Was im Level in welcher Höhe hängt, steht in der Tabelle.

**Treppen** (`Stair_*`): Ursprung = Mitte der **untersten** Stufenkante auf dem unteren Boden, die Treppe steigt
nach **+Y**, Breite entlang X. Die Treppe hat im Spiel keine eigene Kollision (der Level-Bauer behält seine
Stufenkästen für das NavMesh) – das Modell muss nur gut aussehen und die Maße genau treffen.

**Mehrteilige Objekte** (Baum = Stamm + Krone, Zarge = Pfosten + Sturz …) zählen als **ein** Objekt, sind aber
mehrere Modelle mit eigenen ids.

## Objekte und Gruppen

Spalten: id (= `SM_<id>`), Name, Maße B × T × H in cm (B entlang X), Hinweise. Maße aus Katalog bzw. Bauplan –
genau einhalten (±2 cm), außer „ca.“.

### G1 – Katalog-Möbel · `dc_villa_furniture.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| dresser | Kommode | 100 × 45 × 85 | Katalog „Breite Kommode mit Schubladen“, Nussbaum, 2 × 3 Schubladen, Messinggriffe; dient auch als Anrichte (Esszimmer) und Bar-Schrank (Keller) |
| bed | Doppelbett | 200 × 150 × 55 | Länge entlang **X**, Kopfteil bei **+X** (bis ca. 105 hoch), Matratzenoberkante 55, Bettdecke, 2 Kissen, Holzrahmen |
| nightstand | Nachttisch | 45 × 40 × 55 | eine Schublade, darunter offenes Fach |
| grand_piano | Flügel | ca. 150 × 215 × 100 | Tastatur vorn (−Y), Schwanz nach +Y, Deckel offen (Saiten von oben sichtbar), schwarzer Lack, 3 Beine, Pedale, Bank NICHT dazu |
| Deco_key_board | Schlüsselbrett | 40 × 5 × 25 | hängt in der Garderobe (Unterkante 140); Holzbrett, 5 Haken, 3 Schlüsselbunde mit Anhänger; Rückseite bei y = +2,5 |

### G2 – Küche · `dc_villa_kitchen.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Deco_kitchen_island | Kochinsel | 240 × 100 × 92 | Marmorplatte, Eichenfronten ringsum (Schubladen), auf einer Längsseite Überstand mit Platz für Hocker |
| Deco_kitchen_base | Unterschrank-Modul | 60 × 60 × 90 | Modul, wird gereiht: Arbeitsplatte (Marmor) bündig 60 breit, Schubladen, Sockel zurückgesetzt |
| Deco_kitchen_sink | Spülen-Modul | 60 × 60 × 90 | wie base, mit Edelstahlbecken und Armatur (Chrome) |
| Deco_kitchen_stove | Herd-Modul | 60 × 60 × 90 | wie base, mit Glaskeramik-Kochfeld (4 Felder) und Backofentür mit Glas |
| Deco_fridge | Kühlschrank | 60 × 65 × 200 | Edelstahl, zwei Türen, Stangengriffe |

### G3 – Wohnen & Bad · `dc_villa_living.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Deco_fireplace | Kamin | 180 × 45 × 110 | Marmor-Einfassung, dunkle Brennkammer mit Rost und Holzscheiten, Kaminsims; Rückseite an der Wand (+Y) |
| Deco_bookshelf | Bücherregal-Modul | 90 × 37 × 220 | Nussbaum, 6 Böden voller Bücher (verschiedene Höhen/Farben, ein paar liegend), Modul wird gereiht |
| Deco_toilet | WC | 38 × 65 × 80 | Stand-WC mit Spülkasten an der Wand (+Y), Deckel zu |
| Deco_washbasin | Waschtisch | 60 × 45 × 88 | wandhängend, Oberkante 88, Unterschrank oder Siphon, Armatur; Rückseite +Y, Ursprung am Boden |
| Deco_washing_machine | Waschmaschine | 60 × 60 × 85 | weiß, runde Tür vorn, Bedienblende |

### G4 – Technik & Sicherheit · `dc_villa_tech.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Deco_heating | Heizung | 120 × 120 × 180 | Gaskessel + Warmwasserspeicher + Kupferrohre, Manometer; Rückseite +Y |
| Deco_fuse_box | Sicherungskasten | 80 × 18 × 80 | Wandkasten grau, Tür mit Sichtfenster; hängt mit Unterkante 120; Rückseite y = +9 |
| Deco_alarm_central | Alarmzentrale | 120 × 18 × 60 | Wandpaneel: Zentrale mit Display und LEDs, Akkukasten, rote Blitzleuchte, Kabelkanal; Unterkante 120; Rückseite y = +9 |
| Deco_alarm_keypad | Alarm-Bedienteil | 14 × 4 × 18 | Wand neben der Haustür, Unterkante 135; Tastenfeld, kleines Display, grüne/rote LED; Rückseite y = +2 |
| Deco_metal_shelf | Lagerregal Metall | 100 × 40 × 180 | verzinkt (Steel), 5 Böden mit Kartons, Farbeimern, Kisten; Modul wird gereiht |

### G5 – Keller-Freizeit · `dc_villa_leisure.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Deco_wine_rack | Weinregal-Modul | 100 × 50 × 200 | Eiche, Rautenfächer oder Reihen, liegende Flaschen (`Glass_Bottle`, Hälse sichtbar von beiden Seiten – steht auch frei im Raum), Modul wird gereiht |
| Deco_billiard_table | Billardtisch | 280 × 160 × 80 | Mahagoni, grünes Tuch, 6 Taschen, Kugeln im Dreieck, ein Queue liegt drauf |
| Deco_treadmill | Laufband | 180 × 80 × 140 | Länge entlang X, Konsole bei +X |
| Deco_weight_bench | Hantelbank | 140 × 40 × 110 | Länge entlang X, Ablage mit Langhantel, Polster schwarz |
| Deco_sauna_bench | Saunabank | 220 × 55 × 45 | helles Holz, Lattensitz, Rückenlatten an der Wand (+Y) |

### G6 – Remise · `dc_villa_garage.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Deco_workbench | Werkbank | 310 × 60 × 90 | Eichenplatte genau auf **90** (dort liegen Brecheisen u. a.), Schraubstock, Schubladen, Lochwand mit Werkzeug hinten (+Y, bis 190) – Platte vorn frei lassen |
| Deco_car_modern | Auto (modern) | 180 × 430 × 160 | **Länge entlang Y**, Front bei −Y; schwarzer Lack, getönte Scheiben, Felgen; generisch, kein echtes Modell |
| Deco_car_classic | Oldtimer | 190 × 430 × 150 | Länge entlang Y, Front −Y; Karosserie rot, Dach creme, Chromstoßstangen, Weißwandreifen; generischer 50er-Stil |
| Deco_lawn_mower | Rasenmäher | 55 × 90 × 100 | Schiebemäher mit Fangkorb, Griff nach +Y |
| Deco_tool_rack | Gartengeräte-Halter | 100 × 15 × 170 | Wandleiste mit Spaten, Rechen, Besen, Schlauchtrommel; Rückseite y = +7,5 |

### G7 – Neue Beute · `dc_villa_loot.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| gold_bar | Goldbarren 1 kg | 11,6 × 5,2 × 1,8 | Trapezform, Prägung (Zahl/Linien, keine echte Marke) |
| wine_bottle | Wein-Rarität | Ø 8, 32 hoch | dunkles Glas, altes Etikett (Paper), Wachskapsel (Velvet_Red o. ä.) |
| silverware_case | Silberbesteck-Kasten | 45 × 30 × 9 | Holzkasten zu, Messingschließe, Griff |
| watch_box | Uhrenbox | 30 × 20 × 10 | Leder, Glasdeckel, 6 Uhren auf Kissen sichtbar |
| flour_tin | Mehldose | 15 × 15 × 22 | Emaille creme, Deckel, Aufschrift „MEHL“ (erhabene Buchstaben oder Schild) |

### G8 – Werkzeug & Kleines · `dc_villa_tools.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| bolt_cutter | Bolzenschneider | 75 × 15 × 4 | liegend, Länge entlang X, rote Griffe |
| ladder | Schiebeleiter | 300 × 45 × 9 | Alu, zweiteilig zusammengeschoben, **liegend**, Länge entlang X |
| cordless_drill | Akkuschrauber | 25 × 8 × 22 | stehend auf dem Akku, gelb/schwarz |
| key_ring | Schlüsselbund | 10 × 5 × 1,5 | 3 Schlüssel, Ring, Anhänger |
| garage_remote | Handsender | 6 × 3 × 1,5 | schwarz, 2 Tasten, rote LED |

### G9 – Türen I · `dc_villa_doors.py` (Türblatt-Regel!)
| id | Name | Blatt B × D × H | Hinweise |
|---|---|---|---|
| Door_inner | Innentür | 88 × 4 × 208 | weiß lackiert, schlicht mit Kassetten, Drücker Chrom beidseitig |
| Door_front | Haustür | 118 × 7 × 218 | massiv, dunkles Holz, schmaler Glasstreifen, außen (−X) Edelstahl-Stoßgriff, innen Drücker, Sicherheitsbeschlag |
| Door_exterior | Nebeneingangstür | 98 × 5 × 208 | Seiteneingang/Werkstatt/Garage: robust, grün oder anthrazit, kleines Fenster oben |
| Door_glass | Glastür | 98 × 5 × 208 | weißer Rahmen, große Glasscheibe (Küchentür; Saunatür nutzt sie auch) |
| Door_french_leaf | Flügeltür-Flügel | 69 × 4 × 208 | **ein** Flügel einer Doppeltür, weiß, Glas mit Sprossen; zwei davon ergeben 1,4 m |

### G10 – Türen II · `dc_villa_doors_special.py` (Türblatt-Regel!)
| id | Name | Blatt B × D × H | Hinweise |
|---|---|---|---|
| Door_steel | Stahltür | 88 × 5 × 208 | Feuerschutztür grau, Türschließer oben, Drücker |
| Door_vault | Panzertür | 98 × 18 × 208 | Tresorraum: massiv Stahl, großes Drehrad, Zahlenschloss, sichtbare Riegelbolzen an der Kante, schwere Bänder |
| Door_grate | Gittertür | 98 × 4 × 208 | Weinkeller: Schmiedeeisen (Iron_Dark), senkrechte Stäbe, Ranken oben, Schlosskasten |
| Door_slatted | Lattentür | 88 × 4 × 208 | Kellerlager: senkrechte Holzlatten mit Lücken, Querriegel, Vorreiber |
| Door_bookshelf | Regal-Geheimtür | 88 × 30 × 208 | Bücherregal als Türblatt, Bücher zur **−X**-Seite, Rückseite schlicht; Tiefe x −27 … +3 |

### G11 – Zargen & Tore · `dc_villa_gates.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Door_frame_post + Door_frame_top | Holzzarge | 16 × 8 × 100 / 16 × 100 × 8 | Zargen-Regel, weiß, Falz und Bekleidung |
| Door_frame_metal_post + Door_frame_metal_top | Stahlzarge | wie oben | grau (Paint_Grey), für Stahl- und Panzertür |
| Door_garage | Sektionaltor | 498 × 5 × 218 | Türblatt-Regel, weiße waagerechte Sektionen mit Sicken, Griff unten |
| Door_sliding_frame + Door_sliding_panel | Hebeschiebetür | 300 × 15 × 220 / 150 × 6 × 216 | Rahmen mit festem Flügel, beweglicher Flügel extra; Ursprung jeweils linke Kante unten (y 0 … −B) |
| Gate_sliding | Einfahrtstor | 400 × 6 × 180 | Türblatt-Regel, Anthrazit, senkrechte Stäbe, Laufschiene/Rollen unten |

### G12 – Fenster · `dc_villa_windows.py` (Fenster-Regel!)
| id | Name | Öffnung B × H | Hinweise |
|---|---|---|---|
| Window_casement | Fenster | 120 × 120 | Drehkipp, weißer Rahmen, ein oder zwei Flügel, Fensterbänke |
| Window_tall | bodentiefes Fenster | 120 × 210 | wie oben bis zum Boden, ohne Innenbank |
| Window_tilt | Kippfenster | 70 × 70 | Flügel **gekippt** (oben ca. 12 cm nach innen) |
| Window_frosted | Milchglasfenster | 100 × 70 | Glass_Frosted |
| Window_cellar + Deco_lightwell | Kellerfenster + Lichtschacht | 100 × 70 / 110 × 60 × 100 | Fenster mit Gitter; Lichtschacht: Betonwanne außen vor dem Fenster (Oberkante = Gelände, Ursprung oben Mitte an der Wandaußenseite, ragt nach −Y), Gitterrost oben |

### G13 – Zaun & Hecken · `dc_villa_fence.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Fence_panel | Zaunfeld | 200 × 5 × 180 | X −100 … +100, Anthrazit-Stabzaun mit Ober-/Untergurt, Modul |
| Fence_post | Zaunpfosten | 8 × 8 × 190 | mit Kappe |
| Gate_garden | Gartenpforte | 118 × 5 × 180 | Türblatt-Regel, passend zum Zaun, Drücker + Schloss |
| Hedge_thuja | Thujahecke | 200 × 100 × 220 | dicht, dunkel (Leaves_Dark), geschnitten, leicht unregelmäßig, Modul: Enden offen/flach, damit Stücke verschmelzen |
| Hedge_privet | Ligusterhecke | 200 × 80 × 120 | heller (Leaves_Fresh), kleinblättriger, Modul wie oben |

### G14 – Bäume · `dc_villa_trees.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Tree_deciduous_trunk + Tree_deciduous_canopy | Laubbaum | Krone Ø 600, Höhe ca. 800 | Stamm mit Hauptästen bis in die Krone; Krone aus Blattbüscheln, von oben lockerer Umriss |
| Tree_fruit_trunk + Tree_fruit_canopy | Obstbaum | Krone Ø 400, Höhe ca. 450 | knorriger Stamm, rote Früchte |
| Tree_conifer_trunk + Tree_conifer_canopy | Nadelbaum | unten Ø 500, Höhe ca. 900 | Fichte/Tanne in Etagen |
| Deco_bush | Strauch | ca. 160 × 160 × 110 | für Beete |
Krone und Stamm getrennt, beide mit Ursprung am Stammfuß (so kann das Spiel die Krone ausblenden).

### G15 – Außenlicht · `dc_villa_lights.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Lamp_lantern | Mastlaterne | ca. 35 × 35 × 230 | Anthrazit, Laternenkopf mit `Lamp_Glass`; Lichtpunkt-Höhe im Bericht nennen |
| Lamp_bollard | Poller | Ø 16 × 80 | Lichtschlitz oben |
| Lamp_wall | Wandleuchte | ca. 15 × 20 × 30 | Wandteil (Rückseite y = 0), Ursprung Mitte Wandplatte, hängt bei ca. 200 |
| Lamp_spot_sensor | Strahler mit Bewegungsmelder | ca. 25 × 20 × 25 | Wandteil, LED-Fluter + Melderkuppel, **aus** (kein Leuchtmaterial) |
| Lamp_ground | Bodenstrahler | Ø 15 × 3 | bündig im Boden (Oberkante z = 0 … 2), Glas nach oben |

### G16 – Garten & Terrasse · `dc_villa_garden.py`
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Deco_pavilion | Pavillon | 450 × 450 × 320 | Holz, 4 Pfosten, Zeltdach, offen, Bank an einer Seite |
| Deco_fountain | Brunnen | Ø 260 × 120 | rundes Steinbecken (Stone_Sand) mit Wasser (`Water`), Säule mit Schale in der Mitte |
| Deco_mailbox | Briefkasten-Säule | 40 × 15 × 140 | Edelstahl: Briefkasten, Klingel, Gegensprechanlage, Hausnummer „12“ |
| Deco_sun_lounger | Sonnenliege | 70 × 195 × 35 | Teak (Wood_Weathered), Auflage, Kopfteil hochgestellt bei +Y |
| Deco_pool_ladder | Poolleiter | 50 × 60 × 180 | Chrom, Ursprung = Beckenrand Mitte (z = 0 Deckhöhe), Bügel oben ca. 80, Stufen hängen bis −110 ins Becken (Richtung +Y) |

### G17 – Treppen & Anbauten · `dc_villa_stairs.py` (Treppen-Regel!)
| id | Name | Maße | Hinweise |
|---|---|---|---|
| Stair_hall | Hallentreppe | 160 breit, 500 lang, steigt 310 | 17 Stufen, Eichenstufen, weiße Wangen; rechts (+X) Wand mit Handlauf, links (−X) Geländer mit Pfosten |
| Stair_cellar | Kellertreppe | 150 breit, 445 lang, steigt 290 | 16 Stufen, Stein/Beton, beidseitig Wand, Handlauf rechts (+X) |
| Stair_outside | Kelleraußentreppe | 140 breit, 360 lang, steigt 290 | Beton, Stützwände beidseitig bis Geländehöhe (z = 290), oben Geländer um die Öffnung |
| Deco_canopy | Vordach | 380 × 200 | Glasdach auf Edelstahlkonsolen, Wandteil: Rückseite y = 0, Unterkante 250, Ursprung am Boden unter der Wandmitte |
| Deco_security_camera | Überwachungskamera | ca. 12 × 30 × 15 | Wand-/Mastarm, Gehäuse weiß, rote LED, blickt nach −Y; Ursprung Mitte Montageplatte |

## Ablauf und Stand

Je Gruppe ein Agent (max. 5 Objekte), bis zu 6 gleichzeitig. Vorschauen: `Saved/ItemPreviews/_sheet_g<N>.png`.

| Gruppe | Stand |
|---|---|
| G1–G17 (alle 83 Objekte) | fertig, importiert; im Level L_Villa01 (Selbsttest Villa PASSED) |
| G7 Beute, G8 Werkzeug, `grand_piano` | Modelle fertig, im Spiel erst mit Katalogeintrag |

Einbau-Entscheidungen (`build_villa.py`):
- Kelleraußentreppe: das Modell zeigt nur die Stufen (`OUTSIDE_WALLS = ()`), die Mauern des Levels reichen bis zur
  Kellertür und sind oben Brüstung.
- Laternen und Poller ohne eigene Kollision (die Pfortenlaterne steht auf dem Gehweg und hätte den Weg verengt),
  ein unsichtbarer dünner Mast blockt.
- Vordach, Wandleuchten und Strahler an der Hausfassade tragen `DCFloor_EG` (in der Keller-Ansicht ausgeblendet).

Katalog-Vorschläge der Agenten (noch mit dem User zu besprechen, nicht im Spiel):

| id | Gewicht | Größe | Raster | Wert | Hinweis |
|---|---|---|---|---|---|
| gold_bar | 1,0 kg | klein | 1×1 | 20–30 T€ (Spielbalance) | |
| wine_bottle | 1,4 kg | mittel | 3×1 | 3–5 T€ | zerbrechlich beim Fallenlassen? |
| silverware_case | 6 kg | mittel | 4×3 | 300 € + Besteck 5 T€ | oder Behälter mit Schloss 2 |
| watch_box | 2,5 kg | mittel | 3×2 | 150 € + 6 Uhren | Behälter |
| flour_tin | 0,8 kg | mittel | 2×2 | 20 € | Versteck (Geld/Schmuck) |
| bolt_cutter | 3,5 kg | groß | 1×6 | 60 € | Hände/Rücken |
| ladder | 12 kg | sperrig | – | 120 € | nur Hände, Balkon-Einstieg |
| cordless_drill | 1,6 kg | mittel | 2×2 | 150 € | |
| key_ring | 0,1 kg | klein | 1×1 | 5 € | Schlüssel-Funktion |
| garage_remote | 0,05 kg | klein | 1×1 | 30 € | öffnet das Garagentor |

Hinweise aus den Berichten:
- Flügel (`grand_piano`), neue Beute und Werkzeug brauchen noch Katalogeinträge (Werte mit dem User besprechen);
  im Villa-Level steht bis dahin das aufrechte Klavier (`piano`).
- Hantelbank: die Langhantel macht das Modell 150 cm breit (wird unskaliert gesetzt).
- Waschtisch hängt frei (Geometrie ab z = 40, Ursprung am Boden) – nicht auf den Würfel skalieren.
- Bücherregal: jedes zweite Modul wird gespiegelt, damit sich das Muster nicht alle 90 cm wiederholt.
- Bäume: Kronen ohne Kollision; der alte Stammzylinder bleibt unsichtbar als Hindernis.
- Materialwünsche (später): Lacquer_Purple/Orange (Billardkugeln), Wood_Spruce, polierter Stahl, LED_Yellow,
  Plastic_Blue, verzinkter Stahl.

Nach den Agenten: Haupt-Chat prüft die Vorschauen, Import nach Unreal, `ADCDoor` nutzt die Türmodelle,
`build_villa.py` nimmt die Modelle statt der Würfel (nachdem der Level-Chat fertig ist).
