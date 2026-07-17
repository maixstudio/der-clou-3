// Leitet aus einem Klick auf ein Tile die möglichen Plan-Aktionen ab.
// Wählt automatisch das passende Werkzeug aus dem Loadout und markiert
// Aktionen, für die noch Ausrüstung fehlt.

import { ACTIONS, dietrichTier } from '../engine/actions';
import { tileAt } from '../engine/grid';
import type { ActionId, ActorProfile, HeistMap, ToolId, Vec } from '../engine/types';
import { key } from '../engine/types';

export interface ActionChoice {
  action: ActionId;
  label: string;
  target?: Vec;
  targetId?: string;
  tool?: ToolId;
  waitTime?: number;
  disabledReason: string | null;
}

const at = (a: Vec, b: Vec): boolean => a.x === b.x && a.y === b.y;

function choice(
  action: Exclude<ActionId, 'moveTo' | 'wait'>,
  profile: ActorProfile,
  level: number,
  extra: Partial<ActionChoice> = {},
): ActionChoice {
  const spec = ACTIONS[action];
  return {
    action,
    label: spec.label,
    tool: spec.tool,
    disabledReason: spec.available(profile, level),
    ...extra,
  };
}

export function actionsAt(map: HeistMap, pos: Vec, profile: ActorProfile): ActionChoice[] {
  const out: ActionChoice[] = [];
  const t = tileAt(map, pos);
  if (!t) return out;

  // Tür
  if (t.kind === 'door') {
    if (t.lockLevel === 0) {
      out.push(choice('openDoor', profile, 0, { target: pos }));
    } else {
      const c = choice('pickLock', profile, t.lockLevel, { target: pos });
      c.tool = (['dietrich3', 'dietrich2', 'dietrich1'] as ToolId[])[3 - Math.max(1, dietrichTier(profile) || t.lockLevel)] ?? 'dietrich1';
      out.push(c);
      out.push(choice('forceDoor', profile, t.lockLevel, { target: pos }));
    }
  }
  // Fenster
  if (t.kind === 'window') {
    out.push(choice('cutWindow', profile, 0, { target: pos }));
    out.push(choice('smashWindow', profile, 0, { target: pos }));
  }
  // Kletterstelle
  if (t.kind === 'climb') {
    out.push(choice('climb', profile, 0, { target: pos }));
  }

  // Behälter am Tile
  const container = map.containers.find((c) => at(c.pos, pos));
  if (container) {
    if (container.kind === 'safe') {
      out.push(choice('crackSafe', profile, container.level, { target: pos, targetId: container.id }));
      out.push(choice('drillSafe', profile, container.level, { target: pos, targetId: container.id }));
      out.push(choice('grindSafe', profile, container.level, { target: pos, targetId: container.id }));
    } else {
      out.push(choice('cutVitrine', profile, container.level, { target: pos, targetId: container.id }));
      out.push(choice('smashVitrine', profile, container.level, { target: pos, targetId: container.id }));
    }
    // Beute im Behälter
    for (const l of map.loot.filter((x) => x.containerId === container.id)) {
      out.push({
        action: 'grabLoot',
        label: `Einpacken: ${l.icon} ${l.name}`,
        target: pos,
        targetId: l.id,
        disabledReason: null,
      });
    }
  }

  // Freiliegende Beute
  for (const l of map.loot.filter((x) => x.pos && at(x.pos, pos))) {
    out.push({
      action: 'grabLoot',
      label: `Einpacken: ${l.icon} ${l.name}`,
      target: pos,
      targetId: l.id,
      disabledReason: l.bulky && !profile.hasVehicle ? 'Fahrzeug nötig (sperrig)' : null,
    });
  }

  // Sensoren
  const sensor = map.sensors.find((s) => at(s.pos, pos));
  if (sensor) {
    if (sensor.kind === 'motion') out.push(choice('disableSensor', profile, 0, { target: pos, targetId: sensor.id }));
    else if (sensor.kind === 'camera') out.push(choice('disableCamera', profile, 0, { target: pos, targetId: sensor.id }));
    // Druckplatten kann man nicht deaktivieren – nur meiden.
  }

  // Sicherungskasten / Telefon
  if (map.fusebox && at(map.fusebox, pos)) out.push(choice('cutPower', profile, 0, { target: pos }));
  if (map.phone && at(map.phone, pos)) out.push(choice('cutPhone', profile, 0, { target: pos }));

  // Bewegung + Ablenkung auf begehbaren Feldern
  if (t.kind === 'floor' || t.kind === 'outdoor' || t.kind === 'exit') {
    out.unshift({ action: 'moveTo', label: '→ Hierher gehen', target: pos, disabledReason: null });
    out.push(choice('placeDistraction', profile, 0, { target: pos }));
  }

  return out;
}

/** Kurzbeschreibung eines Tiles für die Kontextmenü-Überschrift. */
export function describeTile(map: HeistMap, pos: Vec): string {
  const t = tileAt(map, pos);
  if (!t) return '';
  const container = map.containers.find((c) => at(c.pos, pos));
  if (container) return `${container.name} (Stufe ${container.level})`;
  const loot = map.loot.find((l) => l.pos && at(l.pos, pos));
  if (loot) return `${loot.icon} ${loot.name}`;
  const sensor = map.sensors.find((s) => at(s.pos, pos));
  if (sensor) return sensor.kind === 'camera' ? '📹 Kamera' : sensor.kind === 'motion' ? '🔺 Bewegungsmelder' : '⚠️ Druckplatte';
  if (map.fusebox && at(map.fusebox, pos)) return '⚡ Sicherungskasten';
  if (map.phone && at(map.phone, pos)) return '☎️ Telefon';
  switch (t.kind) {
    case 'door':
      return t.lockLevel === 0 ? 'Offene Tür' : `Verschlossene Tür (Stufe ${t.lockLevel})`;
    case 'window':
      return 'Fenster';
    case 'climb':
      return 'Kletterstelle';
    case 'exit':
      return '🚗 Fluchtpunkt';
    default:
      return `Feld ${key(pos)}`;
  }
}
