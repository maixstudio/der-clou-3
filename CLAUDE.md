# CLAUDE.md – Übergabe & Arbeitsanweisung

Diese Datei ist die Übergabe aus der Cloud-Sitzung an dich, die **lokale Claude-Code-Sitzung auf dem
Windows-PC des Users**. Du sollst das Spiel im **Unreal Editor 5.8 über MCP** weiterbauen.
Lies diese Datei ganz, prüfe dann die Checkliste in Abschnitt 3 und arbeite die Aufgaben in Abschnitt 4
**der Reihe nach** ab. Sprich mit dem User **Deutsch**.

## 1. Projekt in Kürze

- **NACHTWERK** (Arbeitstitel) ist ein taktisches Einbruchs-Planungsspiel aus der Vogelperspektive:
  Crew anwerben → Objekt auskundschaften → Einbruch auf einer Zeitleiste planen → Plan in Echtzeit ausführen
  (mit Pause/Eingriff) → Beute beim Hehler verkaufen. Fiktive Hafenstadt Kaltbrunn, 1972.
- Es ist ein **eigenständiger geistiger Nachfolger** von „Der Clou!“ (1994). **Keine** Namen, Figuren,
  Texte, Grafiken oder Musik des Originals verwenden – nur das Genre und die Mechaniken.
- Der User möchte als **Projektname „der clou 3“** (Dateiname `DerClou3`, weil C++-Projekte keine
  Leerzeichen erlauben). Der Spieltitel in Doku und Spiel bleibt **NACHTWERK**.
- Solo/Kleinteam, Unreal Engine **5.8** + Blender, Ziel „AAA-Look durch Fokus“, nicht AAA-Umfang.

## 2. Was schon existiert

| Bereich | Stand |
|---|---|
| `docs/01–06` | Vision, Game Design (inkl. Beispiel-Coup Juwelier), Technik, Blender-Pipeline, Art & Audio, Roadmap |
| `Source/NWCore` | Deterministische Simulation `nw::sim::Simulation` in **reinem C++** (`Public/Sim`, `Private/Sim`): 10-Hz-Tick, Integer-Positionen, Graph-Navigation, Türen/Schlösser, Kontakt-Alarm, Lichtschranke, Tresor, Crew-Signale, Wachen (Patrouille, Sehen, Hören, Nachsehen), Polizei-Countdown, Event-Log, Zustands-Hash. Referenzszenario `MakeJewelerScenario()` in `SimScenarios.cpp`. Automation Tests in `Private/Tests` |
| `Source/NWPlanning` | Data Assets `UNWHeistObjectAsset`, `UNWCrewMemberAsset`, Plan-Structs (`NWPlanTypes.h`), Konvertierung `NWSimConversion` |
| `Source/NWGame` | Primäres Modul: `ANWGameMode`, `ANWPlayerController`, `ANWCameraPawn` (WASD/Q/E/Mausrad, Platzhalter-Input), `ANWSimulationDirector` (spielt die Sim ab, Debug-Zeichnung, Ghost-Run, Delegate `OnHeistFinished`) |
| `Source/NWUI` | `UNWActivatableScreen` (CommonUI-Basisklasse) |
| `Tools/SimTests` | Standalone-Tests der Sim (CMake), alle 10 grün unter gcc/clang |
| `Config/` | Lumen, VSM, DX12, CommonUI-Viewport, Enhanced Input, Asset-Manager-Typen |

**Wichtig:** Die Unreal-Seite (`NWPlanning`, `NWGame`, `NWUI`, Automation Tests) wurde **noch nie
kompiliert** – in der Cloud gab es keine Engine. Rechne beim ersten Build mit Fehlern und behebe sie.
Die Projektdatei heißt noch `Nachtwerk.uproject` und steht auf `EngineAssociation "5.7"` (Aufgabe 1).

## 3. Checkliste Umgebung (zuerst prüfen)

Prüfe jeden Punkt selbst, soweit möglich. Fehlt etwas, erkläre dem User in wenigen Schritten, was er tun muss,
und warte darauf.

1. **Visual Studio 2022** mit den Workloads *Desktopentwicklung mit C++* und *Spieleentwicklung mit C++*
   (inkl. *IDE-Unterstützung für Unreal Engine*), aktuelles Windows SDK, MSVC v143.
