// Charaktermodell und die zentrale Übersetzung Charakter → Engine-ActorProfile.
// Hier laufen Stats, Skills, Items, Perks und Fahrzeug zusammen. Genau ein Ort,
// an dem Perk-Effekte die Simulation beeinflussen (crew-fähig erweiterbar).

import type { ActorProfile, SkillId, StatId, ToolId } from '../engine/types';
import { clamp } from '../engine/types';
import { carryBonus } from './data/items';
import type { PerkId } from './data/perksData';
import { VEHICLE_BY_ID } from './data/vehicles';

export interface Character {
  name: string;
  level: number;
  xp: number;
  skillPoints: number;
  perkSlots: number;
  stats: Record<StatId, number>;
  skills: Record<SkillId, number>;
  perks: PerkId[];
  /** besessene Werkzeuge (Besitz, nicht zwingend im Loadout) */
  owned: ToolId[];
  /** aktuell eingepacktes Loadout für den nächsten Coup */
  loadout: ToolId[];
  vehicles: string[];
  activeVehicle: string;
  stamina: number; // 0..100
}

export const XP_PER_LEVEL = (level: number): number => 100 + (level - 1) * 60;

export function newCharacter(name: string): Character {
  return {
    name,
    level: 1,
    xp: 0,
    skillPoints: 5,
    perkSlots: 1,
    stats: { staerke: 4, geschick: 5, kondition: 5, intelligenz: 4, nerven: 4 },
    skills: { schloesser: 15, safes: 5, elektronik: 5, schleichen: 20, sport: 10, fahren: 15, handeln: 10 },
    perks: [],
    owned: ['dietrich1', 'handschuhe'],
    loadout: ['dietrich1', 'handschuhe'],
    vehicles: ['none'],
    activeVehicle: 'none',
    stamina: 100,
  };
}

/** Fügt XP hinzu und verarbeitet Level-Ups (Skillpunkte + Perk-Slots). */
export function addXp(c: Character, amount: number): { leveledTo: number | null } {
  c.xp += amount;
  let leveledTo: number | null = null;
  while (c.xp >= XP_PER_LEVEL(c.level)) {
    c.xp -= XP_PER_LEVEL(c.level);
    c.level += 1;
    c.skillPoints += 3;
    if (c.level % 2 === 0) c.perkSlots += 1;
    leveledTo = c.level;
  }
  return { leveledTo };
}

const has = (c: Character, p: PerkId): boolean => c.perks.includes(p);

/**
 * Baut das ActorProfile für die Engine. EINZIGER Ort, an dem Perks/Items/Fahrzeug
 * die Simulationsparameter modifizieren.
 */
export function buildProfile(c: Character): ActorProfile {
  const veh = VEHICLE_BY_ID[c.activeVehicle] ?? VEHICLE_BY_ID.none;
  const tools = c.loadout;

  // Bewegungstempo aus Kondition + Perks
  let walkSpeed = 2.6 + c.stats.kondition * 0.12;
  let sneakSpeed = 1.2 + c.stats.kondition * 0.06 + c.skills.schleichen * 0.004;
  if (has(c, 'ausdauernd')) {
    walkSpeed *= 1.12;
    sneakSpeed *= 1.12;
  }

  // Gehlärm aus Schleichen + Turnschuhen + Perk
  let sneakNoise = 2.2 - c.skills.schleichen * 0.012;
  if (tools.includes('turnschuhe')) sneakNoise *= 0.5;
  if (has(c, 'katzenpfoten')) sneakNoise *= 0.65;
  sneakNoise = clamp(sneakNoise, 0.4, 3);

  // Tragkraft: Basis aus Stärke + Perk + Taschen + Fahrzeug
  let carry = 12 + c.stats.staerke * 3 + carryBonus(tools) + veh.capacity;
  if (has(c, 'muskelprotz')) carry += 12;

  const detectionFactor =
    (has(c, 'sechster_sinn') ? 0.8 : 1) * (has(c, 'nachtsicht') ? 0.9 : 1);

  return {
    skills: { ...c.skills },
    stats: { ...c.stats },
    tools: [...tools],
    walkSpeed,
    sneakSpeed,
    walkNoise: clamp(4 - c.skills.schleichen * 0.006, 2, 4),
    sneakNoise,
    carryCapacity: carry,
    detectionFactor,
    lightFloor: has(c, 'nachtsicht') ? 0.05 : 0.1,
    alarmSpeedBonus: (has(c, 'bleifuss') ? 0.25 : 0) + (has(c, 'adrenalin') ? 0.2 : 0),
    kaltbluetig: has(c, 'kaltbluetig'),
    reroll: has(c, 'gluckspilz'),
    elektronikFix: has(c, 'elektronikfreak'),
    safeOhr: has(c, 'safeohr'),
    escapeBonus: veh.escape + c.skills.fahren * 0.003 + (has(c, 'bleifuss') ? 0.1 : 0),
    hasVehicle: veh.bulky,
  };
}

/** Gesamttragkraft (für Loadout-/Inventaranzeige). */
export function totalCarry(c: Character): number {
  return buildProfile(c).carryCapacity;
}

/** Hehler-Faktor aus Handeln-Skill + Feilscher-Perk. */
export function haggleFactor(c: Character): number {
  let f = 0.5 + c.skills.handeln * 0.004; // 0.5 .. 0.9
  if (has(c, 'feilscher')) f += 0.15;
  return f;
}

/** Fahndungsabbau pro Schlaf-Tag. */
export function wantedDecayPerDay(c: Character): number {
  return has(c, 'unauffaellig') ? 6 : 3;
}
