// Planner: übersetzt die vom Spieler zusammengeklickte Schrittliste in einen
// kompilierten Plan (Pfade, Dauern, Fehlschlag-Wahrscheinlichkeiten, Warnungen).
// Der kompilierte Plan speist Timeline, Vorschau und Ausführung.

import { ACTIONS, DISTRACTION_DELAY, DISTRACTION_LENGTH } from './actions';
import { findPath, pathSteps, tileAt } from './grid';
import type {
  ActionId,
  ActorProfile,
  CompiledPlan,
  CompiledStep,
  Container,
  HeistMap,
  LootItem,
  Plan,
  PlanStep,
  Sensor,
  Vec,
} from './types';
import { idx, key } from './types';

/** Weltzustand während der Plan-Kompilierung (rein logisch, ohne Zufall). */
interface PlanWorld {
  pos: Vec;
  opened: Set<number>; // Tile-Indizes offener Türen/Fenster/Kletterstellen
  openedContainers: Set<string>;
  disabledSensors: Set<string>;
  powerCut: boolean;
  carried: LootItem[];
  weight: number;
}

export interface CompileCtx {
  map: HeistMap;
  profile: ActorProfile;
}

const roundV = (v: Vec): Vec => ({ x: Math.round(v.x), y: Math.round(v.y) });

function findContainer(map: HeistMap, id?: string): Container | undefined {
  return map.containers.find((c) => c.id === id);
}
function findSensor(map: HeistMap, id?: string): Sensor | undefined {
  return map.sensors.find((s) => s.id === id);
}
function findLoot(map: HeistMap, id?: string): LootItem | undefined {
  return map.loot.find((l) => l.id === id);
}

/** Wo muss der Spieler stehen, um die Aktion auszuführen? (adjazentes Tile) */
function actionStandTile(map: HeistMap, step: PlanStep, world: PlanWorld): Vec | null {
  const spec = ACTIONS[step.action as keyof typeof ACTIONS];
  if (!spec) return step.target ? roundV(step.target) : null;

  // Türen/Fenster/Kletterstellen: das Tile davor (begehbares Nachbarfeld)
  if (step.target && (spec.targetKind === 'door' || spec.targetKind === 'window' || spec.targetKind === 'climb')) {
    return adjacentReachable(map, roundV(step.target), world) ?? roundV(step.target);
  }
  // Objekt-Aktionen: Nachbarfeld des Objekts
  let objPos: Vec | undefined;
  if (spec.targetKind === 'safe' || spec.targetKind === 'vitrine') objPos = findContainer(map, step.targetId)?.pos;
  else if (spec.targetKind === 'loot') objPos = findLoot(map, step.targetId)?.pos;
  else if (spec.targetKind === 'sensor' || spec.targetKind === 'camera') objPos = findSensor(map, step.targetId)?.pos;
  else if (spec.targetKind === 'fusebox') objPos = map.fusebox;
  else if (spec.targetKind === 'phone') objPos = map.phone;
  else if (step.target) return roundV(step.target);

  if (!objPos) return null;
  return adjacentReachable(map, objPos, world) ?? objPos;
}

/** Begehbares Nachbarfeld eines (evtl. blockierenden) Zielfeldes. */
function adjacentReachable(map: HeistMap, p: Vec, world: PlanWorld): Vec | null {
  const cand = [
    { x: p.x + 1, y: p.y },
    { x: p.x - 1, y: p.y },
    { x: p.x, y: p.y + 1 },
    { x: p.x, y: p.y - 1 },
  ];
  for (const c of cand) {
    const t = tileAt(map, c);
    if (!t) continue;
    if (t.kind === 'floor' || t.kind === 'outdoor' || t.kind === 'exit') return c;
    if ((t.kind === 'door' || t.kind === 'window' || t.kind === 'climb') && world.opened.has(idx(c, map.w)))
      return c;
  }
  return null;
}

function actionLevel(map: HeistMap, step: PlanStep): number {
  const spec = ACTIONS[step.action as keyof typeof ACTIONS];
  if (!spec) return 1;
  if (spec.targetKind === 'door' && step.target) return tileAt(map, roundV(step.target))?.lockLevel ?? 1;
  if (spec.targetKind === 'safe') return findContainer(map, step.targetId)?.level ?? 1;
  return 1;
}

