// Trainingsangebote. Jede Einheit kostet Geld + Tage und hebt einen Skill oder
// Stat mit abnehmendem Ertrag (je höher der Wert, desto kleiner der Zuwachs).

import type { SkillId, StatId } from '../../engine/types';

export interface TrainingDef {
  id: string;
  name: string;
  icon: string;
  desc: string;
  cost: number;
  days: number;
  target: { kind: 'skill'; id: SkillId } | { kind: 'stat'; id: StatId };
  /** Basiszuwachs bei niedrigem Ausgangswert */
  gain: number;
}

export const TRAININGS: TrainingDef[] = [
  { id: 't_lock', name: 'Schlossknack-Übungsbrett', icon: '🔐', desc: 'Übe an Schlössern aller Art.', cost: 200, days: 2, target: { kind: 'skill', id: 'schloesser' }, gain: 12 },
  { id: 't_safe', name: 'Safeknacker-Kurs', icon: '🔓', desc: 'Ein alter Meister zeigt dir seine Tricks.', cost: 350, days: 3, target: { kind: 'skill', id: 'safes' }, gain: 12 },
  { id: 't_elec', name: 'VHS-Elektronikkurs', icon: '📺', desc: 'Alarmanlagen verstehen und überlisten.', cost: 300, days: 3, target: { kind: 'skill', id: 'elektronik' }, gain: 12 },
  { id: 't_sneak', name: 'Schleich-Training', icon: '🥷', desc: 'Lautlos bewegen wie ein Schatten.', cost: 180, days: 2, target: { kind: 'skill', id: 'schleichen' }, gain: 12 },
  { id: 't_sport', name: 'Kletterhalle', icon: '🧗', desc: 'Fassaden und Fenster meistern.', cost: 160, days: 2, target: { kind: 'skill', id: 'sport' }, gain: 12 },
  { id: 't_drive', name: 'Fahrsicherheitstraining', icon: '🏎️', desc: 'Fluchtfahrten üben.', cost: 240, days: 2, target: { kind: 'skill', id: 'fahren' }, gain: 12 },
  { id: 't_deal', name: 'Verhandlungsseminar', icon: '💬', desc: 'Bessere Preise beim Hehler herausholen.', cost: 220, days: 2, target: { kind: 'skill', id: 'handeln' }, gain: 12 },
  { id: 't_str', name: 'Kraftraum', icon: '🏋️', desc: 'Muskeln für schwere Beute und Türen.', cost: 300, days: 3, target: { kind: 'stat', id: 'staerke' }, gain: 1 },
  { id: 't_dex', name: 'Fingerfertigkeits-Drill', icon: '✋', desc: 'Ruhige Hände für Feinarbeit.', cost: 300, days: 3, target: { kind: 'stat', id: 'geschick' }, gain: 1 },
  { id: 't_con', name: 'Ausdauerlauf', icon: '🏃', desc: 'Kondition für lange Nächte.', cost: 250, days: 2, target: { kind: 'stat', id: 'kondition' }, gain: 1 },
  { id: 't_int', name: 'Fachbücher wälzen', icon: '📚', desc: 'Köpfchen für knifflige Anlagen.', cost: 350, days: 3, target: { kind: 'stat', id: 'intelligenz' }, gain: 1 },
  { id: 't_nrv', name: 'Meditationskurs', icon: '🧘', desc: 'Nerven wie Drahtseile.', cost: 300, days: 3, target: { kind: 'stat', id: 'nerven' }, gain: 1 },
];

/** Abnehmender Ertrag: bei Skills 0..100, bei Stats 1..10. */
export function trainingResult(t: TrainingDef, current: number): number {
  if (t.target.kind === 'skill') {
    const factor = 1 - current / 110; // je höher, desto weniger
    return Math.max(1, Math.round(t.gain * Math.max(0.2, factor)));
  }
  // Stats: linear teurer, harte Kappe bei 10
  if (current >= 10) return 0;
  return 1;
}
