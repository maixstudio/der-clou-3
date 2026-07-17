// Werkzeug-Katalog für „Werkzeug-Willi". Jedes Item schaltet Aktionen frei
// oder verbessert Kennwerte. Preise in DM (80er-Jahre-Niveau).

import type { ToolId } from '../../engine/types';

export type ItemCategory = 'schloss' | 'safe' | 'glas' | 'elektronik' | 'ablenkung' | 'zugang' | 'traglast' | 'sonstiges';

export interface ItemDef {
  id: ToolId;
  name: string;
  icon: string;
  category: ItemCategory;
  price: number;
  weight: number; // kg im Loadout
  desc: string;
  /** Voraussetzung: anderes Item muss besessen werden */
  requires?: ToolId;
}

export const ITEMS: ItemDef[] = [
  // Schlösser
  { id: 'dietrich1', name: 'Dietrich-Set „Anfänger"', icon: '🗝️', category: 'schloss', price: 250, weight: 0.3, desc: 'Öffnet einfache Schlösser (Stufe 1).' },
  { id: 'dietrich2', name: 'Dietrich-Set „Profi"', icon: '🗝️', category: 'schloss', price: 900, weight: 0.4, desc: 'Öffnet Schlösser bis Stufe 2.' },
  { id: 'dietrich3', name: 'Dietrich-Set „Meister"', icon: '🗝️', category: 'schloss', price: 2600, weight: 0.5, desc: 'Öffnet auch komplexe Schlösser (Stufe 3).' },
  { id: 'brecheisen', name: 'Brecheisen', icon: '🪛', category: 'schloss', price: 180, weight: 2.5, desc: 'Bricht Türen roh auf – schnell, aber sehr laut.' },

  // Safes
  { id: 'stethoskop', name: 'Stethoskop', icon: '🩺', category: 'safe', price: 700, weight: 0.3, desc: 'Safe nach Gehör knacken – leise, aber langsam.' },
  { id: 'bohrmaschine', name: 'Bohrmaschine', icon: '🔩', category: 'safe', price: 1400, weight: 3, desc: 'Bohrt Safes bis Stufe 2 auf. Mittellaut.' },
  { id: 'winkelschleifer', name: 'Winkelschleifer', icon: '⚙️', category: 'safe', price: 2200, weight: 4, desc: 'Flext jeden Safe auf – funkensprühend laut.' },

  // Glas
  { id: 'glasschneider', name: 'Glasschneider', icon: '🔪', category: 'glas', price: 320, weight: 0.4, desc: 'Schneidet Fenster & Vitrinen leise auf.' },

  // Elektronik
  { id: 'drahtschere', name: 'Drahtschere', icon: '✂️', category: 'elektronik', price: 220, weight: 0.5, desc: 'Kappt Strom, Telefon und verdrahtete Sensoren.' },
  { id: 'loetkolben', name: 'Lötkolben-Set', icon: '🔌', category: 'elektronik', price: 640, weight: 1, desc: 'Manipuliert Kameras, ohne Alarm auszulösen.' },
  { id: 'stoersender', name: 'Funk-Störsender', icon: '📡', category: 'elektronik', price: 1800, weight: 1.2, desc: 'Unterdrückt stille Funkalarme an Safes & Vitrinen.' },

  // Ablenkung
  { id: 'rekorder', name: 'Kassettenrekorder', icon: '📻', category: 'ablenkung', price: 400, weight: 1, desc: 'Abgestellt lockt er Wachen mit Lärm an einen Ort.' },

  // Zugang
  { id: 'seil', name: 'Seil mit Wurfhaken', icon: '🪝', category: 'zugang', price: 350, weight: 2, desc: 'Erklimmt Fassaden und Kletterstellen.' },
  { id: 'turnschuhe', name: 'Leise Turnschuhe', icon: '👟', category: 'zugang', price: 300, weight: 0.8, desc: 'Halbiert den Gehlärm beim Schleichen.' },
  { id: 'handschuhe', name: 'Lederhandschuhe', icon: '🧤', category: 'sonstiges', price: 150, weight: 0.2, desc: 'Keine Fingerabdrücke – weniger Fahndung nach lauten Coups.' },

  // Traglast
  { id: 'sporttasche', name: 'Sporttasche', icon: '🎒', category: 'traglast', price: 200, weight: 0.5, desc: '+15 kg Tragkraft für die Beute.' },
  { id: 'rucksack', name: 'Großer Rucksack', icon: '🎒', category: 'traglast', price: 550, weight: 0.8, desc: '+30 kg Tragkraft. Ersetzt die Sporttasche.' },
];

export const ITEM_BY_ID: Record<ToolId, ItemDef> = Object.fromEntries(
  ITEMS.map((i) => [i.id, i]),
) as Record<ToolId, ItemDef>;

export const CATEGORY_LABEL: Record<ItemCategory, string> = {
  schloss: 'Schlösser & Türen',
  safe: 'Safeknacken',
  glas: 'Glas',
  elektronik: 'Elektronik',
  ablenkung: 'Ablenkung',
  zugang: 'Zugang & Schleichen',
  traglast: 'Traglast',
  sonstiges: 'Sonstiges',
};

/** Zusätzliche Tragkraft in kg durch getragene Taschen. */
export function carryBonus(tools: ToolId[]): number {
  let bonus = 0;
  if (tools.includes('rucksack')) bonus += 30;
  else if (tools.includes('sporttasche')) bonus += 15;
  return bonus;
}