function walkFactor(profile: ActorProfile, mode: 'walk' | 'sneak'): number {
  const speed = mode === 'sneak' ? profile.sneakSpeed : profile.walkSpeed;
  return 1 / speed;
}

/** Wendet die logischen Folgen einer Aktion auf den Plan-Weltzustand an. */
function applyEffect(step: PlanStep, world: PlanWorld, ctx: CompileCtx): void {
  const { map } = ctx;
  switch (step.action) {
    case 'openDoor':
    case 'pickLock':
    case 'forceDoor':
      if (step.target) world.opened.add(idx(roundV(step.target), map.w));
      break;
    case 'cutWindow':
    case 'smashWindow':
    case 'climb':
      if (step.target) world.opened.add(idx(roundV(step.target), map.w));
      break;
    case 'crackSafe':
    case 'drillSafe':
    case 'grindSafe':
    case 'cutVitrine':
    case 'smashVitrine':
      if (step.targetId) world.openedContainers.add(step.targetId);
      break;
    case 'disableSensor':
    case 'disableCamera':
      if (step.targetId) world.disabledSensors.add(step.targetId);
      break;
    case 'cutPower':
      world.powerCut = true;
      break;
    case 'grabLoot': {
      const loot = findLoot(map, step.targetId);
      if (loot) {
        world.carried.push(loot);
        world.weight += loot.weight;
      }
      break;
    }
  }
}

/** Prüft eine einzelne Aktion und liefert Warnungen (leere Liste = ok). */
function stepWarnings(step: PlanStep, world: PlanWorld, ctx: CompileCtx): string[] {
  const { map, profile } = ctx;
  const warn: string[] = [];
  const spec = ACTIONS[step.action as keyof typeof ACTIONS];
  if (step.action === 'moveTo' || step.action === 'wait') return warn;
  if (!spec) return ['Unbekannte Aktion'];

  const level = actionLevel(map, step);
  const avail = spec.available(profile, level);
  if (avail) warn.push(avail);

  if (step.action === 'grabLoot') {
    const loot = findLoot(map, step.targetId);
    if (loot?.containerId && !world.openedContainers.has(loot.containerId)) {
      warn.push('Behälter ist noch verschlossen');
    }
    if (loot?.bulky && !profile.hasVehicle) {
      warn.push('Sperrige Beute braucht ein Fahrzeug');
    }
    if (loot && world.weight + loot.weight > profile.carryCapacity) {
      warn.push('Überschreitet die Tragkraft');
    }
  }
  if ((step.action === 'crackSafe' || step.action === 'drillSafe' || step.action === 'grindSafe')) {
    if (!findContainer(map, step.targetId)) warn.push('Kein Safe am Ziel');
  }
  return warn;
}

/**
 * Kompiliert den Plan. `moveTo`-Schritte bekommen A*-Pfade, alle Schritte
 * Dauer/Lärm/Fehlschlag. Zwischen zwei Aktionen wird automatisch ein Laufweg
 * zum Standfeld eingefügt (impliziter Move), damit der Spieler nicht jeden
 * Schritt einzeln klicken muss.
 */
