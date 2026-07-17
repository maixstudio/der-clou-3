// Seedbarer RNG (mulberry32): gleiche Seeds ⇒ identische Simulationsläufe.

export interface Rng {
  /** [0, 1) */
  next(): number;
  /** [lo, hi) */
  range(lo: number, hi: number): number;
  /** true mit Wahrscheinlichkeit p */
  chance(p: number): boolean;
}

export function makeRng(seed: number): Rng {
  let a = seed >>> 0;
  const next = (): number => {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  return {
    next,
    range: (lo, hi) => lo + next() * (hi - lo),
    chance: (p) => next() < p,
  };
}

/** Vorschau-Modus: keine Zufälle – Erwartungswerte statt Würfe. */
export function makeExpectedRng(): Rng {
  return {
    next: () => 0.5,
    range: (lo, hi) => (lo + hi) / 2,
    chance: () => false,
  };
}
