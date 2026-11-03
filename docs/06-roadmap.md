# 06 – Roadmap, Team & Risiken

Ausgelegt auf ein **Kleinteam von 1–5 Personen**. Gesamtdauer realistisch **2,5–3,5 Jahre** bis Release.

## Phasen

| Phase | Dauer | Ziel | Ergebnis |
|---|---|---|---|
| 0 – Vorproduktion | 2–3 Monate | Kernmechanik beweisen | GDD, Papier-Prototyp, Graybox-Simulation |
| 1 – Vertical Slice | 6–9 Monate | Finale Qualität für einen Coup | Spielbare Demo „Juwelier“, Pitch-Deck, Trailer |
| 2 – Produktion | 12–18 Monate | Inhalt bauen | Alle Objekte, Hub, Story, Progression |
| 3 – Alpha → Beta | 4–6 Monate | Feature-Komplett, Balancing | Steam-Demo, Next Fest, Beta-Tests |
| 4 – Release & Post-Launch | fortlaufend | Patches, ggf. DLC | 1.0, Updates |

## Phase 0 – Vorproduktion (Meilensteine)

1. **Papier-Prototyp des Planers:** Grundriss auf Papier, Zeitleiste als Streifen, Wachen als Spielfiguren.
   Fünf Testspieler planen den Juwelier. Frage: Macht Planen allein Spaß?
2. **Graybox-Simulation in UE:** `NWCore` mit Tick, Graph-Navigation, einer Wache, Türen, Alarm,
   simplem Zeitleisten-Widget. Geisterlauf funktioniert.
3. **Determinismus-Test:** 1.000 Läufe desselben Plans → identischer End-Hash.
4. **Look-Test:** Ein Raum + Straßenstück in Finalqualität (Regen, Lumen, Puppenhaus-Ausblendung).

**Abbruchkriterium:** Wenn der Papier- und Graybox-Prototyp nicht trägt, wird die Planungsmechanik
überarbeitet, bevor auch nur ein finales Asset gebaut wird.

## Phase 1 – Vertical Slice (Abnahmekriterien)

- Ein vollständiger Loop: Hub (minimal: Kneipe, Hehler, Versteck) → Auskundschaften → Planung →
  Ausführung → Flucht → Hehler → Teilung.
- **Juwelier** in Finalqualität (Art, Licht, Audio), 2 Wachen, 1 Lichtschranke, 1 Tresor, 2 Alarmtypen.
- 3 Crewmitglieder mit vertonten Funksprüchen, 6 Werkzeuge.
- Heist-Film-Replay funktioniert und erzeugt ein teilbares Video.
- 60 fps auf Ziel-Hardware, keine Crashes in 2 Stunden Spielzeit.
- 90-Sekunden-Trailer aus Spielmaterial.

## Team-Rollen (1–5 Personen)

| Rolle | Aufgaben | Bei Solo-Entwicklung |
|---|---|---|
| Game Director / Design | Systeme, Level-Design, Balancing | selbst |
| Gameplay-Programmierung (C++) | `NWCore`, `NWPlanning`, KI, Tools | selbst – Priorität 1 |
| Environment Art (Blender/UE) | Kits, Objekte, Licht | selbst + Fab/Megascans |
| Character/Animation | Crew, Wachen, Interaktions-Animationen | Freelancer |
| UI/UX | Planer, Hub-Menüs | selbst, später Freelancer |
| Audio & Musik | Komposition, Sound-Design, Sprachaufnahme | Freelancer/Studio |
| Writing | Story, Dialoge, Zeitungstexte | selbst oder Freelancer |
| QA / Community | Tests, Discord, Steam | Teilzeit/Community |

**Outsourcing-Prioritäten:** Musik, Charakter-Modelle, Animation, Sprachaufnahme, Lokalisierung.
**Nicht outsourcen:** Simulation, Planer-UX, Art Direction.

## Budget-Rahmen (grobe Orientierung)

| Posten | Kleinteam (3–5) |
|---|---|
| Personal (2,5–3,5 Jahre) | größter Posten, abhängig von Gehältern/Standort |
| Freelancer (Charaktere, Animation, Audio, Musik) | 60–150 Tsd. € |
| Sprachaufnahme DE/EN | 20–50 Tsd. € |
| Lokalisierung weiterer Sprachen | 5–10 Tsd. € pro Sprache |
| Hardware, Software, Lizenzen | 10–30 Tsd. € |
| Marketing (Trailer, Messen, PR) | 30–100 Tsd. € |

Unreal Engine: Lizenzgebühr erst ab hohen Umsätzen (aktuelle Epic-EULA bei Start prüfen).

## Finanzierung

- **Games-Förderung des Bundes** (Prototyp- und Produktionsförderung).
- **Länderförderungen**, z. B. FFF Bayern, Medienboard Berlin-Brandenburg, Film- und Medienstiftung NRW,
  MOIN Filmförderung Hamburg Schleswig-Holstein, MFG Baden-Württemberg.
- **Epic MegaGrants** (UE-Projekte).
- **Publisher** mit dem Vertical Slice pitchen (Taktik-/Strategie-Publisher sind die natürlichen Partner).
- Steam-Wunschliste als Beleg für Nachfrage aufbauen (Ziel vor Release: 100.000+ Wishlists).

## Risikoregister

| Risiko | Wahrscheinlichkeit | Auswirkung | Gegenmaßnahme |
|---|---|---|---|
| Scope wächst (zu viele Objekte/Features) | hoch | hoch | Harte Obergrenze 10 Objekte, Feature-Freeze ab Alpha |
| Planer ist zu kompliziert | mittel | sehr hoch | Papier-Prototyp, frühe Usability-Tests, Tutorial-Objekte |
| Determinismus bricht | mittel | hoch | Integer-Sim, automatisierte Hash-Tests in CI |
| KI-Verhalten unlesbar | mittel | hoch | Klare Zustands-Signale, Debug-Overlays, Playtests |
| Performance auf Steam Deck | mittel | mittel | Früh Profil „Deck“ testen, Lumen-Qualität skalieren |
| Engine-Upgrade bricht Projekt | niedrig | mittel | Version fixieren, Upgrades nur zwischen Meilensteinen |
| Finanzierung bleibt aus | mittel | sehr hoch | Vertical Slice schlank halten, Förderanträge parallel |
| Rechtliche Nähe zum Vorbild | niedrig | hoch | Eigene Marke, Figuren, Texte, Musik; keine Original-Assets |

## Nächste konkrete Schritte

1. Papier-Prototyp des Juweliers bauen und mit 5 Personen testen.
2. ~~UE5-Projekt anlegen (C++-Template), Git LFS einrichten, Module `NWCore` und `NWPlanning` anlegen.~~
   Erledigt, inkl. Graybox-Simulation des Juweliers und Tests.
3. Graybox-Geometrie des Juweliers mit Modeling Tools in UE bauen und mit dem Sim-Graphen abgleichen;
   den Juwelier als `NWHeistObjectAsset` anlegen statt fest im Code.
4. Ersten Look-Test-Raum in Blender modellieren und nach den Regeln aus
   [04 – Pipeline](04-pipeline-blender-unreal.md) importieren.
5. Förderprogramme recherchieren und Fristen für den Prototyp-Antrag eintragen.
