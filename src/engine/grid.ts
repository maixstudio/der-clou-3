// Tile-Helfer und A*-Pathfinding (4 Richtungen).

import type { HeistMap, Tile, Vec } from './types';
import { idx } from './types';

export interface PathOpts {
  /** Tile-Indizes bereits geöffneter Türen/Fenster (kostenfrei passierbar) */
  opened?: Set<number>;
  /** darf durch (noch zu öffnende) Türen planen */
  passDoors?: boolean;
  /** darf durch (noch zu öffnende) Fenster planen */
  passWindows?: boolean;
  /** darf Kletterstellen nutzen (Seil dabei) */
  canClimb?: boolean;
  /** bekannte Druckplatten (Tile-Indizes) werden gemieden */
  avoid?: Set<number>;
}

export function tileAt(map: HeistMap, p: Vec): Tile | undefined {
  if (p.x < 0 || p.y < 0 || p.x >= map.w || p.y >= map.h) return undefined;
  return map.tiles[idx(p, map.w)];
}

/** Ist das Tile im aktuellen Weltzustand betretbar? */
export function walkable(map: HeistMap, p: Vec, opened: Set<number>): boolean {
  const t = tileAt(map, p);
  if (!t) return false;
  switch (t.kind) {
    case 'floor':
    case 'outdoor':
    case 'exit':
      return true;
    case 'door':
      return t.lockLevel === 0 || opened.has(idx(p, map.w));
    case 'window':
    case 'climb':
      return opened.has(idx(p, map.w));
    default:
      return false;
  }
}

function stepCost(map: HeistMap, p: Vec, o: PathOpts): number | null {
  const t = tileAt(map, p);
  if (!t) return null;
  const i = idx(p, map.w);
  const openedFree = o.opened?.has(i);
  let c: number | null = null;
  switch (t.kind) {
    case 'floor':
    case 'outdoor':
    case 'exit':
      c = 1;
      break;
    case 'door':
      if (openedFree || t.lockLevel === 0) c = 1.5;
      else if (o.passDoors) c = 3;
      break;
    case 'window':
      if (openedFree) c = 3;
      else if (o.passWindows) c = 6;
      break;
    case 'climb':
      if (openedFree) c = 3;
      else if (o.canClimb) c = 8;
      break;
    default:
      c = null;
  }
  if (c !== null && o.avoid?.has(i)) c += 40;
  return c;
}

/**
 * A* von start nach goal. Liefert Tile-Folge inkl. Start und Ziel oder null.
 * Kosten bevorzugen offene Wege und meiden bekannte Druckplatten.
 */
export function findPath(map: HeistMap, start: Vec, goal: Vec, opts: PathOpts = {}): Vec[] | null {
  const w = map.w;
  const startI = idx(start, w);
  const goalI = idx(goal, w);
  if (startI === goalI) return [start];

  const g = new Map<number, number>();
  const from = new Map<number, number>();
  const open: { i: number; f: number }[] = [{ i: startI, f: 0 }];
  g.set(startI, 0);
  const hx = (i: number): number =>
    Math.abs((i % w) - goal.x) + Math.abs(Math.floor(i / w) - goal.y);

  while (open.length > 0) {
    let best = 0;
    for (let k = 1; k < open.length; k++) if (open[k].f < open[best].f) best = k;
    const { i: cur } = open.splice(best, 1)[0];
    if (cur === goalI) {
      const path: Vec[] = [];
      let c: number | undefined = cur;
      while (c !== undefined) {
        path.push({ x: c % w, y: Math.floor(c / w) });
        c = from.get(c);
      }
      return path.reverse();
    }
    const cx = cur % w;
    const cy = Math.floor(cur / w);
    for (const [dx, dy] of [
      [1, 0],
      [-1, 0],
      [0, 1],
      [0, -1],
    ] as const) {
      const np = { x: cx + dx, y: cy + dy };
      const cost = stepCost(map, np, opts);
      if (cost === null) continue;
      const ni = idx(np, w);
      const ng = (g.get(cur) ?? 0) + cost;
      if (ng < (g.get(ni) ?? Infinity)) {
        g.set(ni, ng);
        from.set(ni, cur);
        open.push({ i: ni, f: ng + hx(ni) });
      }
    }
  }
  return null;
}

/** Länge eines Pfads in Bewegungs-Tiles (Anzahl Schritte). */
export function pathSteps(path: Vec[]): number {
  return Math.max(0, path.length - 1);
}
