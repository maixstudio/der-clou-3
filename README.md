# NACHTWERK *(Arbeitstitel)*

**Ein taktisches Einbruchsspiel für PC – planen, timen, durchziehen.**
Gebaut mit **Unreal Engine 5** und **Blender** von einem kleinen Team mit dem Anspruch an AAA-Look & Feel.

> Du stellst eine Crew zusammen, kundschaftest ein Objekt aus, planst den Einbruch Sekunde für Sekunde
> auf einer Zeitleiste – und siehst dann zu, wie dein Plan in einer Regennacht des Jahres 1972
> funktioniert … oder eben nicht.

## Dokumentation

| Dokument | Inhalt |
|---|---|
| [01 – Vision](docs/01-vision.md) | Pitch, Design-Säulen, USP, Zielgruppe |
| [02 – Game Design](docs/02-game-design.md) | Kernloop, Systeme, Beispiel-Coup |
| [03 – Technik (Unreal)](docs/03-technik-unreal.md) | Architektur, Simulation, KI, Kamera, UI |
| [04 – Pipeline Blender → Unreal](docs/04-pipeline-blender-unreal.md) | Konventionen, Export, Versionierung |
| [05 – Art & Audio](docs/05-art-und-audio.md) | Art Direction, Licht, Charaktere, Sound |
| [06 – Roadmap](docs/06-roadmap.md) | Phasen, Vertical Slice, Team, Budget, Risiken |

## Status

Vorproduktion. Das UE5-Projektgerüst steht, die deterministische Simulation läuft als
Graybox mit dem Juwelier-Szenario aus dem Game Design.

## Loslegen

**Voraussetzungen:** Unreal Engine 5 (das Projekt ist auf 5.7 eingestellt, siehe unten),
Visual Studio 2022 bzw. Rider mit C++-Workload, Git LFS.

```bash
git lfs install
git clone https://github.com/maixstudio/der-clou-3.git
```

1. Rechtsklick auf `Nachtwerk.uproject` → *Generate Visual Studio project files*.
   Ist eine andere UE-Version installiert: Rechtsklick → *Switch Unreal Engine version…*
2. `Nachtwerk.sln` öffnen, Konfiguration **Development Editor**, Projekt `Nachtwerk` bauen und starten.
3. Im Editor eine leere Map anlegen (`/Game/NW/Maps/Graybox_Jeweler`), einen
   **NWSimulationDirector** in die Szene ziehen und **Play** drücken.
   Kamera: WASD schwenken, Q/E drehen, Mausrad zoomen. Das Event-Log erscheint im Output Log
   unter `LogNachtwerk`.

**Simulationstests ohne Unreal** (laufen auch in GitHub Actions):

```bash
cmake -S Tools/SimTests -B Tools/SimTests/build
```

```bash
cmake --build Tools/SimTests/build
```

```bash
ctest --test-dir Tools/SimTests/build --output-on-failure
```

Im Editor laufen dieselben Prüfungen als Automation Tests unter `Nachtwerk.Core.Sim.*`.

## Projektstruktur

```
Nachtwerk.uproject
Config/                 Engine-, Spiel- und Input-Einstellungen
Content/NW/             Unreal-Assets (Git LFS)
Source/
  NWCore/               Deterministische Simulation (reines C++ unter Sim/) + Automation Tests
  NWPlanning/           Data Assets (Objekte, Crew), Plan-Strukturen, Konvertierung in die Simulation
  NWGame/               Primäres Spielmodul: GameMode, Kamera, NWSimulationDirector
  NWUI/                 CommonUI-Basisklassen
SourceArt/              Blender-/Substance-Quelldateien (Git LFS)
Tools/SimTests/         Standalone-Tests der Simulation (CMake)
docs/                   Konzept und Design
```

## Hinweis zu Inspiration und Rechten

NACHTWERK ist ein **eigenständiges Werk**. Es greift das Genre der Einbruchs-Planungsspiele auf, das in den
1990ern unter anderem durch „Der Clou!“ (neo Software, 1994) geprägt wurde. Name, Figuren, Schauplätze,
Grafiken, Texte und Musik des Originals werden **nicht** verwendet; Spielmechaniken als solche
(Crew anwerben, planen, ausführen, Beute verkaufen) sind Genre-Konventionen.
