# Der Clou 3 — Einbrecher-Simulation 🕵️

Ein spielbarer Nachfolger des Amiga/DOS-Klassikers **„Der Clou!"** (1994) im Browser.
Ein **Klick-Planer**: Du studierst den Grundriss eines Objekts, klickst Schritt für Schritt
deinen Einbruch zusammen, testest ihn in der **Vorschau** und führst ihn dann nachts *scharf*
aus – mit echten Würfen, patrouillierenden Wachen, Alarmanlagen und Polizei-Countdown.

Drumherum ein vollwertiges Meta-Spiel: **Charakterentwicklung** (Stats, Skills, XP, Level,
Perks), **Inventar & Loadout**, **Shop**, **Garage**, **Training**, **Hehler**, **Informant**,
**Fahndungssystem** und **Speicherstände** – alles im **80er-Jahre-Synthwave-Look**.

## Spielen

```bash
npm install
npm run dev      # Dev-Server (http://localhost:5173)
npm run build    # Produktions-Build (dist/)
npm test         # Engine- und Spiellogik-Tests (Vitest)
```

Es gibt kein Backend. Der Spielstand wird automatisch im `localStorage` gespeichert.

## So spielst du

1. **Neue Verbrecherkarriere** starten und einen Decknamen wählen.
2. Beim **Informanten Kalle** einen Tipp kaufen (schaltet Objekte frei) und optional
   **auskundschaften** (Stufe 1–3 → Grundriss, Wachrouten, Sensorpläne werden sichtbar).
3. Beim **Shop „Werkzeug-Willi"** Werkzeug kaufen (Dietriche, Glasschneider, Stethoskop,
   Drahtschere, Störsender, Kassettenrekorder …) und in der **Ausrüstung** ins Loadout packen.
4. Ein **Objekt** wählen und den **Einbruch planen**:
   - Auf ein Feld / Schloss / Fenster / Safe / Sensor **klicken** öffnet die möglichen Aktionen.
   - Die Wege zwischen den Aktionen werden **automatisch per A\*** berechnet.
   - Warteschritte einfügen, um die Wachen-Patrouillen auszutricksen.
   - Die **Timeline** zeigt die Dauer, die **Vorschau** spielt den Plan ab (Play/Pause/Tempo/Scrubbing).
5. **Ausführen** 🌙 – zusehen, wie der Plan läuft. Wird's brenzlig: **Notabbruch** und ab zum
   Fluchtwagen. Bei **Alarm** zählt der Polizei-Countdown.
6. Im **Debriefing** gibt's Beute, XP und die Zeitungsmeldung. Beute beim **Hehler** zu Geld
   machen (heiße Ware erst abkühlen lassen!), **trainieren**, **Level-Ups** mit Skillpunkten
   und **Perks** ausgeben – und den nächsten, größeren Coup angehen.

## Objekte (steigende Schwierigkeit)

Späti (Tutorial) → Videothek → Villa → Elektronikladen → Juwelier → Spielhalle → Museum →
**Stadtbank (Finale)**. Jedes Objekt bringt neue Gefahren (Wachen, Bewegungsmelder,
schwenkende Kameras, Druckplatten, Funkalarm, Panzer-Tresore).

## Architektur

```
src/
  engine/     Reine, deterministische TS-Simulation (React-frei, seedbarer RNG)
    types · rng · grid (A*) · los (Sichtlinien/Kegel/Licht) · noise (Lärm)
    guards (Wachen-KI) · actions (Aktionsformeln) · planner (Plan→Pfade/Zeiten)
    simulate (Vorschau & Ausführung in EINER Funktion) · scoring
  game/       Meta-Spiel: character · economy · state (Zustand-Store, persistiert)
    data/     items · vehicles · perksData · training · heists (8 Grundrisse als ASCII)
  ui/         React-Screens, Canvas-Renderer (canvas/render.ts) und Playback-Hook
```

**Kern-Prinzip:** Vorschau und Ausführung nutzen dieselbe `simulate()`-Funktion – nur der
RNG-Modus (Erwartungswerte vs. echte Würfe) und der Wissensstand (ausgekundschaftete Wachen/
Sensoren, „Fog of Planning") unterscheiden sich. Alle Charakterwerte, Perks, Werkzeuge und das
Fahrzeug fließen über `buildProfile()` (in `game/character.ts`) zentral als `ActorProfile` in
die Engine – die Architektur ist bereits **crew-fähig** (mehrere Akteure) angelegt, V1 spielt
solo.

Da die Engine mit einem **seedbaren RNG** (mulberry32) arbeitet, ist jede Ausführung
reproduzierbar und testbar (`src/engine/engine.test.ts`, `src/game/*.test.ts`).

## Setting

Bundesrepublik, Mitte der 80er: frühe Elektronik-Alarmanlagen, Kassettenrekorder als
Ablenkung, klapprige Fluchtwagen vom Kadett bis zum unscheinbaren alten Benz. DM statt Euro.
