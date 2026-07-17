// Aktionskatalog: Für jede klickbare Aktion Dauer-, Lärm- und Fehlschlag-Formeln.
// Alle Formeln lesen ausschließlich aus dem ActorProfile (crew-fähig).

import type { ActionId, ActorProfile, ToolId } from './types';
import { clamp } from './types';

export type TargetKind =
  | 'door'
  | 'window'
  | 'climb'
  | 'safe'
  | 'vitrine'
  | 'loot'
  | 'sensor'
  | 'camera'
  | 'fusebox'
  | 'phone'
  | 'tile'
  | 'none';

export interface ActionCtx {
  profile: ActorProfile;
  /** Schloss-/Safestufe des Ziels */
  level?: number;
  bulky?: boolean;
  waitTime?: number;
}

export interface ActionSpec {
  id: ActionId;
  label: string;
  targetKind: TargetKind;
  tool?: ToolId;
  /** null = verfügbar, sonst deutscher Hinderungsgrund */
  available(p: ActorProfile, level?: number): string | null;
  duration(ctx: ActionCtx): number;
  noise(ctx: ActionCtx): number;
  failChance(ctx: ActionCtx): number;
  /** zusätzlicher Lärmradius bei Fehlschlag */
  failNoise: number;
  xp(level?: number): number;
}

const has = (p: ActorProfile, t: ToolId): boolean => p.tools.includes(t);

/** Beste Dietrich-Stufe im Gepäck (0 = keiner). */
export function dietrichTier(p: ActorProfile): number {
  if (has(p, 'dietrich3')) return 3;
  if (has(p, 'dietrich2')) return 2;
  if (has(p, 'dietrich1')) return 1;
  return 0;
}

/** 1.6 (Skill 0) … 0.6 (Skill 100) – Tempofaktor aus Skill */
const skillF = (s: number): number => 1.6 - s / 100;
/** Tempofaktor aus Geschick (Stat 1..10) */
const dexF = (p: ActorProfile): number => 1.3 - p.stats.geschick * 0.055;
const elekF = (p: ActorProfile): number =>
  skillF(p.skills.elektronik) * (p.elektronikFix ? 0.75 : 1);

