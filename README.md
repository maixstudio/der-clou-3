# der clou 3

Taktisches Einbruchs-Planungsspiel in Unreal Engine 5.8 – ein eigenständiger geistiger Nachfolger
des Genres der Einbruchs-Planungsspiele der 90er.

**Stand:** Neuanfang. Das Projekt ist ein leeres Gerüst und wächst Schritt für Schritt
(zuerst: ein Raum, eine Tür, eine Figur). Der frühere Prototyp liegt im Git-Archiv
`archiv/prototyp-1`.

## Einrichten

Voraussetzungen: Unreal Engine 5.8, Visual Studio 2022 (17.14) mit C++- und Spiele-Workload,
Git LFS.

```bash
git lfs install
git clone https://github.com/maixstudio/der-clou-3.git
```

Bauen (Editor geschlossen):

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" DerClou3Editor Win64 Development -Project="C:\Dev\der-clou-3\DerClou3.uproject" -WaitMutex
```

Danach `DerClou3.uproject` öffnen.