2. **Git + Git LFS** (`git lfs version`), Branch `claude/sharp-rubin-uqa487` ausgecheckt.
3. **Unreal Engine 5.8** installiert (Standardpfad meist `C:\Program Files\Epic Games\UE_5.8`).
4. **MCP-Plugins im Projekt:** `ModelContextProtocol` und `AllToolsets` (werden in Aufgabe 1 in die
   `.uproject` eingetragen; alternativ im Editor unter *Edit → Plugins* aktivieren und neu starten).
5. **MCP-Server im Editor starten** (Konsole unten im Editor; nach jedem Editor-Start erneut, außer
   `bAutoStartServer` ist aktiviert):
   ```
   ModelContextProtocol.StartServer
   ```
   ```
   ModelContextProtocol.GenerateClientConfig ClaudeCode
   ```
   Der Server lauscht auf `localhost:8000/mcp`.
6. **Epic-Plugin für Claude Code** (ist in `.claude/settings.json` vorgemerkt; sonst):
   ```
   /plugin install unreal-engine-skills-for-claude-code@claude-plugins-official
   ```
7. `/mcp` zeigt **`unreal-mcp`** als verbunden. Test: „List all actors in the current level“.

Das MCP-Plugin ist **experimentell**. Es kann Actors, Blueprints, Assets, Data Tables, Sequencer, Tests,
Screenshots und Kamera steuern. **C++ wird nicht über MCP kompiliert** – dafür `Build.bat` (Editor
geschlossen) oder Live Coding im Editor (`Strg+Alt+F11`).

## 4. Aufgaben (in dieser Reihenfolge)

### Aufgabe 1 – Projekt auf `DerClou3` / UE 5.8 umstellen und bauen
- `Nachtwerk.uproject` → `DerClou3.uproject` (`git mv`), `EngineAssociation` → `"5.8"`.
- Plugins `ModelContextProtocol` und `AllToolsets` mit `"TargetAllowList": ["Editor"]` ergänzen.
- `Source/Nachtwerk.Target.cs` → `Source/DerClou3.Target.cs` (Klasse `DerClou3Target`),
  `Source/NachtwerkEditor.Target.cs` → `Source/DerClou3Editor.Target.cs` (Klasse `DerClou3EditorTarget`).
- `Source/NWGame/Private/NWGame.cpp`: dritter Parameter von `IMPLEMENT_PRIMARY_GAME_MODULE` → `"DerClou3"`.
- `Config/DefaultGame.ini`: `ProjectName=der clou 3`.
- Modulnamen `NW*`, `LogNachtwerk` und die Testpfade `Nachtwerk.*` bleiben unverändert.
- README/Doku-Verweise auf `Nachtwerk.uproject` anpassen (`grep -r "Nachtwerk.uproject\|NachtwerkTarget\|NachtwerkEditor"`).
- Bauen (Editor geschlossen):
  ```
  "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" DerClou3Editor Win64 Development -Project="%CD%\DerClou3.uproject" -WaitMutex
  ```
- **Fertig, wenn** der Build fehlerfrei durchläuft und sich `DerClou3.uproject` ohne Rückfragen öffnen lässt.
  Kompilierfehler in den ungetesteten UE-Klassen hier beheben (Typische Kandidaten: Signaturen von
  `DrawDebug*`, `EAutomationTestFlags`, `FInputModeGameAndUI`, Include-Pfade).

### Aufgabe 2 – Tests grün
- Standalone (Developer PowerShell für VS oder Git Bash):
  ```
  cmake -S Tools/SimTests -B Tools/SimTests/build
  ```
  ```
  cmake --build Tools/SimTests/build --config Release
  ```
  ```
  ctest --test-dir Tools/SimTests/build -C Release --output-on-failure
  ```
- In der Engine: Automation Tests `Nachtwerk.Core.Sim.*` über das MCP-Test-Toolset ausführen, oder:
  ```
  "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\DerClou3.uproject" -ExecCmds="Automation RunTests Nachtwerk.Core.Sim; Quit" -unattended -nullrhi -nosplash
  ```
- **Fertig, wenn** beide Testläufe grün sind.

### Aufgabe 3 – Graybox-Map über MCP
- Editor öffnen, MCP-Server starten (Checkliste Punkt 5).
- Über MCP: neue leere Map `/Game/NW/Maps/Graybox_Jeweler`, Boden (Plane/Cube, ca. 3000×2000 cm),
  Directional Light + Sky Atmosphere (oder Nachtstimmung), einen `NWSimulationDirector` bei (0,0,0).
  Map speichern, als Editor-Startmap und `GameDefaultMap` eintragen (`Config/DefaultEngine.ini`).
