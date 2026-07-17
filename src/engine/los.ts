// Sichtlinien (Bresenham), Sichtkegel und Lichtberechnung.

import type { HeistMap, Vec } from './types';
import { clamp, dist, idx } from './types';
import { tileAt } from './grid';

/** Blockiert das Tile die Sicht? Fenster und offene Türen sind durchsichtig. */
function blocksSight(map: HeistMap, p: Vec, opened: Set<number>): boolean {
  const t = tileAt(map, p);
  if (!t) return true;
  if (t.kind === 'wall') return true;
  if (t.kind === 'climb') return true;
  if (t.kind === 'door') return t.lockLevel !== 0 && !opened.has(idx(p, map.w));
  return false;
}

/** Alle Tiles auf der Linie a→b (exklusive Endpunkte). */
export function lineTiles(a: Vec, b: Vec): Vec[] {
  const out: Vec[] = [];
  let x0 = a.x;
  let y0 = a.y;
  const x1 = b.x;
  const y1 = b.y;
  const dx = Math.abs(x1 - x0);
  const dy = -Math.abs(y1 - y0);
  const sx = x0 < x1 ? 1 : -1;
  const sy = y0 < y1 ? 1 : -1;
  let err = dx + dy;
  for (;;) {
    if (x0 === x1 && y0 === y1) break;
    const e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
    if (x0 === x1 && y0 === y1) break;
    out.push({ x: x0, y: y0 });
  }
  return out;
}

export function lineOfSight(map: HeistMap, a: Vec, b: Vec, opened: Set<number>): boolean {
  for (const p of lineTiles(a, b)) {
    if (blocksSight(map, p, opened)) return false;
  }
  return true;
}

/** Anzahl sichtblockierender Tiles zwischen a und b (für Lärm-Dämpfung). */
export function wallsBetween(map: HeistMap, a: Vec, b: Vec, opened: Set<number>): number {
  let n = 0;
  for (const p of lineTiles(a, b)) {
    if (blocksSight(map, p, opened)) n++;
  }
  return n;
}

/** Liegt target im Kegel (origin, facing, fov, range)? */
export function inCone(origin: Vec, facing: number, fov: number, range: number, target: Vec): boolean {
  const d = dist(origin, target);
  if (d > range) return false;
  if (d < 0.75) return true;
  const ang = Math.atan2(target.y - origin.y, target.x - origin.x);
  let diff = Math.abs(ang - facing);
  if (diff > Math.PI) diff = 2 * Math.PI - diff;
  return diff <= fov / 2;
}

/** Lichtstärke 0..1 am Punkt p (Umgebungslicht + Lampen; Stromausfall löscht Lampen). */
export function lightAt(map: HeistMap, p: Vec, powerCut: boolean): number {
  let l = powerCut ? Math.min(map.baseLight, 0.12) : map.baseLight;
  if (!powerCut) {
    for (const lamp of map.lamps) {
      const d = dist(lamp.pos, p);
      if (d < lamp.radius) l = Math.max(l, lamp.level * (1 - d / lamp.radius));
    }
  }
  return clamp(l, 0, 1);
}