export const ACTIONS: Record<Exclude<ActionId, 'moveTo' | 'wait'>, ActionSpec> = {
  openDoor: {
    id: 'openDoor',
    label: 'Tür öffnen',
    targetKind: 'door',
    available: () => null,
    duration: () => 1,
    noise: () => 1,
    failChance: () => 0,
    failNoise: 0,
    xp: () => 0,
  },
  pickLock: {
    id: 'pickLock',
    label: 'Schloss knacken',
    targetKind: 'door',
    available: (p, level = 1) =>
      dietrichTier(p) >= level ? null : `Dietrich Stufe ${level} nötig`,
    duration: ({ profile: p, level = 1 }) =>
      (4 + 5 * level) * skillF(p.skills.schloesser) * dexF(p),
    noise: () => 1.5,
    failChance: ({ profile: p, level = 1 }) =>
      clamp(
        0.04 + 0.16 * level - p.skills.schloesser * 0.005 - (dietrichTier(p) - level) * 0.06,
        0.02,
        0.85,
      ),
    failNoise: 2,
    xp: (level = 1) => 6 * level,
  },
  forceDoor: {
    id: 'forceDoor',
    label: 'Tür aufbrechen',
    targetKind: 'door',
    tool: 'brecheisen',
    available: (p, level = 1) => {
      if (!has(p, 'brecheisen')) return 'Brecheisen nötig';
      if (level > 2) return 'Schloss zu massiv fürs Brecheisen';
      return null;
    },
    duration: ({ profile: p }) => 5 * (1.5 - p.stats.staerke * 0.07),
    noise: () => 9,
    failChance: () => 0.05,
    failNoise: 3,
    xp: () => 3,
  },
  cutWindow: {
    id: 'cutWindow',
    label: 'Fenster aufschneiden',
    targetKind: 'window',
    tool: 'glasschneider',
    available: (p) => (has(p, 'glasschneider') ? null : 'Glasschneider nötig'),
    duration: ({ profile: p }) => 7 * dexF(p),
    noise: () => 1,
    failChance: ({ profile: p }) => clamp(0.1 - p.stats.geschick * 0.008, 0.02, 0.3),
    failNoise: 4,
    xp: () => 4,
  },
  smashWindow: {
    id: 'smashWindow',
    label: 'Fenster einschlagen',
    targetKind: 'window',
    available: () => null,
    duration: () => 2,
    noise: () => 11,
    failChance: () => 0,
    failNoise: 0,
    xp: () => 1,
  },
  climb: {
    id: 'climb',
    label: 'Hochklettern (Seil)',
    targetKind: 'climb',
    tool: 'seil',
    available: (p) => (has(p, 'seil') ? null : 'Seil mit Haken nötig'),
    duration: ({ profile: p }) => 6 * (1.7 - p.skills.sport / 100),
    noise: () => 2,
    failChance: ({ profile: p }) => clamp(0.22 - p.skills.sport * 0.002, 0.02, 0.3),
    failNoise: 6,
    xp: () => 5,
  },
  crackSafe: {
    id: 'crackSafe',
    label: 'Safe knacken (Gehör)',
    targetKind: 'safe',
    tool: 'stethoskop',
    available: (p) => (has(p, 'stethoskop') ? null : 'Stethoskop nötig'),
    duration: ({ profile: p, level = 1 }) =>
      15 * level * (1.75 - p.skills.safes / 100) * (p.safeOhr ? 0.7 : 1),
    noise: () => 0.5,
    failChance: ({ profile: p, level = 1 }) =>
      clamp(0.06 + 0.22 * level - p.skills.safes * 0.006, 0.03, 0.9),
    failNoise: 1,
    xp: (level = 1) => 12 * level,
  },
  drillSafe: {
    id: 'drillSafe',
    label: 'Safe aufbohren',
    targetKind: 'safe',
    tool: 'bohrmaschine',
    available: (p, level = 1) => {
      if (!has(p, 'bohrmaschine')) return 'Bohrmaschine nötig';
      if (level > 2) return 'Panzerung zu stark für die Bohrmaschine';
      return null;
    },
    duration: ({ level = 1 }) => 11 * level,
    noise: () => 8,
    failChance: ({ profile: p }) => clamp(0.12 - p.skills.safes * 0.001, 0.04, 0.2),
    failNoise: 3,
    xp: (level = 1) => 8 * level,
  },
  grindSafe: {
    id: 'grindSafe',
    label: 'Safe aufflexen',
    targetKind: 'safe',
    tool: 'winkelschleifer',
    available: (p) => (has(p, 'winkelschleifer') ? null : 'Winkelschleifer nötig'),
    duration: ({ level = 1 }) => 7 * level,
    noise: () => 12,
    failChance: () => 0.04,
    failNoise: 4,
    xp: (level = 1) => 6 * level,
  },
  cutVitrine: {
    id: 'cutVitrine',
    label: 'Vitrine aufschneiden',
    targetKind: 'vitrine',
    tool: 'glasschneider',
    available: (p) => (has(p, 'glasschneider') ? null : 'Glasschneider nötig'),
    duration: ({ profile: p }) => 6 * dexF(p),
    noise: () => 1,
    failChance: ({ profile: p }) => clamp(0.08 - p.stats.geschick * 0.006, 0.02, 0.25),
    failNoise: 4,
    xp: () => 4,
  },
  smashVitrine: {
    id: 'smashVitrine',
    label: 'Vitrine einschlagen',
    targetKind: 'vitrine',
    available: () => null,
    duration: () => 1.5,
    noise: () => 10,
    failChance: () => 0,
    failNoise: 0,
    xp: () => 1,
  },
  grabLoot: {
    id: 'grabLoot',
    label: 'Beute einpacken',
    targetKind: 'loot',
    available: () => null,
    duration: ({ bulky }) => (bulky ? 6 : 2.5),
    noise: ({ bulky }) => (bulky ? 2 : 0.5),
    failChance: () => 0,
    failNoise: 0,
    xp: () => 2,
  },
  disableSensor: {
    id: 'disableSensor',
    label: 'Sensor deaktivieren',
    targetKind: 'sensor',
    tool: 'drahtschere',
    available: (p) => (has(p, 'drahtschere') ? null : 'Drahtschere nötig'),
    duration: ({ profile: p }) => 6 * elekF(p),
    noise: () => 1,
    failChance: ({ profile: p }) =>
      clamp(0.32 - p.skills.elektronik * 0.0035 - (p.elektronikFix ? 0.08 : 0), 0.03, 0.5),
    failNoise: 1,
    xp: () => 8,
  },
  disableCamera: {
    id: 'disableCamera',
    label: 'Kamera manipulieren',
    targetKind: 'camera',
    tool: 'loetkolben',
    available: (p) => (has(p, 'loetkolben') ? null : 'Lötkolben-Set nötig'),
    duration: ({ profile: p }) => 8 * elekF(p),
    noise: () => 1,
    failChance: ({ profile: p }) =>
      clamp(0.3 - p.skills.elektronik * 0.0035 - (p.elektronikFix ? 0.08 : 0), 0.03, 0.5),
    failNoise: 1,
    xp: () => 8,
  },
  cutPower: {
    id: 'cutPower',
    label: 'Strom kappen',
    targetKind: 'fusebox',
    tool: 'drahtschere',
    available: (p) => (has(p, 'drahtschere') ? null : 'Drahtschere nötig'),
    duration: () => 5,
    noise: () => 2,
    failChance: () => 0.05,
    failNoise: 2,
    xp: () => 5,
  },
  cutPhone: {
    id: 'cutPhone',
    label: 'Telefonleitung kappen',
    targetKind: 'phone',
    tool: 'drahtschere',
    available: (p) => (has(p, 'drahtschere') ? null : 'Drahtschere nötig'),
    duration: () => 4,
    noise: () => 1,
    failChance: () => 0.02,
    failNoise: 1,
    xp: () => 5,
  },
  placeDistraction: {
    id: 'placeDistraction',
    label: 'Kassettenrekorder abstellen',
    targetKind: 'tile',
    tool: 'rekorder',
    available: (p) => (has(p, 'rekorder') ? null : 'Kassettenrekorder nötig'),
    duration: () => 2,
    noise: () => 0.5,
    failChance: () => 0,
    failNoise: 0,
    xp: () => 2,
  },
};

/** Verzögerung und Dauer der Rekorder-Ablenkung */
export const DISTRACTION_DELAY = 15;
export const DISTRACTION_LENGTH = 12;
export const DISTRACTION_RADIUS = 9;

export const ACTION_LABELS: Record<ActionId, string> = {
  moveTo: 'Gehen',
  wait: 'Warten',
  ...Object.fromEntries(
    Object.values(ACTIONS).map((a) => [a.id, a.label]),
  ),
} as Record<ActionId, string>;
