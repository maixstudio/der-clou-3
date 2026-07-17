import { useEffect, useRef, useState } from 'react';
import type { CompiledPlan, HeistMap, Snapshot, Vec } from '../../engine/types';
import { clamp } from '../../engine/types';
import { renderScene } from './render';

interface Props {
  map: HeistMap;
  compiled?: CompiledPlan | null;
  snapshot?: Snapshot | null;
  intel: number;
  activeStep?: number;
  selected?: Vec | null;
  onPick?: (tile: Vec, screen: { x: number; y: number }) => void;
}

export function FloorplanCanvas({ map, compiled, snapshot, intel, activeStep, selected, onPick }: Props) {
  const ref = useRef<HTMLCanvasElement>(null);
  const [hover, setHover] = useState<Vec | null>(null);
  const tile = clamp(Math.floor(900 / map.w), 16, 32);
  const W = map.w * tile;
  const H = map.h * tile;

  useEffect(() => {
    const c = ref.current;
    if (!c) return;
    const dpr = window.devicePixelRatio || 1;
    c.width = W * dpr;
    c.height = H * dpr;
    c.style.width = `${W}px`;
    c.style.height = `${H}px`;
    const ctx = c.getContext('2d')!;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    renderScene(ctx, { map, tile, compiled, snapshot, hover, selected, intel, activeStep });
  }, [map, tile, W, H, compiled, snapshot, hover, selected, intel, activeStep]);

  const toTile = (e: React.MouseEvent): Vec => {
    const rect = ref.current!.getBoundingClientRect();
    const x = clamp(Math.floor((e.clientX - rect.left) / tile), 0, map.w - 1);
    const y = clamp(Math.floor((e.clientY - rect.top) / tile), 0, map.h - 1);
    return { x, y };
  };

  return (
    <canvas
      ref={ref}
      onMouseMove={(e) => setHover(toTile(e))}
      onMouseLeave={() => setHover(null)}
      onClick={(e) => onPick?.(toTile(e), { x: e.clientX, y: e.clientY })}
      style={{ cursor: onPick ? 'crosshair' : 'default' }}
    />
  );
}
