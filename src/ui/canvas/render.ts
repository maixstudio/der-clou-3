// Zeichnet den Grundriss auf ein 2D-Canvas: Tiles, Objekte, Sensoren, den
// geplanten Pfad und – falls vorhanden – einen Live-Snapshot (Spieler, Wachen,
// Alarmzustand). „Fog of Planning": Sensoren/Wachrouten nur bei genug Intel.

import type {
  CompiledPlan,
  HeistMap,
  Sensor,
  Snapshot,
  Vec,
} from '../../engine/types';
import { idx } from '../../engine/types';

export interface RenderOpts {
  map: HeistMap;
  tile: number;
  compiled?: CompiledPlan | null;
  snapshot?: Snapshot | null;
  hover?: Vec | null;
  selected?: Vec | null;
  intel: number;
  activeStep?: number;
}

const COL = {
  bg: '#070512',
  outdoor: '#161232',
  floor: '#1d1746',
  floorGrid: '#241d57',
  wall: '#3a2f7a',
  wallTop: '#4a3d94',
  furniture: '#2c2560',
  door: '#ffce54',
  doorLock2: '#ff9f45',
  doorLock3: '#ff5470',
  window: '#23e0e0',
  exit: '#37e29a',
  climb: '#8a5cff',
  loot: '#ffce54',
  safe: '#c0c0d8',
  vitrine: '#23e0e0',
  sensor: '#ff5470',
  camera: '#ff2fb9',
  plate: '#ff9f45',
  fuse: '#ffce54',
  phone: '#23e0e0',
  player: '#37e29a',
  guard: '#ff5470',
  path: '#23e0e0',
};

