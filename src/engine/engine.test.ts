import { describe, expect, it } from 'vitest';
import { makeRng } from './rng';
import { parseHeist, type HeistDef } from './parse';
import { findPath, walkable } from './grid';
import { lineOfSight, inCone, lightAt } from './los';
import { noiseAt } from './noise';
import { simulate } from './simulate';
import { compilePlan } from './planner';
import type { ActorProfile, Plan } from './types';
import { idx } from './types';

const testDef: HeistDef = {
  id: 'test',
  name: 'Testhaus',
  desc: 'Kleiner Testgrundriss',
  difficulty: 1,
  unlockCost: 0,
  intelCosts: [0, 0, 0],
  policeBase: 40,
  baseLight: 0.3,
  xpBase: 20,
  // 10x7 Grundriss: Außen unten, Eingangstür (Stufe 1), Kasse K, Fluchtpunkt E
  map: [
    '##########',
    '#....K...#',
    '#........#',
    '#........#',
    '#..f.....#',
    '####1#####',
    '   ,E,    ',
  ],
  legend: {
    K: { obj: 'loot', name: 'Kasse', icon: '💵', value: 800, weight: 3 },
  },
  guards: [],
};

const profile: ActorProfile = {
  skills: { schloesser: 50, safes: 30, elektronik: 20, schleichen: 40, sport: 30, fahren: 30, handeln: 30 },
  stats: { staerke: 5, geschick: 5, kondition: 5, intelligenz: 5, nerven: 5 },
  tools: ['dietrich1', 'handschuhe'],
  walkSpeed: 3,
  sneakSpeed: 1.6,
  walkNoise: 3,
  sneakNoise: 1,
  carryCapacity: 30,
  detectionFactor: 1,
  lightFloor: 0.1,
  alarmSpeedBonus: 0,
  kaltbluetig: false,
  reroll: false,
  elektronikFix: false,
  safeOhr: false,
  escapeBonus: 0.2,
  hasVehicle: true,
};

describe('rng', () => {
  it('ist deterministisch für gleiche seeds', () => {
    const a = makeRng(123);
    const b = makeRng(123);
    const seqA = [a.next(), a.next(), a.next()];
    const seqB = [b.next(), b.next(), b.next()];
    expect(seqA).toEqual(seqB);
  });
  it('liefert Werte in [0,1)', () => {
    const r = makeRng(7);
    for (let i = 0; i < 100; i++) {
      const v = r.next();
      expect(v).toBeGreaterThanOrEqual(0);
      expect(v).toBeLessThan(1);
    }
  });
});

describe('parse', () => {
  it('liest Karte, Tür, Beute und Fluchtpunkt', () => {
    const map = parseHeist(testDef);
    expect(map.w).toBe(10);
    expect(map.h).toBe(7);
    expect(map.exit).toEqual({ x: 4, y: 6 });
    expect(map.loot).toHaveLength(1);
    expect(map.loot[0].value).toBe(800);
    // Tür bei y=5 x=4, Schlossstufe 1
    const door = map.tiles[idx({ x: 4, y: 5 }, map.w)];
    expect(door.kind).toBe('door');
    expect(door.lockLevel).toBe(1);
  });
});

describe('grid / pathfinding', () => {
  const map = parseHeist(testDef);
  it('walkable respektiert verschlossene Türen', () => {
    expect(walkable(map, { x: 4, y: 5 }, new Set())).toBe(false);
    const opened = new Set([idx({ x: 4, y: 5 }, map.w)]);
    expect(walkable(map, { x: 4, y: 5 }, opened)).toBe(true);
  });
  it('findet keinen Weg durch verschlossene Tür ohne passDoors', () => {
    const p = findPath(map, { x: 4, y: 6 }, { x: 4, y: 1 }, {});
    expect(p).toBeNull();
  });
  it('findet Weg durch geöffnete Tür', () => {
    const opened = new Set([idx({ x: 4, y: 5 }, map.w)]);
    const p = findPath(map, { x: 4, y: 6 }, { x: 4, y: 1 }, { opened });
    expect(p).not.toBeNull();
    expect(p![0]).toEqual({ x: 4, y: 6 });
    expect(p![p!.length - 1]).toEqual({ x: 4, y: 1 });
  });
});