- PIE starten, Screenshot über MCP machen und dem User zeigen.
- **Fertig, wenn** die Debug-Darstellung (Knoten, Türen, Crew blau, Wache gelb) sichtbar ist und
  `LogNachtwerk` am Ende „Heist finished: CleanGetaway … loot 85000“ meldet.

### Aufgabe 4 – Juwelier als Data Assets
- In `/Game/NW/Data`: `DA_Object_Jeweler` (`UNWHeistObjectAsset`) und `DA_Crew_Vic`, `DA_Crew_Marta`,
  `DA_Crew_Otto` (`UNWCrewMemberAsset`) mit genau den Werten aus `MakeJewelerScenario()` anlegen
  (Zeiten in Sekunden, Geschwindigkeiten in cm/s = cm/Tick × 10). Den Plan im Director als `Plan`-Array eintragen,
  `bUseBuiltInJewelerScenario = false`.
- **Fertig, wenn** `RunGhostPreview` mit Seed 42 ebenfalls CleanGetaway mit Beute 85000 liefert.
  Optional: Automation Test, der Asset-Konvertierung und Built-in-Szenario vergleicht.

### Aufgabe 5 – Graybox-Geometrie
- Wände, Türöffnungen und Räume des Juweliers mit Würfeln/Modeling Tools passend zu den Knotenkoordinaten
  (Gasse, Flur, Keller, Laden, Büro; Raster 50 cm, Wandhöhe 300 cm – siehe `docs/04`) bauen.
- Screenshot von oben zeigen.

### Aufgabe 6 – Nächstes Feature abstimmen
- Mit dem User besprechen, ob als Nächstes die **Planer-Zeitleiste** (`NWUI`, siehe `docs/02` Abschnitt 7)
  oder echte Wach-Interaktion (Türen für Wachen, StateTree) kommt. Erst nach Zustimmung beginnen.

## 5. Arbeitsregeln

- **Sprache:** mit dem User Deutsch; Code, Bezeichner und Code-Kommentare Englisch, Doku Deutsch.
- **Simulation:** Alles unter `Source/NWCore/*/Sim` bleibt **reines C++ ohne Unreal-Header**. Kein
  `FMath::Rand`/`std::rand` in der Sim – Zufall nur über `nw::sim::Random`. Integer statt Float.
  Nach jeder Änderung an der Sim `Tools/SimTests` laufen lassen; neue Mechanik = neuer Test.
- **Unreal-Typen** erst in `NWPlanning`/`NWGame`/`NWUI`; Übersetzung über `NWSimConversion`.
- **Assets:** alles unter `/Game/NW/…`, Präfixe nach `docs/04-pipeline-blender-unreal.md`
  (`SM_`, `BP_`, `DA_`, `M_`, `MI_` …). Binärdateien laufen über Git LFS (`.gitattributes`).
- **MCP-Sicherheit:** vor längeren MCP-Sitzungen committen. Nicht mit `--dangerously-skip-permissions`
  arbeiten (das Plugin kann Python im Editor ausführen). Keine Assets löschen, ohne den User zu fragen.
- **Git:** Branch `claude/sharp-rubin-uqa487`, Commit-Nachrichten Deutsch, nach jeder abgeschlossenen
  Aufgabe committen und pushen. Keinen PR ohne Auftrag.
- **Berichten:** nach jeder Aufgabe kurz zusammenfassen, was fertig ist, was nicht geprüft wurde,
  und einen Screenshot über MCP zeigen, wenn es etwas zu sehen gibt.

## 6. Wegweiser

| Frage | Datei |
|---|---|
| Was ist das Spiel? | `docs/01-vision.md`, `docs/02-game-design.md` |
| Wie ist der Code aufgebaut? | `docs/03-technik-unreal.md`, `Source/NWCore/Public/Sim/Simulation.h` |
| Wie kommen Assets aus Blender? | `docs/04-pipeline-blender-unreal.md` |
| Wie soll es aussehen/klingen? | `docs/05-art-und-audio.md` |
| Was kommt wann? | `docs/06-roadmap.md` |
| Referenz-Coup (Werte) | `Source/NWCore/Private/Sim/SimScenarios.cpp` |
| Sim in der Welt abspielen | `Source/NWGame/Private/NWSimulationDirector.cpp` |
