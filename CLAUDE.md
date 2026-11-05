# CLAUDE.md – der clou 3

Taktisches Einbruchs-Planungsspiel in **Unreal Engine 5.8** (C++), eigenständiger geistiger Nachfolger
von „Der Clou!“. **Keine** Namen, Figuren, Texte, Grafiken oder Musik des Originals verwenden – nur
Genre und Mechaniken. Mit dem User **Deutsch** sprechen.

## Stand: Neuanfang (2026-11-05)

Das Projekt wurde bewusst auf ein leeres Gerüst zurückgesetzt. Wir bauen **Schritt für Schritt**,
jeder Schritt muss im Spiel sichtbar und bedienbar sein, bevor der nächste kommt:

1. Ein Raum, eine Tür, eine Figur. Figur anklicken = auswählen, Boden anklicken = hingehen.
2. Danach nach Absprache: Tür-Aktionen (Radialmenü), weitere Türen/Fenster, Zeitleiste, …

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
- Ich darf den Editor fürs Bauen selbst schließen und neu starten – aber nur, wenn über MCP geprüft
  ist, dass nichts ungespeichert ist und kein Play läuft. Sonst den User fragen.
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