describe('los', () => {
  const map = parseHeist(testDef);
  it('Wände blockieren Sicht', () => {
    // von Kasse (x5,y1) durch Wand nach außen (x4,y6)
    expect(lineOfSight(map, { x: 5, y: 1 }, { x: 4, y: 6 }, new Set())).toBe(false);
  });
  it('freie Sicht im Raum', () => {
    expect(lineOfSight(map, { x: 1, y: 1 }, { x: 7, y: 3 }, new Set())).toBe(true);
  });
  it('Kegel erkennt Ziel in Blickrichtung', () => {
    expect(inCone({ x: 0, y: 0 }, 0, Math.PI / 2, 5, { x: 3, y: 0 })).toBe(true);
    expect(inCone({ x: 0, y: 0 }, 0, Math.PI / 2, 5, { x: 0, y: 4 })).toBe(false);
  });
  it('Licht: Basislicht ohne Lampen', () => {
    expect(lightAt(map, { x: 5, y: 2 }, false)).toBeCloseTo(0.3);
  });
});

describe('noise', () => {
  const map = parseHeist(testDef);
  it('Lärm nimmt mit Distanz ab', () => {
    const near = noiseAt(map, { x: 2, y: 2 }, 8, { x: 3, y: 2 }, new Set());
    const far = noiseAt(map, { x: 2, y: 2 }, 8, { x: 7, y: 2 }, new Set());
    expect(near).toBeGreaterThan(far);
  });
  it('Wände dämpfen Lärm', () => {
    const open = noiseAt(map, { x: 4, y: 4 }, 10, { x: 4, y: 2 }, new Set());
    const through = noiseAt(map, { x: 4, y: 4 }, 10, { x: 4, y: 6 }, new Set());
    expect(open).toBeGreaterThan(through);
  });
});

function tutorialPlan(map: ReturnType<typeof parseHeist>): Plan {
  const loot = map.loot[0];
  return {
    heistId: 'test',
    steps: [
      { id: 's1', action: 'pickLock', target: { x: 4, y: 5 }, tool: 'dietrich1', mode: 'sneak' },
      { id: 's2', action: 'grabLoot', targetId: loot.id, mode: 'sneak' },
    ],
  };
}

describe('simulate', () => {
  it('Vorschau ist deterministisch und ohne Fehlschläge', () => {
    const map = parseHeist(testDef);
    const plan = tutorialPlan(map);
    const a = simulate(plan, map, profile, { mode: 'preview', seed: 1, intel: 3 });
    const b = simulate(plan, parseHeist(testDef), profile, { mode: 'preview', seed: 999, intel: 3 });
    expect(a.outcome).toBe(b.outcome);
    expect(a.lootTaken.map((l) => l.id)).toEqual(b.lootTaken.map((l) => l.id));
    expect(a.duration).toBeCloseTo(b.duration);
  });

  it('erfolgreicher Tutorial-Coup nimmt Beute mit', () => {
    const map = parseHeist(testDef);
    const res = simulate(tutorialPlan(map), map, profile, { mode: 'preview', seed: 1, intel: 3 });
    expect(res.lootTaken).toHaveLength(1);
    expect(res.outcome).toBe('success');
    expect(res.alarmTriggered).toBe(false);
  });

  it('Ausführung ist bei gleichem Seed reproduzierbar', () => {
    const map1 = parseHeist(testDef);
    const map2 = parseHeist(testDef);
    const r1 = simulate(tutorialPlan(map1), map1, profile, { mode: 'exec', seed: 42, intel: 3 });
    const r2 = simulate(tutorialPlan(map2), map2, profile, { mode: 'exec', seed: 42, intel: 3 });
    expect(r1.outcome).toBe(r2.outcome);
    expect(r1.duration).toBeCloseTo(r2.duration);
    expect(r1.events.length).toBe(r2.events.length);
  });

  it('compilePlan meldet Weg-Warnung wenn Tür nicht geöffnet wird', () => {
    const map = parseHeist(testDef);
    const plan: Plan = {
      heistId: 'test',
      steps: [{ id: 's1', action: 'grabLoot', targetId: map.loot[0].id, mode: 'sneak' }],
    };
    const compiled = compilePlan(plan, { map, profile });
    // Ohne Türöffnung ist die Kasse nicht erreichbar → Warnung + invalid
    expect(compiled.valid).toBe(false);
  });
});
