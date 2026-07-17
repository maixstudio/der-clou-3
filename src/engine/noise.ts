// Lärmmodell: Aktionen erzeugen Lärm mit Radius; Wände und geschlossene
// Türen dämpfen. Wachen reagieren auf die wahrgenommene Lautstärke.

import type { HeistMap, Vec } from './types';
import { dist } from './types';
import { wallsBetween } from './los';

/**
 * Wahrgenommene Lautstärke (0..1) eines Geräuschs bei `listener`.
 * radius = Lärmradius in Tiles bei freier Ausbreitung.
 */
export function noiseAt(
  map: HeistMap,
  source: Vec,
  radius: number,
  listener: Vec,
  opened: Set<number>,
): number {
  if (radius <= 0) return 0;
  const walls = wallsBetween(map, source, listener, opened);
  const effective = radius - walls * 2;
  if (effective <= 0) return 0;
  const d = dist(source, listener);
  if (d >= effective) return 0;
  return 1 - d / effective;
}