export function compilePlan(plan: Plan, ctx: CompileCtx): CompiledPlan {
  const { map, profile } = ctx;
  const world: PlanWorld = {
    pos: roundV(map.exit),
    opened: new Set(),
    openedContainers: new Set(),
    disabledSensors: new Set(),
    powerCut: false,
    carried: [],
    weight: 0,
  };
  const steps: CompiledStep[] = [];
  const planWarnings: string[] = [];
  let t = 0;
  let valid = true;

  const pushMoveTo = (dest: Vec, mode: 'walk' | 'sneak', srcStep: PlanStep): void => {
    const path = findPath(map, world.pos, dest, {
      opened: world.opened,
      passDoors: false,
      passWindows: false,
      canClimb: false,
    });
    const warnings: string[] = [];
    let moveDuration = 0;
    if (!path) {
      warnings.push('Kein Weg zum Ziel (verschlossen?)');
      valid = false;
    } else {
      moveDuration = pathSteps(path) * walkFactor(profile, mode);
      world.pos = dest;
    }
    const cs: CompiledStep = {
      step: srcStep,
      path: path ?? [],
      moveDuration,
      actionDuration: 0,
      noise: mode === 'sneak' ? profile.sneakNoise : profile.walkNoise,
      failChance: 0,
      startT: t,
      endT: t + moveDuration,
      warnings,
    };
    steps.push(cs);
    t = cs.endT;
  };

  for (const step of plan.steps) {
    const mode = step.mode ?? 'sneak';
    if (step.action === 'moveTo') {
      if (!step.target) continue;
      pushMoveTo(roundV(step.target), mode, step);
      continue;
    }
    if (step.action === 'wait') {
      const dur = step.waitTime ?? 5;
      const cs: CompiledStep = {
        step,
        path: [],
        moveDuration: 0,
        actionDuration: dur,
        noise: 0,
        failChance: 0,
        startT: t,
        endT: t + dur,
        warnings: [],
      };
      steps.push(cs);
      t = cs.endT;
      continue;
    }

    // Aktion: erst zum Standfeld laufen (impliziter Move), dann ausführen.
    const stand = actionStandTile(map, step, world);
    let moveDuration = 0;
    let path: Vec[] = [];
    if (stand && key(stand) !== key(world.pos)) {
      const p = findPath(map, world.pos, stand, {
        opened: world.opened,
        passDoors: false,
        passWindows: false,
        canClimb: false,
      });
      if (p) {
        path = p;
        moveDuration = pathSteps(p) * walkFactor(profile, mode);
        world.pos = stand;
      } else {
        planWarnings.push(`Kein Weg zu ${ACTIONS[step.action as keyof typeof ACTIONS]?.label ?? step.action}`);
        valid = false;
      }
    }

    const spec = ACTIONS[step.action as keyof typeof ACTIONS];
    const level = actionLevel(map, step);
    const loot = findLoot(map, step.targetId);
    const actionCtx = { profile, level, bulky: loot?.bulky, waitTime: step.waitTime };
    const warnings = stepWarnings(step, world, ctx);
    if (warnings.length > 0) valid = false;

    const actionDuration = spec ? spec.duration(actionCtx) : 0;
    const cs: CompiledStep = {
      step,
      path,
      moveDuration,
      actionDuration,
      noise: spec ? spec.noise(actionCtx) : 0,
      failChance: spec ? spec.failChance(actionCtx) : 0,
      startT: t,
      endT: t + moveDuration + actionDuration,
      warnings,
    };
    steps.push(cs);
    t = cs.endT;
    applyEffect(step, world, ctx);
  }

  // Warnung: keine Beute eingepackt
  if (world.carried.length === 0) planWarnings.push('Es wird keine Beute eingepackt');
  // Warnung: endet nicht am Fluchtpunkt
  const lastPos = world.pos;
  if (key(lastPos) !== key(roundV(map.exit))) {
    planWarnings.push('Plan endet nicht am Fluchtpunkt – Fluchtweg wird automatisch angehängt');
  }

  return { heistId: plan.heistId, steps, totalTime: t, warnings: planWarnings, valid };
}

/** Summe der eingeplanten Beute (für die Tragkraft-Anzeige). */
export function plannedLoot(plan: Plan, map: HeistMap): LootItem[] {
  const out: LootItem[] = [];
  for (const s of plan.steps) {
    if (s.action === 'grabLoot') {
      const l = map.loot.find((x) => x.id === s.targetId);
      if (l) out.push(l);
    }
  }
  return out;
}

export const ACTION_ORDER: ActionId[] = [
  'moveTo',
  'pickLock',
  'forceDoor',
  'openDoor',
  'cutWindow',
  'smashWindow',
  'climb',
  'crackSafe',
  'drillSafe',
  'grindSafe',
  'cutVitrine',
  'smashVitrine',
  'grabLoot',
  'disableSensor',
  'disableCamera',
  'cutPower',
  'cutPhone',
  'placeDistraction',
  'wait',
];

export { DISTRACTION_DELAY, DISTRACTION_LENGTH };
