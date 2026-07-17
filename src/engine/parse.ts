// Parser für das Autoren-Format der Heists: ASCII-Grundriss + Legende.
//
// Basiszeichen:  ' ' void · ',' Außenbereich · '.' Boden · '#' Wand
//                '0'-'3' Tür (Schlossstufe) · 'w' Fenster · 'f' Möbel
//                'E' Fluchtpunkt · 'C' Kletterstelle
// Alle anderen Zeichen sind Marker und werden über die Legende aufgelöst
// (Beute, Safes, Sensoren, Lampen, Wach-Wegpunkte …); das Tile darunter ist Boden.

import type {
  Container,
  Guard,
  HeistMap,
  Lamp,
  LootItem,
  Sensor,
  Tile,
  Vec,
} from './types';

export interface LootSpec {
  name: string;
  icon: string;
  value: number;
  weight: number;
  bulky?: boolean;
  hot?: boolean;
}

export type LegendEntry =
  | ({ obj: 'loot' } & LootSpec)
  | {
      obj: 'safe' | 'vitrine';
      name: string;
      level: 1 | 2 | 3;
      funkalarm?: boolean;
      loot: LootSpec[];
    }
  | { obj: 'motion' | 'camera'; dir: 'N' | 'S' | 'E' | 'W'; range?: number; fov?: number; sweep?: number }
  | { obj: 'plate' }
  | { obj: 'fusebox' }
  | { obj: 'phone' }
  | { obj: 'lamp'; radius?: number; level?: number }
  | { obj: 'waypoint' };

export interface GuardSpec {
  name: string;
  route: { at: string; wait: number }[];
  speed?: number;
  visionRange?: number;
}

export interface HeistDef {
  id: string;
  name: string;
  desc: string;
  difficulty: 1 | 2 | 3 | 4 | 5;
  /** Preis des Tipps beim Informanten (0 = von Anfang an bekannt) */
  unlockCost: number;
  /** Preise der Auskundschaftungs-Stufen 1..3 */
  intelCosts: [number, number, number];
  policeBase: number;
  baseLight: number;
  xpBase: number;
  map: string[];
  legend: Record<string, LegendEntry>;
  guards: GuardSpec[];
}

const DIRS: Record<'N' | 'S' | 'E' | 'W', number> = {
  E: 0,
  S: Math.PI / 2,
  W: Math.PI,
  N: -Math.PI / 2,
};

export function parseHeist(def: HeistDef): HeistMap {
  const h = def.map.length;
  const w = Math.max(...def.map.map((r) => r.length));
  const tiles: Tile[] = new Array(w * h);
  const loot: LootItem[] = [];
  const containers: Container[] = [];
  const sensors: Sensor[] = [];
  const lamps: Lamp[] = [];
  const waypoints: Record<string, Vec> = {};
  let exit: Vec | undefined;
  let fusebox: Vec | undefined;
  let phone: Vec | undefined;
  let nextId = 0;

  for (let y = 0; y < h; y++) {
    const row = def.map[y];
    for (let x = 0; x < w; x++) {
      const ch = x < row.length ? row[x] : ' ';
      const pos = { x, y };
      let tile: Tile = { kind: 'void', lockLevel: 0 };
      switch (ch) {
        case ' ':
          break;
        case ',':
          tile = { kind: 'outdoor', lockLevel: 0 };
          break;
        case '.':
          tile = { kind: 'floor', lockLevel: 0 };
          break;
        case '#':
          tile = { kind: 'wall', lockLevel: 0 };
          break;
        case '0':
        case '1':
        case '2':
        case '3':
          tile = { kind: 'door', lockLevel: Number(ch) as 0 | 1 | 2 | 3 };
          break;
        case 'w':
          tile = { kind: 'window', lockLevel: 0 };
          break;
        case 'f':
          tile = { kind: 'furniture', lockLevel: 0 };
          break;
        case 'E':
          tile = { kind: 'exit', lockLevel: 0 };
          exit = pos;
          break;
        case 'C':
          tile = { kind: 'climb', lockLevel: 0 };
          break;
        default: {
          const entry = def.legend[ch];
          if (!entry) throw new Error(`Heist ${def.id}: unbekanntes Zeichen '${ch}' bei ${x},${y}`);
          tile = { kind: 'floor', lockLevel: 0 };
          switch (entry.obj) {
            case 'loot':
              loot.push({
                id: `L${nextId++}`,
                name: entry.name,
                icon: entry.icon,
                value: entry.value,
                weight: entry.weight,
                bulky: entry.bulky,
                hot: entry.hot,
                pos,
              });
              break;
            case 'safe':
            case 'vitrine': {
              const cid = `C${nextId++}`;
              containers.push({
                id: cid,
                kind: entry.obj,
                name: entry.name,
                pos,
                level: entry.level,
                funkalarm: entry.funkalarm,
              });
              for (const l of entry.loot) {
                loot.push({
                  id: `L${nextId++}`,
                  name: l.name,
                  icon: l.icon,
                  value: l.value,
                  weight: l.weight,
                  bulky: l.bulky,
                  hot: l.hot,
                  containerId: cid,
                });
              }
              break;
            }
            case 'motion':
            case 'camera':
              sensors.push({
                id: `S${nextId++}`,
                kind: entry.obj,
                pos,
                dir: DIRS[entry.dir],
                fov: entry.fov ?? (entry.obj === 'camera' ? Math.PI / 3 : Math.PI / 2.2),
                range: entry.range ?? (entry.obj === 'camera' ? 7 : 5),
                sweep: entry.sweep ?? (entry.obj === 'camera' ? Math.PI / 4 : 0),
              });
              break;
            case 'plate':
              sensors.push({ id: `S${nextId++}`, kind: 'plate', pos, dir: 0, fov: 0, range: 0, sweep: 0 });
              break;
            case 'fusebox':
              fusebox = pos;
              break;
            case 'phone':
              phone = pos;
              break;
            case 'lamp':
              lamps.push({ pos, radius: entry.radius ?? 4, level: entry.level ?? 0.85 });
              break;
            case 'waypoint':
              waypoints[ch] = pos;
              break;
          }
        }
      }
      tiles[y * w + x] = tile;
    }
  }

  if (!exit) throw new Error(`Heist ${def.id}: kein Fluchtpunkt 'E' in der Karte`);

  const guards: Guard[] = def.guards.map((g, i) => ({
    id: `G${i}`,
    name: g.name,
    speed: g.speed ?? 1.4,
    visionRange: g.visionRange ?? 6,
    route: g.route.map((r) => {
      const p = waypoints[r.at];
      if (!p) throw new Error(`Heist ${def.id}: Wegpunkt '${r.at}' fehlt in der Karte`);
      return { pos: p, wait: r.wait };
    }),
  }));

  return {
    w,
    h,
    tiles,
    exit,
    loot,
    containers,
    sensors,
    guards,
    lamps,
    fusebox,
    phone,
    baseLight: def.baseLight,
    policeBase: def.policeBase,
  };
}
