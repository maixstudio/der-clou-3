// Ökonomie-Helfer: Hehlerpreise, „heiße Ware" und Fahndungs-Malus.

import type { LootItem } from '../engine/types';
import { clamp } from '../engine/types';

/** In den Safe/Kleiderschrank gelegte Beute wartet auf besseren Verkauf. */
export interface StashItem extends LootItem {
  /** Spieltag, an dem die Beute erbeutet wurde */
  acquiredDay: number;
}

/**
 * Verkaufspreis beim Hehler.
 * - haggle: Handelsfaktor (0.5..1.05) aus Skill + Perk
 * - wanted: aktuelle Fahndung (0..100) drückt den Preis
 * - heiße Ware bringt frisch nur einen Bruchteil, „kühlt" über Tage ab
 */
export function fencePrice(item: StashItem, currentDay: number, haggle: number, wanted: number): number {
  let price = item.value * haggle;
  const wantedMalus = 1 - (wanted / 100) * 0.35; // bis −35 %
  price *= wantedMalus;
  if (item.hot) {
    const daysHeld = currentDay - item.acquiredDay;
    const coolFactor = clamp(0.45 + daysHeld * 0.09, 0.45, 1); // nach ~6 Tagen abgekühlt
    price *= coolFactor;
  }
  return Math.round(price);
}

/** Tage, bis heiße Ware vollständig „abgekühlt" (voller Preis) ist. */
export function coolDaysLeft(item: StashItem, currentDay: number): number {
  if (!item.hot) return 0;
  const held = currentDay - item.acquiredDay;
  return Math.max(0, Math.ceil((1 - 0.45) / 0.09) - held);
}