export function renderScene(ctx: CanvasRenderingContext2D, o: RenderOpts): void {
  const { map, tile } = o;
  const W = map.w * tile;
  const H = map.h * tile;
  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = COL.bg;
  ctx.fillRect(0, 0, W, H);

  const powerCut = o.snapshot?.powerCut ?? false;
  const doorsOpen = new Set(o.snapshot?.doorsOpen ?? []);

  // --- Tiles ---
  for (let y = 0; y < map.h; y++) {
    for (let x = 0; x < map.w; x++) {
      const t = map.tiles[idx({ x, y }, map.w)];
      const px = x * tile;
      const py = y * tile;
      switch (t.kind) {
        case 'void':
          break;
        case 'outdoor':
          ctx.fillStyle = COL.outdoor;
          ctx.fillRect(px, py, tile, tile);
          break;
        case 'exit':
          ctx.fillStyle = COL.outdoor;
          ctx.fillRect(px, py, tile, tile);
          ctx.strokeStyle = COL.exit;
          ctx.lineWidth = 2;
          ctx.strokeRect(px + 2, py + 2, tile - 4, tile - 4);
          drawGlyph(ctx, '🚗', px, py, tile);
          break;
        case 'floor':
          ctx.fillStyle = COL.floor;
          ctx.fillRect(px, py, tile, tile);
          ctx.strokeStyle = COL.floorGrid;
          ctx.lineWidth = 1;
          ctx.strokeRect(px + 0.5, py + 0.5, tile - 1, tile - 1);
          break;
        case 'furniture':
          ctx.fillStyle = COL.floor;
          ctx.fillRect(px, py, tile, tile);
          ctx.fillStyle = COL.furniture;
          ctx.fillRect(px + 3, py + 3, tile - 6, tile - 6);
          break;
        case 'wall':
          ctx.fillStyle = COL.wall;
          ctx.fillRect(px, py, tile, tile);
          ctx.fillStyle = COL.wallTop;
          ctx.fillRect(px, py, tile, 3);
          break;
        case 'door': {
          const open = t.lockLevel === 0 || doorsOpen.has(idx({ x, y }, map.w));
          ctx.fillStyle = COL.floor;
          ctx.fillRect(px, py, tile, tile);
          const c = t.lockLevel >= 3 ? COL.doorLock3 : t.lockLevel === 2 ? COL.doorLock2 : COL.door;
          ctx.strokeStyle = c;
          ctx.lineWidth = 2;
          if (open) {
            ctx.strokeRect(px + 2, py + 2, tile - 4, tile - 4);
            ctx.globalAlpha = 0.35;
          }
          ctx.fillStyle = c;
          ctx.fillRect(px + tile * 0.2, py + tile * 0.2, tile * 0.6, tile * 0.6);
          ctx.globalAlpha = 1;
          if (t.lockLevel > 0 && !open) {
            drawLabel(ctx, String(t.lockLevel), px + tile / 2, py + tile / 2, '#1a1440');
          }
          break;
        }
        case 'window': {
          const open = doorsOpen.has(idx({ x, y }, map.w));
          ctx.fillStyle = COL.floor;
          ctx.fillRect(px, py, tile, tile);
          ctx.strokeStyle = COL.window;
          ctx.lineWidth = open ? 1 : 3;
          ctx.globalAlpha = open ? 0.4 : 1;
          ctx.strokeRect(px + 2, py + 2, tile - 4, tile - 4);
          ctx.globalAlpha = 1;
          break;
        }
        case 'climb':
          ctx.fillStyle = COL.wall;
          ctx.fillRect(px, py, tile, tile);
          ctx.strokeStyle = COL.climb;
          ctx.lineWidth = 2;
          ctx.beginPath();
          for (let i = 0; i < 3; i++) {
            ctx.moveTo(px + 3, py + 4 + i * (tile / 3));
            ctx.lineTo(px + tile - 3, py + 4 + i * (tile / 3));
          }
          ctx.stroke();
          break;
      }
    }
  }

  // --- Lampenlicht (dezent) ---
  if (!powerCut) {
    for (const lamp of map.lamps) {
      const g = ctx.createRadialGradient(
        (lamp.pos.x + 0.5) * tile,
        (lamp.pos.y + 0.5) * tile,
        2,
        (lamp.pos.x + 0.5) * tile,
        (lamp.pos.y + 0.5) * tile,
        lamp.radius * tile,
      );
      g.addColorStop(0, 'rgba(255,206,84,0.18)');
      g.addColorStop(1, 'rgba(255,206,84,0)');
      ctx.fillStyle = g;
      ctx.fillRect(0, 0, W, H);
    }
  }

  // --- Sensoren (nur bei Intel >= 3 oder Live) ---
  const showSensors = o.intel >= 3 || o.snapshot;
  if (showSensors) {
    for (const s of map.sensors) {
      drawSensor(ctx, s, tile, powerCut, o.snapshot);
    }
  }

  // --- Behälter ---
  for (const c of map.containers) {
    const px = c.pos.x * tile;
    const py = c.pos.y * tile;
    ctx.fillStyle = c.kind === 'safe' ? COL.safe : 'rgba(35,224,224,0.15)';
    ctx.fillRect(px + 2, py + 2, tile - 4, tile - 4);
    ctx.strokeStyle = c.kind === 'safe' ? COL.safe : COL.vitrine;
    ctx.lineWidth = 2;
    ctx.strokeRect(px + 2, py + 2, tile - 4, tile - 4);
    drawGlyph(ctx, c.kind === 'safe' ? '🔒' : '💠', px, py, tile);
    if (c.level > 1) drawLabel(ctx, 'S' + c.level, px + tile - 8, py + 8, '#1a1440');
  }

  // --- Freiliegende Beute ---
  for (const l of map.loot) {
    if (!l.pos) continue;
    drawGlyph(ctx, l.icon, l.pos.x * tile, l.pos.y * tile, tile);
  }

  // --- Sicherungskasten / Telefon ---
  if (map.fusebox) drawGlyph(ctx, '⚡', map.fusebox.x * tile, map.fusebox.y * tile, tile);
  if (map.phone) drawGlyph(ctx, '☎️', map.phone.x * tile, map.phone.y * tile, tile);

  // --- Wach-Routen (nur bei Intel >= 2) ---
  if (o.intel >= 2 && !o.snapshot) {
    for (const g of map.guards) {
      ctx.strokeStyle = 'rgba(255,84,112,0.35)';
      ctx.lineWidth = 1.5;
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      g.route.forEach((wp, i) => {
        const cx = (wp.pos.x + 0.5) * tile;
        const cy = (wp.pos.y + 0.5) * tile;
        if (i === 0) ctx.moveTo(cx, cy);
        else ctx.lineTo(cx, cy);
      });
      if (g.route.length > 1) ctx.lineTo((g.route[0].pos.x + 0.5) * tile, (g.route[0].pos.y + 0.5) * tile);
      ctx.stroke();
      ctx.setLineDash([]);
      for (const wp of g.route) {
        ctx.fillStyle = 'rgba(255,84,112,0.5)';
        ctx.beginPath();
        ctx.arc((wp.pos.x + 0.5) * tile, (wp.pos.y + 0.5) * tile, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  // --- Geplanter Pfad + Schrittmarken ---
  if (o.compiled) {
    drawPlannedPath(ctx, o.compiled, tile, o.activeStep);
  }

  // --- Wachen (Live) ---
  if (o.snapshot) {
    for (const g of o.snapshot.guards) {
      drawGuard(ctx, g, tile);
    }
    // --- Spieler ---
    drawPlayer(ctx, o.snapshot, tile);
  }

  // --- Auswahl / Hover ---
  if (o.selected) {
    ctx.strokeStyle = COL.loot;
    ctx.lineWidth = 2;
    ctx.strokeRect(o.selected.x * tile + 1, o.selected.y * tile + 1, tile - 2, tile - 2);
  }
  if (o.hover) {
    ctx.strokeStyle = 'rgba(255,47,185,0.8)';
    ctx.lineWidth = 2;
    ctx.strokeRect(o.hover.x * tile + 1, o.hover.y * tile + 1, tile - 2, tile - 2);
  }
}

function drawSensor(
  ctx: CanvasRenderingContext2D,
  s: Sensor,
  tile: number,
  powerCut: boolean,
  snap?: Snapshot | null,
): void {
  const disabled = snap ? false : false;
  const cx = (s.pos.x + 0.5) * tile;
  const cy = (s.pos.y + 0.5) * tile;
  const off = (s.kind === 'camera' && powerCut) || disabled;
  if (s.kind === 'plate') {
    ctx.fillStyle = off ? 'rgba(255,159,69,0.15)' : 'rgba(255,159,69,0.5)';
    ctx.fillRect(s.pos.x * tile + 3, s.pos.y * tile + 3, tile - 6, tile - 6);
    ctx.strokeStyle = COL.plate;
    ctx.strokeRect(s.pos.x * tile + 3, s.pos.y * tile + 3, tile - 6, tile - 6);
    return;
  }
  const col = s.kind === 'camera' ? COL.camera : COL.sensor;
  // Kegel
  if (!off) {
    const facing = s.kind === 'camera' && s.sweep ? s.dir : s.dir;
    ctx.fillStyle =
      s.kind === 'camera' ? 'rgba(255,47,185,0.10)' : 'rgba(255,84,112,0.10)';
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, s.range * tile, facing - s.fov / 2, facing + s.fov / 2);
    ctx.closePath();
    ctx.fill();
  }
  // Gehäuse
  ctx.fillStyle = off ? '#555' : col;
  ctx.beginPath();
  ctx.arc(cx, cy, tile * 0.22, 0, Math.PI * 2);
  ctx.fill();
  drawGlyph(ctx, s.kind === 'camera' ? '📹' : '🔺', s.pos.x * tile, s.pos.y * tile, tile, 0.7);
}

function drawPlannedPath(
  ctx: CanvasRenderingContext2D,
  compiled: CompiledPlan,
  tile: number,
  activeStep?: number,
): void {
  ctx.lineWidth = 2;
  ctx.strokeStyle = 'rgba(35,224,224,0.55)';
  ctx.beginPath();
  let started = false;
  for (const cs of compiled.steps) {
    for (const p of cs.path) {
      const cx = (p.x + 0.5) * tile;
      const cy = (p.y + 0.5) * tile;
      if (!started) {
        ctx.moveTo(cx, cy);
        started = true;
      } else ctx.lineTo(cx, cy);
    }
  }
  ctx.stroke();

  // Schrittmarken der Aktionen
  compiled.steps.forEach((cs, i) => {
    if (cs.step.action === 'moveTo') return;
    const pos = cs.step.target ?? cs.path[cs.path.length - 1];
    if (!pos) return;
    const cx = (pos.x + 0.5) * tile;
    const cy = (pos.y + 0.5) * tile;
    const active = i === activeStep;
    ctx.fillStyle = cs.warnings.length ? '#ff5470' : active ? '#ff2fb9' : '#8a5cff';
    ctx.beginPath();
    ctx.arc(cx, cy, active ? 9 : 7, 0, Math.PI * 2);
    ctx.fill();
    drawLabel(ctx, String(i + 1), cx, cy, '#fff', 10);
  });
}

function drawGuard(ctx: CanvasRenderingContext2D, g: Snapshot['guards'][number], tile: number): void {
  const cx = (g.pos.x + 0.5) * tile;
  const cy = (g.pos.y + 0.5) * tile;
  // Sichtkegel
  const col =
    g.state === 'chase' || g.state === 'phone'
      ? 'rgba(255,84,112,0.22)'
      : g.state === 'investigate'
        ? 'rgba(255,206,84,0.18)'
        : 'rgba(138,92,255,0.14)';
  ctx.fillStyle = col;
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.arc(cx, cy, 5 * tile, g.facing - 0.6, g.facing + 0.6);
  ctx.closePath();
  ctx.fill();
  // Körper
  ctx.fillStyle = g.state === 'patrol' ? '#ff5470' : g.state === 'investigate' ? '#ffce54' : '#ff2fb9';
  ctx.beginPath();
  ctx.arc(cx, cy, tile * 0.32, 0, Math.PI * 2);
  ctx.fill();
  drawGlyph(ctx, '👮', g.pos.x * tile, g.pos.y * tile, tile, 0.8);
  // Verdachtsbalken
  if (g.suspicion > 0.05 && g.state !== 'chase') {
    ctx.fillStyle = '#0a0820';
    ctx.fillRect(cx - 10, cy - tile * 0.5 - 6, 20, 4);
    ctx.fillStyle = g.suspicion >= 1 ? '#ff5470' : '#ffce54';
    ctx.fillRect(cx - 10, cy - tile * 0.5 - 6, 20 * Math.min(1, g.suspicion), 4);
  }
  if (g.state === 'chase' || g.state === 'phone') {
    drawLabel(ctx, '!', cx, cy - tile * 0.5 - 4, '#ff5470', 14);
  }
}

function drawPlayer(ctx: CanvasRenderingContext2D, snap: Snapshot, tile: number): void {
  const cx = (snap.player.x + 0.5) * tile;
  const cy = (snap.player.y + 0.5) * tile;
  ctx.shadowColor = snap.fleeing ? '#ff2fb9' : '#37e29a';
  ctx.shadowBlur = 12;
  ctx.fillStyle = snap.fleeing ? '#ff2fb9' : '#37e29a';
  ctx.beginPath();
  ctx.arc(cx, cy, tile * 0.34, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;
  drawGlyph(ctx, '🕵️', snap.player.x * tile, snap.player.y * tile, tile, 0.85);
}

function drawGlyph(
  ctx: CanvasRenderingContext2D,
  glyph: string,
  px: number,
  py: number,
  tile: number,
  scale = 0.75,
): void {
  ctx.font = `${Math.floor(tile * scale)}px serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(glyph, px + tile / 2, py + tile / 2 + 1);
}

function drawLabel(
  ctx: CanvasRenderingContext2D,
  text: string,
  cx: number,
  cy: number,
  color: string,
  size = 11,
): void {
  ctx.font = `bold ${size}px 'Courier New', monospace`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillStyle = color;
  ctx.fillText(text, cx, cy);
}
