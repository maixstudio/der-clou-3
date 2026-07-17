import { describe, expect, it } from 'vitest';
import { HEISTS } from './heists';
import { parseHeist } from '../../engine/parse';
import { findPath, tileAt } from '../../engine/grid';
import type { HeistMap, Vec } from '../../engine/types';

const ALL_OPEN = { passDoors: true, passWindows: true, canClimb: true };

function neighbors(p: Vec): Vec[] {
  return [
    { x: p.x + 1, y: p.y },
    { x: p.x - 1, y: p.y },
    { x: p.x, y: p.y + 1 },
    { x: p.x, y: p.y - 1 },
  ];
}

/** Ist p ein Ziel, das man von der Nachbarschaft aus erreichen kann? */
function reachableAdjacent(map: HeistMap, target: Vec): boolean {
  for (const n of neighbors(target)) {
    const t = tileAt(map, n);
    if (!t) continue;
    if (t.kind === 'wall' || t.kind === 'void' || t.kind === 'furniture') continue;
    const path = findPath(map, map.exit, n, ALL_OPEN);
    if (path) return true;
  }
  // Ziel selbst begehbar?
  const path = findPath(map, map.exit, target, ALL_OPEN);
  return !!path;
}

describe('Heist-Content-Validierung', () => {
  for (const def of HEISTS) {
    describe(def.name, () => {
      const map = parseHeist(def);

      it('hat einen Fluchtpunkt', () => {
        expect(map.exit).toBeDefined();
      });

      it('alle freiliegende Beute ist erreichbar', () => {
        for (const l of map.loot.filter((x) => x.pos)) {
          expect(reachableAdjacent(map, l.pos!), `${def.name}: Beute ${l.name} bei ${l.pos!.x},${l.pos!.y} unerreichbar`).toBe(true);
        }
      });

      it('alle Behälter sind erreichbar', () => {
        for (const c of map.containers) {
          expect(reachableAdjacent(map, c.pos), `${def.name}: Behälter ${c.name} bei ${c.pos.x},${c.pos.y} unerreichbar`).toBe(true);
        }
      });

      it('alle Wach-Wegpunkte sind untereinander erreichbar', () => {
        for (const g of map.guards) {
          for (const wp of g.route) {
            const path = findPath(map, g.route[0].pos, wp.pos, { passDoors: false });
            expect(path, `${def.name}: Wegpunkt ${wp.pos.x},${wp.pos.y} von ${g.name} unerreichbar`).not.toBeNull();
          }
        }
      });

      it('Sicherungskasten/Telefon (falls vorhanden) erreichbar', () => {
        if (map.fusebox) expect(reachableAdjacent(map, map.fusebox)).toBe(true);
        if (map.phone) expect(reachableAdjacent(map, map.phone)).toBe(true);
      });
    });
  }
});
