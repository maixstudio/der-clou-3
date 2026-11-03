# 04 – Pipeline Blender → Unreal

## Werkzeuge

| Zweck | Werkzeug |
|---|---|
| Modellierung, UVs, Rigging, Animation-Cleanup | **Blender** (eine LTS-Version für das ganze Team fixieren) |
| Export nach Unreal | Epic **„Send to Unreal“**-Add-on (Unreal Blender Tools) oder manueller FBX-Export |
| Texturierung | Trim-Sheets in Blender; optional Substance Painter für Hero-Props |
| Fertige Assets | **Fab / Quixel Megascans** (für UE-Projekte frei nutzbar – Lizenz pro Asset prüfen) |
| Charaktere | Stilisierte Eigenmodelle; MetaHuman für Porträts/Nahaufnahmen |
| Level-Bau | Unreal: Modular-Kits + PCG für Dressing |

## Einheiten und Achsen

- Blender-Szene in **Metern**, Unit Scale **1.0**; 1 Blender-Einheit = 1 m = 100 UE-Einheiten.
- **Statische Meshes:** FBX-Export mit *Apply Scalings: FBX Units Scale*, *Forward: X Forward*, *Up: Z Up*,
  *Apply Transform* aktiv → das Mesh schaut in Unreal entlang +X.
- **Skeletal Meshes:** Für saubere Bone-Skalierung entweder Szene auf *Unit Scale 0.01 / Zentimeter*
  umstellen oder konsequent das Send-to-Unreal-Add-on nutzen (übernimmt die Korrekturen). Eine Variante
  wählen und im Team nicht mischen.
- Pivot: Modular-Teile unten links am Raster, Props unten mittig (steht auf dem Boden).
- Vor Export: Transformationen anwenden (`Ctrl+A → All Transforms`), keine negativen Skalierungen.

## Modular-Kits (Gebäude)

- **Raster 50 cm**, Wandhöhe 300 cm (Stockwerk inkl. Decke 350 cm), Wandstärke 20 cm (innen) / 40 cm (außen).
- Standardteile: Wand 100/200/400 cm, Ecke innen/außen, Tür 100 cm, Doppeltür 200 cm, Fenster 100/150 cm,
  Treppe 1 Stockwerk, Boden 200×200 cm, Deckenplatte.
- Türen und Fenster sind **eigene Actors** (Blueprint mit Smart-Object-Komponente), nicht Teil der Wand.
- Jedes Kit-Teil hat eine **Innenseite und eine Außenseite** als separate Material-Slots, damit die
  Puppenhaus-Ausblendung nur die Kamera-zugewandte Seite ausblendet.
- Kits pro Epoche/Bauart: Altbau, Nachkriegs-Geschäftshaus, Industrie/Hafen, Bank (Marmor).

## Benennung

| Präfix | Typ | Beispiel |
|---|---|---|
| `SM_` | Static Mesh | `SM_Altbau_Wand_200` |
| `SK_` | Skeletal Mesh | `SK_Crew_Elektrikerin` |
| `SKEL_` | Skeleton | `SKEL_Humanoid` |
| `A_` / `AM_` | Animation / Montage | `A_Lockpick_Loop` |
| `T_` | Textur (`_D`, `_N`, `_ORM`, `_E`) | `T_Altbau_Trim_N` |
| `M_` / `MI_` | Material / Instanz | `MI_Holz_Eiche_Dunkel` |
| `BP_` | Blueprint | `BP_Door_Security` |
| `DA_` / `DT_` | Data Asset / DataTable | `DA_Tool_Stethoskop` |
| `UCX_` | Kollision (in Blender) | `UCX_SM_Altbau_Wand_200_00` |

Namen auf Englisch oder Deutsch – **einmal festlegen**. Keine Umlaute und Leerzeichen in Dateinamen.

## Geometrie-Budgets

| Asset | Richtwert |
|---|---|
| Modular-Teil (Nanite) | frei, aber sinnvoll; Bevels mitmodellieren statt Normal-Map-Bake |
| Hero-Prop (Tresor, Alarmkasten) | 5–30k Tris, Nanite |
| Kleinteile (Stifte, Gläser) | Nanite oder Instanced, Aufsammeln über PCG |
| Charakter | 30–60k Tris, 4 LODs (Skeletal Meshes ohne Nanite planen) |
| Texturen | 2K Standard, 4K nur Hero-Props, Trim-Sheets 4K |

## Materialien

- Ein **Master-Material** pro Kategorie (Umgebung, Prop, Charakter, Glas, Wasser/Nass) mit Instanzen.
- Gemeinsame Parameter in der Material Parameter Collection: *Nässe* (Regen), *Wand-Ausblendung*,
  *Planungs-Highlight*.
- Texturkanäle gepackt: `ORM` (Occlusion, Roughness, Metallic).

## Ordnerstruktur in Unreal

```
/Content/NW/
├── Characters/   (Crew, Wachen, Passanten)
├── Environments/ (Kits, Objekte, Hub)
├── Props/
├── Materials/    (Master-Materialien, MPC)
├── VFX/
├── Audio/
├── UI/
├── Data/         (Data Assets, DataTables)
├── Maps/         (Hub, Objekte, Test)
└── Developers/   (persönliche Testordner, nicht ins Release cooken)
```

Quelldateien (`.blend`, `.spp`, `.psd`) liegen außerhalb von `Content/` in `/SourceArt/` mit derselben Struktur.

## Versionierung

- **Git + Git LFS** reicht für 1–5 Personen. Ab mehreren Artists, die parallel an Maps arbeiten,
  **Perforce** (bis 5 Nutzer kostenlos) oder **Diversion** erwägen.
- `.gitattributes` (wird beim Anlegen des UE-Projekts ergänzt):

```gitattributes
*.uasset filter=lfs diff=lfs merge=lfs -text lockable
*.umap   filter=lfs diff=lfs merge=lfs -text lockable
*.blend  filter=lfs diff=lfs merge=lfs -text lockable
*.fbx    filter=lfs diff=lfs merge=lfs -text
*.png    filter=lfs diff=lfs merge=lfs -text
*.tga    filter=lfs diff=lfs merge=lfs -text
*.wav    filter=lfs diff=lfs merge=lfs -text
*.spp    filter=lfs diff=lfs merge=lfs -text lockable
```

- **Datei-Locking** für alle binären UE-Assets nutzen; im Editor Revision Control (Git-Plugin) aktivieren.
- **One File Per Actor** (World Partition) oder Sublevels pro Gebäudeteil, damit zwei Personen gleichzeitig
  an einer Map arbeiten können.
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` niemals einchecken.

## Ablauf für ein neues Asset

1. Blockout in Unreal (Grau-Geometrie, Modeling Tools) – Spielbarkeit zuerst prüfen.
2. Modell in Blender nach Kit-Regeln, Kollision `UCX_` dazu.
3. Export per Send-to-Unreal in den passenden Ordner, Material-Instanz zuweisen.
4. Validierung über `NWEditor` (Namen, Pivot, Kollision, Texturgröße).
5. Commit mit Asset-Lock-Freigabe.
