import { describe, expect, it } from 'vitest';
import { addXp, buildProfile, haggleFactor, newCharacter } from './character';
import { fencePrice, type StashItem } from './economy';
import { trainingResult, TRAININGS } from './data/training';

describe('Charakter / XP', () => {
  it('sammelt XP und steigt im Level auf', () => {
    const c = newCharacter('Test');
    const before = c.level;
    const r = addXp(c, 1000);
    expect(c.level).toBeGreaterThan(before);
    expect(r.leveledTo).toBe(c.level);
    // Level-Up vergibt Skillpunkte und (alle 2 Level) Perk-Slots
    expect(c.skillPoints).toBeGreaterThan(5);
    expect(c.perkSlots).toBeGreaterThanOrEqual(1);
  });
});

describe('buildProfile: Perk- und Item-Effekte', () => {
  it('Katzenpfoten senkt den Schleichlärm', () => {
    const base = newCharacter('A');
    const withPerk = newCharacter('B');
    withPerk.perks.push('katzenpfoten');
    expect(buildProfile(withPerk).sneakNoise).toBeLessThan(buildProfile(base).sneakNoise);
  });

  it('Muskelprotz erhöht die Tragkraft', () => {
    const base = newCharacter('A');
    const buff = newCharacter('B');
    buff.perks.push('muskelprotz');
    expect(buildProfile(buff).carryCapacity).toBeGreaterThan(buildProfile(base).carryCapacity);
  });

  it('Turnschuhe im Loadout halbieren den Gehlärm', () => {
    const base = newCharacter('A');
    const shod = newCharacter('B');
    shod.owned.push('turnschuhe');
    shod.loadout.push('turnschuhe');
    expect(buildProfile(shod).sneakNoise).toBeLessThan(buildProfile(base).sneakNoise);
  });

  it('Feilscher verbessert den Handelsfaktor', () => {
    const base = newCharacter('A');
    const deal = newCharacter('B');
    deal.perks.push('feilscher');
    expect(haggleFactor(deal)).toBeGreaterThan(haggleFactor(base));
  });
});

describe('Hehler-Ökonomie', () => {
  const hotItem: StashItem = {
    id: 'x',
    name: 'Diamant',
    icon: '💎',
    value: 1000,
    weight: 1,
    hot: true,
    acquiredDay: 1,
  };

  it('heiße Ware bringt frisch weniger als abgekühlt', () => {
    const fresh = fencePrice(hotItem, 1, 0.8, 0);
    const cooled = fencePrice(hotItem, 10, 0.8, 0);
    expect(cooled).toBeGreaterThan(fresh);
  });

  it('hohe Fahndung drückt den Preis', () => {
    const calm = fencePrice(hotItem, 10, 0.8, 0);
    const hot = fencePrice(hotItem, 10, 0.8, 90);
    expect(hot).toBeLessThan(calm);
  });
});

describe('Training: abnehmender Ertrag', () => {
  it('bringt bei niedrigem Wert mehr als bei hohem', () => {
    const t = TRAININGS.find((x) => x.target.kind === 'skill')!;
    const low = trainingResult(t, 5);
    const high = trainingResult(t, 90);
    expect(low).toBeGreaterThan(high);
  });
  it('Stats sind bei 10 gedeckelt', () => {
    const t = TRAININGS.find((x) => x.target.kind === 'stat')!;
    expect(trainingResult(t, 10)).toBe(0);
  });
});
