// Wachen-Zustandsmaschine. Läuft im Simulationstakt. Wachen patrouillieren
// Wegpunkte ab, reagieren auf Lärm (untersuchen) und Sichtkontakt (Alarm → Telefon).

import type { Guard, GuardSnap, GuardState, HeistMap, Vec } from './types';
import { clamp, dist } from './types';
import { findPath } from './grid';
import { inCone, lightAt, lineOfSight } from './los';

export interface GuardRuntime {
  def: Guard;
  pos: Vec;
  facing: number;
  state: GuardState;
  suspicion: number;
  /** Index des aktuellen Wegpunkts */
  wpIndex: number;
  /** verbleibende Wartezeit am Wegpunkt */
  waitLeft: number;
  /** aktueller Bewegungspfad (Tile-Folge, ohne Start) */
  path: Vec[];
  pathStep: number;
  /** Ziel bei Untersuchung / Telefonlauf */
  goal: Vec | null;
  /** true wenn die Wache das Telefon erreicht und Alarm ausgelöst hat */
  calledPolice: boolean;
}

export function initGuard(g: Guard): GuardRuntime {
  return {
    def: g,
    pos: { ...g.route[0].pos },
    facing: 0,
    state: 'patrol',
    suspicion: 0,
    wpIndex: 0,
    waitLeft: g.route[0].wait,
    path: [],
    pathStep: 0,
    goal: null,
    calledPolice: false,
  };
}

export function snapGuard(g: GuardRuntime): GuardSnap {
  return {
    id: g.def.id,
    pos: { x: g.pos.x, y: g.pos.y },
    facing: g.facing,
    state: g.state,
    suspicion: g.suspicion,
  };
}

function setPath(g: GuardRuntime, map: HeistMap, goal: Vec, opened: Set<number>): void {
  const p = findPath(map, roundV(g.pos), goal, { opened, passDoors: false });
  g.path = p ? p.slice(1) : [];
  g.pathStep = 0;
  g.goal = goal;
}

const roundV = (v: Vec): Vec => ({ x: Math.round(v.x), y: Math.round(v.y) });

/** Bewegt die Wache `speed*dt` Tiles ihrem Pfad entlang. true bei Zielankunft. */
function advance(g: GuardRuntime, dt: number, speed: number): boolean {
  let budget = speed * dt;
  while (budget > 0 && g.pathStep < g.path.length) {
    const target = g.path[g.pathStep];
    const dx = target.x - g.pos.x;
    const dy = target.y - g.pos.y;
    const d = Math.hypot(dx, dy);
    if (d < 1e-4) {
      g.pathStep++;
      continue;
    }
    g.facing = Math.atan2(dy, dx);
    if (budget >= d) {
      g.pos = { x: target.x, y: target.y };
      budget -= d;
      g.pathStep++;
    } else {
      g.pos = { x: g.pos.x + (dx / d) * budget, y: g.pos.y + (dy / d) * budget };
      budget = 0;
    }
  }
  return g.pathStep >= g.path.length;
}

export interface GuardSenses {
  /** aktuelle Spielerposition, oder null wenn Spieler nicht sichtbar-relevant (geflohen/raus) */
  player: Vec | null;
  /** Bewegt sich der Spieler gerade? (leichter zu entdecken) */
  playerMoving: boolean;
  /** Schleich-Faktor 0..1 des Spielers (klein = leiser/unauffälliger) */
  stealth: number;
  powerCut: boolean;
  opened: Set<number>;
  /** wahrgenommene Lärmquelle diesen Tick (Position) + Stärke 0..1 */
  noiseSource: Vec | null;
  noiseLevel: number;
  detectionFactor: number;
}

export interface GuardTickResult {
  sawPlayer: boolean;
  reachedPhone: boolean;
}

/**
 * Ein Wachen-Tick. Aktualisiert Position/Zustand/Verdacht und meldet, ob die
 * Wache den Spieler diesen Tick gesehen hat und ob sie das Telefon erreichte.
 */
