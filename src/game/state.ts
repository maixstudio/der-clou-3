// Zentraler Spielzustand (Zustand-Store mit localStorage-Persistenz).
// Hält Charakter, Geld, Kalender, Fahndung, Beutelager und Fortschritt und
// stellt alle Meta-Aktionen (kaufen, trainieren, schlafen, ausführen …) bereit.

import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import type { ToolId, Plan, LootItem } from '../engine/types';
import { clamp } from '../engine/types';
import { parseHeist } from '../engine/parse';
import { simulate } from '../engine/simulate';
import { scoreResult, type JobReport } from '../engine/scoring';
import {
  addXp,
  buildProfile,
  haggleFactor,
  newCharacter,
  wantedDecayPerDay,
  type Character,
} from './character';
import { ITEM_BY_ID } from './data/items';
import { VEHICLE_BY_ID } from './data/vehicles';
import { TRAININGS, trainingResult } from './data/training';
import { PERK_BY_ID, type PerkId } from './data/perksData';
import { HEIST_BY_ID, HEISTS } from './data/heists';
import { fencePrice, type StashItem } from './economy';

export type Screen =
  | 'menu'
  | 'hub'
  | 'character'
  | 'inventory'
  | 'shop'
  | 'garage'
  | 'training'
  | 'fence'
  | 'informant'
  | 'heists'
  | 'plan'
  | 'exec'
  | 'debrief';

export interface LogEntry {
  day: number;
  text: string;
}

interface Persisted {
  started: boolean;
  character: Character;
  money: number;
  day: number;
  wanted: number;
  stash: StashItem[];
  unlocked: string[]; // freigeschaltete Heist-Ids
  intel: Record<string, number>; // Auskundschaftungsstufe je Heist
  completed: string[];
  plans: Record<string, Plan>; // gespeicherte Pläne je Heist
  log: LogEntry[];
}

interface Transient {
  screen: Screen;
  selectedHeist: string | null;
  lastReport: JobReport | null;
}

export interface GameState extends Persisted, Transient {
  // Navigation
  go: (s: Screen) => void;
  // Spielstart
  newGame: (name: string) => void;
  resetGame: () => void;
  // Shop / Garage
  buyItem: (id: ToolId) => void;
  toggleLoadout: (id: ToolId) => void;
  buyVehicle: (id: string) => void;
  setVehicle: (id: string) => void;
  // Training
  train: (trainingId: string) => void;
  // Charakter
  raiseSkill: (id: keyof Character['skills']) => void;
  choosePerk: (id: PerkId) => void;
  // Kalender / Wohnung
  sleep: (nights: number) => void;
  // Informant
  buyTip: (heistId: string) => void;
  buyIntel: (heistId: string) => void;
  // Heist
  selectHeist: (heistId: string) => void;
  savePlan: (plan: Plan) => void;
  executeHeist: (seed: number, abortAt?: number) => void;
  // Hehler
  sellItem: (index: number) => void;
  sellAll: () => void;
  addLog: (text: string) => void;
}

const START_MONEY = 800;

function initialPersisted(): Persisted {
  return {
    started: false,
    character: newCharacter('Neuling'),
    money: START_MONEY,
    day: 1,
    wanted: 0,
    stash: [],
    unlocked: HEISTS.filter((h) => h.unlockCost === 0).map((h) => h.id),
    intel: {},
    completed: [],
    plans: {},
    log: [],
  };
}

