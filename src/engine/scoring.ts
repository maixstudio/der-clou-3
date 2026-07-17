// Bewertet ein Simulationsergebnis: Beutewert, XP, Fahndungsänderung und
// Konsequenzen einer Verhaftung. Rein datengetrieben, ohne Seiteneffekte.

import type { LootItem, SimResult } from './types';
import { clamp } from './types';

export interface JobReport {
  outcome: SimResult['outcome'];
  loot: LootItem[];
  lootValue: number;
  xp: number;
  wantedDelta: number;
  fineDM: number;
  jailDays: number;
  headline: string;
  success: boolean;
}

export function scoreResult(result: SimResult, heistXpBase: number, wantedBefore: number): JobReport {
  const loot = result.lootTaken;
  const lootValue = loot.reduce((s, l) => s + l.value, 0);
  const caught = result.outcome === 'caught';

  // XP: Basiswert + Beuteanteil, Bonus für sauberen Coup, Malus bei Verhaftung
  let xp = heistXpBase + Math.round(lootValue / 60);
  if (result.outcome === 'success') xp = Math.round(xp * 1.4);
  if (caught) xp = Math.round(xp * 0.4);

  // Fahndung: Alarm/Verhaftung treiben sie hoch, sauberer Coup senkt sie leicht
  let wantedDelta = 0;
  if (result.outcome === 'success') wantedDelta = -3;
  else if (result.outcome === 'aborted') wantedDelta = 0;
  else if (result.outcome === 'escaped') wantedDelta = result.leftEvidence ? 14 : 9;
  else if (caught) wantedDelta = 22;

  // Verhaftung: Geldstrafe + Haft skalieren mit bisheriger Fahndung
  let fineDM = 0;
  let jailDays = 0;
  if (caught) {
    fineDM = Math.round(1500 + wantedBefore * 40);
    jailDays = Math.round(3 + wantedBefore / 12);
  }

  const headline = makeHeadline(result, lootValue);
  return {
    outcome: result.outcome,
    loot,
    lootValue,
    xp,
    wantedDelta: clamp(wantedDelta, -10, 40),
    fineDM,
    jailDays,
    headline,
    success: result.outcome === 'success' || result.outcome === 'escaped',
  };
}

function makeHeadline(result: SimResult, value: number): string {
  switch (result.outcome) {
    case 'success':
      return value > 0
        ? `Dreister Coup: Beute im Wert von ${value} DM spurlos verschwunden!`
        : 'Rätselhafter Einbruch – doch nichts gestohlen?';
    case 'escaped':
      return 'Alarm in der Nacht: Täter entkommt der Polizei nur knapp!';
    case 'caught':
      return 'Polizei schnappt Einbrecher auf frischer Tat!';
    case 'aborted':
      return 'Verdächtige Person gesichtet – kein Schaden gemeldet.';
  }
}