export function tickGuard(
  g: GuardRuntime,
  map: HeistMap,
  dt: number,
  s: GuardSenses,
  phone: Vec | undefined,
  alarm: boolean,
): GuardTickResult {
  const res: GuardTickResult = { sawPlayer: false, reachedPhone: false };

  // Sichtprüfung
  let canSee = false;
  if (s.player) {
    const gp = roundV(g.pos);
    const range = g.def.visionRange;
    if (
      inCone(g.pos, g.facing, Math.PI * 0.7, range, s.player) &&
      lineOfSight(map, gp, roundV(s.player), s.opened)
    ) {
      const d = dist(g.pos, s.player);
      const light = lightAt(map, roundV(s.player), s.powerCut);
      // Entdeckungszuwachs: nah + hell + bewegt + schlechtes Schleichen = schnell
      let gain =
        (1 - d / (range + 1)) *
        (0.35 + light * 0.65) *
        (s.playerMoving ? 1 : 0.4) *
        (0.4 + s.stealth) *
        s.detectionFactor;
      gain = clamp(gain, 0, 1);
      g.suspicion = clamp(g.suspicion + gain * dt * 1.6, 0, 1.2);
      canSee = true;
      if (g.suspicion >= 1) res.sawPlayer = true;
    }
  }

  // Zustandsübergänge
  if (alarm || g.suspicion >= 1) {
    if (g.state !== 'chase' && g.state !== 'phone') {
      g.state = phone ? 'phone' : 'chase';
      if (phone) setPath(g, map, phone, s.opened);
      else if (s.player) setPath(g, map, roundV(s.player), s.opened);
    }
  } else if (g.suspicion > 0.35) {
    if (g.state === 'patrol' && s.noiseSource) {
      g.state = 'investigate';
      setPath(g, map, roundV(s.noiseSource), s.opened);
    }
  }

  // Lärm heraufsetzt Verdacht auch ohne Sicht
  if (!canSee && s.noiseSource && s.noiseLevel > 0) {
    g.suspicion = clamp(g.suspicion + s.noiseLevel * dt * 0.5, 0, 0.95);
    if (g.state === 'patrol' && s.noiseLevel > 0.25) {
      g.state = 'investigate';
      setPath(g, map, roundV(s.noiseSource), s.opened);
    } else if (g.state === 'investigate' && g.suspicion > 0.4) {
      // frische Quelle: Ziel nachführen
      setPath(g, map, roundV(s.noiseSource), s.opened);
    }
  }

  // Verdacht kühlt ab, wenn nichts wahrgenommen wird
  if (!canSee && (!s.noiseSource || s.noiseLevel <= 0)) {
    g.suspicion = clamp(g.suspicion - dt * 0.12, 0, 1.2);
  }

  // Bewegung je nach Zustand
  switch (g.state) {
    case 'patrol': {
      if (g.waitLeft > 0) {
        g.waitLeft -= dt;
        // beim Warten langsam zum nächsten Wegpunkt schauen
        const nextWp = g.def.route[(g.wpIndex + 1) % g.def.route.length].pos;
        g.facing = Math.atan2(nextWp.y - g.pos.y, nextWp.x - g.pos.x);
        break;
      }
      if (g.path.length === 0 || g.pathStep >= g.path.length) {
        const next = (g.wpIndex + 1) % g.def.route.length;
        g.wpIndex = next;
        setPath(g, map, g.def.route[next].pos, s.opened);
        g.waitLeft = 0;
      }
      if (advance(g, dt, g.def.speed)) {
        g.waitLeft = g.def.route[g.wpIndex].wait;
        g.path = [];
      }
      break;
    }
    case 'investigate': {
      if (advance(g, dt, g.def.speed * 1.15)) {
        // Ziel erreicht, nichts gefunden → zurück auf Patrouille
        if (g.suspicion < 0.35) {
          g.state = 'patrol';
          g.path = [];
        } else {
          g.suspicion = clamp(g.suspicion - dt * 0.05, 0, 1.2);
        }
      }
      break;
    }
    case 'phone': {
      if (phone && advance(g, dt, g.def.speed * 1.3)) {
        g.calledPolice = true;
        res.reachedPhone = true;
        g.state = 'chase';
        g.path = [];
      }
      break;
    }
    case 'chase': {
      if (s.player) {
        setPath(g, map, roundV(s.player), s.opened);
        advance(g, dt, g.def.speed * 1.3);
      }
      break;
    }
    case 'alert':
      break;
  }

  return res;
}