export const useGame = create<GameState>()(
  persist(
    (set, get) => ({
      ...initialPersisted(),
      screen: 'menu',
      selectedHeist: null,
      lastReport: null,

      go: (s) => set({ screen: s }),

      newGame: (name) => {
        const base = initialPersisted();
        base.started = true;
        base.character = newCharacter(name || 'Der Boss');
        set({
          ...base,
          screen: 'hub',
          selectedHeist: null,
          lastReport: null,
        });
        get().addLog(`${base.character.name} steigt ins Geschäft ein. Willkommen im Milieu.`);
      },

      resetGame: () => {
        set({
          ...initialPersisted(),
          screen: 'menu',
          selectedHeist: null,
          lastReport: null,
        });
      },

      buyItem: (id) => {
        const s = get();
        const item = ITEM_BY_ID[id];
        if (!item || s.character.owned.includes(id) || s.money < item.price) return;
        const c = structuredClone(s.character);
        c.owned.push(id);
        c.loadout.push(id); // gekauftes Werkzeug direkt einpacken
        set({ character: c, money: s.money - item.price });
        get().addLog(`${item.name} bei Werkzeug-Willi gekauft (−${item.price} DM).`);
      },

      toggleLoadout: (id) => {
        const s = get();
        if (!s.character.owned.includes(id)) return;
        const c = structuredClone(s.character);
        if (c.loadout.includes(id)) c.loadout = c.loadout.filter((x) => x !== id);
        else c.loadout.push(id);
        set({ character: c });
      },

      buyVehicle: (id) => {
        const s = get();
        const v = VEHICLE_BY_ID[id];
        if (!v || s.character.vehicles.includes(id) || s.money < v.price) return;
        const c = structuredClone(s.character);
        c.vehicles.push(id);
        c.activeVehicle = id;
        set({ character: c, money: s.money - v.price });
        get().addLog(`${v.name} bei Bennos Karren gekauft (−${v.price} DM).`);
      },

      setVehicle: (id) => {
        const s = get();
        if (!s.character.vehicles.includes(id)) return;
        const c = structuredClone(s.character);
        c.activeVehicle = id;
        set({ character: c });
      },

      train: (trainingId) => {
        const s = get();
        const t = TRAININGS.find((x) => x.id === trainingId);
        if (!t || s.money < t.cost) return;
        const c = structuredClone(s.character);
        let msg = '';
        if (t.target.kind === 'skill') {
          const cur = c.skills[t.target.id];
          const gain = trainingResult(t, cur);
          c.skills[t.target.id] = clamp(cur + gain, 0, 100);
          msg = `${t.name}: +${gain} ${t.target.id}.`;
        } else {
          const cur = c.stats[t.target.id];
          const gain = trainingResult(t, cur);
          if (gain === 0) {
            get().addLog(`${t.target.id} ist bereits am Maximum.`);
            return;
          }
          c.stats[t.target.id] = clamp(cur + gain, 1, 10);
          msg = `${t.name}: +${gain} ${t.target.id}.`;
        }
        // XP fürs Training
        addXp(c, Math.round(t.cost / 20));
        set({ character: c, money: s.money - t.cost });
        get().addLog(msg);
        get().sleep(t.days);
      },

      raiseSkill: (id) => {
        const s = get();
        const c = structuredClone(s.character);
        if (c.skillPoints <= 0 || c.skills[id] >= 100) return;
        c.skills[id] = clamp(c.skills[id] + 5, 0, 100);
        c.skillPoints -= 1;
        set({ character: c });
      },

      choosePerk: (id) => {
        const s = get();
        const c = structuredClone(s.character);
        const def = PERK_BY_ID[id];
        if (!def || c.perks.includes(id) || c.perkSlots <= 0 || c.level < def.minLevel) return;
        c.perks.push(id);
        c.perkSlots -= 1;
        set({ character: c });
        get().addLog(`Neue Fähigkeit erlernt: ${def.name}.`);
      },

      sleep: (nights) => {
        const s = get();
        const c = structuredClone(s.character);
        const decay = wantedDecayPerDay(c) * nights;
        c.stamina = clamp(c.stamina + nights * 40, 0, 100);
        set({
          day: s.day + nights,
          wanted: clamp(s.wanted - decay, 0, 100),
          character: c,
        });
      },

      buyTip: (heistId) => {
        const s = get();
        const def = HEIST_BY_ID[heistId];
        if (!def || s.unlocked.includes(heistId) || s.money < def.unlockCost) return;
        set({ unlocked: [...s.unlocked, heistId], money: s.money - def.unlockCost });
        get().addLog(`Kalle steckt dir einen Tipp: „${def.name}" ist jetzt spruchreif.`);
      },

      buyIntel: (heistId) => {
        const s = get();
        const def = HEIST_BY_ID[heistId];
        if (!def) return;
        const cur = s.intel[heistId] ?? 0;
        if (cur >= 3) return;
        const cost = def.intelCosts[cur];
        if (s.money < cost) return;
        set({ intel: { ...s.intel, [heistId]: cur + 1 }, money: s.money - cost });
        const labels = ['Grundriss-Details', 'Wachrouten', 'Sensorpläne'];
        get().addLog(`Auskundschaften „${def.name}": ${labels[cur]} beschafft (−${cost} DM).`);
      },

      selectHeist: (heistId) => set({ selectedHeist: heistId, screen: 'plan' }),

      savePlan: (plan) => {
        const s = get();
        set({ plans: { ...s.plans, [plan.heistId]: plan } });
      },

      executeHeist: (seed, abortAt) => {
        const s = get();
        if (!s.selectedHeist) return;
        const def = HEIST_BY_ID[s.selectedHeist];
        const plan = s.plans[s.selectedHeist];
        if (!def || !plan) return;
        const map = parseHeist(def);
        const profile = buildProfile(s.character);
        const intel = s.intel[def.id] ?? 0;
        const result = simulate(plan, map, profile, { mode: 'exec', seed, intel, abortAt });
        const report = scoreResult(result, def.xpBase, s.wanted);

        const c = structuredClone(s.character);
        const level = addXp(c, report.xp);
        c.stamina = clamp(c.stamina - 30, 0, 100);

        let money = s.money - report.fineDM;
        const stash = [...s.stash];
        for (const l of report.loot) {
          stash.push({ ...(l as LootItem), acquiredDay: s.day + (report.jailDays || 0) });
        }
        const completed = report.success && !s.completed.includes(def.id)
          ? [...s.completed, def.id]
          : s.completed;

        set({
          character: c,
          money: Math.max(0, money),
          wanted: clamp(s.wanted + report.wantedDelta, 0, 100),
          day: s.day + 1 + (report.jailDays || 0),
          stash,
          completed,
          lastReport: report,
          screen: 'debrief',
        });

        get().addLog(report.headline);
        if (report.jailDays > 0) get().addLog(`Erwischt! ${report.jailDays} Tage gesessen, ${report.fineDM} DM Strafe.`);
        if (level.leveledTo) get().addLog(`Aufgestiegen auf Stufe ${level.leveledTo}! Neue Punkte verfügbar.`);
      },

      sellItem: (index) => {
        const s = get();
        const item = s.stash[index];
        if (!item) return;
        const price = fencePrice(item, s.day, haggleFactor(s.character), s.wanted);
        const stash = s.stash.filter((_, i) => i !== index);
        set({ stash, money: s.money + price });
        get().addLog(`${item.name} an Goldfinger-Gerd verkauft (+${price} DM).`);
      },

      sellAll: () => {
        const s = get();
        let money = s.money;
        let sum = 0;
        for (const item of s.stash) {
          sum += fencePrice(item, s.day, haggleFactor(s.character), s.wanted);
        }
        money += sum;
        set({ stash: [], money });
        if (s.stash.length > 0) get().addLog(`Gesamte Beute verkauft (+${sum} DM).`);
      },

      addLog: (text) => {
        const s = get();
        const log = [{ day: s.day, text }, ...s.log].slice(0, 40);
        set({ log });
      },
    }),
    {
      name: 'der-clou-3-save',
      partialize: (s): Persisted => ({
        started: s.started,
        character: s.character,
        money: s.money,
        day: s.day,
        wanted: s.wanted,
        stash: s.stash,
        unlocked: s.unlocked,
        intel: s.intel,
        completed: s.completed,
        plans: s.plans,
        log: s.log,
      }),
    },
  ),
);
