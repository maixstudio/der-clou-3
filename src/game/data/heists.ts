// Die 8 Einbruchsobjekte mit steigender Schwierigkeit. Grundrisse als ASCII
// (siehe parse.ts für die Zeichen). Marker-Legende löst Beute/Safes/Sensoren/
// Lampen/Wach-Wegpunkte auf. Wegpunkte sind Kleinbuchstaben (p,q,r,s,t,u,v).

import type { HeistDef } from '../../engine/parse';

export const HEISTS: HeistDef[] = [
  // 1 — KIOSK (Tutorial): keine Wachen, 1 Schloss, offene Kasse ----------------
  {
    id: 'kiosk',
    name: 'Späti „Nachteule"',
    desc: 'Ein kleiner Kiosk. Ideal für den ersten Bruch: nur ein simples Türschloss, keine Wachen.',
    difficulty: 1,
    unlockCost: 0,
    intelCosts: [0, 0, 0],
    policeBase: 45,
    baseLight: 0.35,
    xpBase: 20,
    map: [
      '###########',
      '#Z.......K#',
      '#.........#',
      '#....f....#',
      '##1########',
      ' ,E,       ',
    ],
    legend: {
      Z: { obj: 'loot', name: 'Stange Zigaretten', icon: '🚬', value: 220, weight: 2, hot: true },
      K: { obj: 'loot', name: 'Tageskasse', icon: '💵', value: 480, weight: 1 },
    },
    guards: [],
  },

  // 2 — VIDEOTHEK: 1 Wache, Kasse + Sammlerkassetten --------------------------
  {
    id: 'videothek',
    name: 'Videothek „Kassettenkönig"',
    desc: 'Ein Nachtwächter dreht seine Runden. Kasse und ein paar begehrte Sammlerkassetten warten.',
    difficulty: 2,
    unlockCost: 150,
    intelCosts: [80, 150, 200],
    policeBase: 40,
    baseLight: 0.4,
    xpBase: 32,
    map: [
      '################',
      '#K...p......q..#',
      '#..............#',
      '#..ff....ff..V.#',
      '#..............#',
      '#..r......s....#',
      '#....ff........#',
      '#######2########',
      '      ,E,       ',
    ],
    legend: {
      K: { obj: 'loot', name: 'Wochenkasse', icon: '💵', value: 650, weight: 1 },
      V: { obj: 'loot', name: 'Sammler-Kassetten', icon: '📼', value: 900, weight: 4, hot: true },
      p: { obj: 'waypoint' },
      q: { obj: 'waypoint' },
      r: { obj: 'waypoint' },
      s: { obj: 'waypoint' },
    },
    guards: [
      {
        name: 'Nachtwächter Kunze',
        speed: 1.3,
        visionRange: 6,
        route: [
          { at: 'p', wait: 2 },
          { at: 'q', wait: 2 },
          { at: 's', wait: 2 },
          { at: 'r', wait: 2 },
        ],
      },
    ],
  },

  // 3 — VILLA: Bewegungsmelder + Wandsafe -------------------------------------
  {
    id: 'villa',
    name: 'Villa am Stadtrand',
    desc: 'Der Hausherr ist verreist. Ein Bewegungsmelder sichert den Flur, der Schmuck liegt im Wandsafe.',
    difficulty: 2,
    unlockCost: 300,
    intelCosts: [120, 200, 280],
    policeBase: 38,
    baseLight: 0.25,
    xpBase: 42,
    map: [
      '##################',
      '#S....#....G....C#',
      '#.....#........###',
      '#..ff.2....M....w.',
      '#.....#........###',
      '#.....#...ff....##',
      '####1#############',
      '    ,E,           ',
    ],
    legend: {
      S: { obj: 'safe', name: 'Wandsafe', level: 1, loot: [
        { name: 'Goldschmuck', icon: '💍', value: 1600, weight: 1, hot: true },
        { name: 'Bargeldbündel', icon: '💵', value: 700, weight: 1 },
      ] },
      G: { obj: 'loot', name: 'Silbergemälde', icon: '🖼️', value: 1100, weight: 6, bulky: true },
      M: { obj: 'motion', dir: 'W', range: 5, fov: 1.4 },
    },
    guards: [],
  },

  // 4 — ELEKTRONIKLADEN: Kameras + Sicherungskasten ---------------------------
  {
    id: 'elektro',
    name: 'Elektronik-Center „Watt"',
    desc: 'Schwenkkameras überwachen den Verkaufsraum. Wer den Sicherungskasten findet, macht sie blind.',
    difficulty: 3,
    unlockCost: 500,
    intelCosts: [160, 260, 360],
    policeBase: 35,
    baseLight: 0.5,
    xpBase: 55,
    map: [
      '####################',
      '#F........A.......T#',
      '#..................#',
      '#..ff....ff....ff..#',
      '#..................#',
      '#K.......A.........#',
      '#..................#',
      '######3#######w#####',
      '     ,E,            ',
    ],
    legend: {
      F: { obj: 'fusebox' },
      T: { obj: 'loot', name: 'Fernseher-Neuware', icon: '📺', value: 800, weight: 12, bulky: true },
      K: { obj: 'loot', name: 'Ladenkasse', icon: '💵', value: 900, weight: 1 },
      A: { obj: 'camera', dir: 'S', range: 6, fov: 1.0, sweep: 0.6 },
    },
    guards: [],
  },

  // 5 — JUWELIER: Vitrinen mit Funkalarm, 2 Wachen ----------------------------
  {
    id: 'juwelier',
    name: 'Juwelier „Karat & Sohn"',
    desc: 'Edle Vitrinen mit stillem Funkalarm, zwei Wachen im Wechsel. Ohne Störsender geht hier nichts leise.',
    difficulty: 4,
    unlockCost: 800,
    intelCosts: [220, 340, 460],
    policeBase: 30,
    baseLight: 0.45,
    xpBase: 72,
    map: [
      '#####################',
      '#p........A........q#',
      '#...................#',
      '#..V....V....V....V.#',
      '#...................#',
      '#..r.....P.....s....#',
      '#...................#',
      '#..V....V....V....V.#',
      '#...................#',
      '#t........w........u#',
      '#########2###########',
      '        ,E,          ',
    ],
    legend: {
      V: { obj: 'vitrine', name: 'Schmuckvitrine', level: 1, funkalarm: true, loot: [
        { name: 'Diamantcollier', icon: '💎', value: 1400, weight: 0.5, hot: true },
      ] },
      A: { obj: 'camera', dir: 'S', range: 7, fov: 0.9, sweep: 0.7 },
      P: { obj: 'plate' },
      p: { obj: 'waypoint' },
      q: { obj: 'waypoint' },
      r: { obj: 'waypoint' },
      s: { obj: 'waypoint' },
      t: { obj: 'waypoint' },
      u: { obj: 'waypoint' },
    },
    guards: [
      {
        name: 'Wachmann Brandt',
        speed: 1.35,
        visionRange: 6,
        route: [
          { at: 'p', wait: 2 },
          { at: 'q', wait: 2 },
          { at: 'u', wait: 2 },
          { at: 't', wait: 2 },
        ],
      },
      {
        name: 'Wachmann Loose',
        speed: 1.35,
        visionRange: 6,
        route: [
          { at: 'r', wait: 3 },
          { at: 's', wait: 3 },
        ],
      },
    ],
  },

  // 6 — SPIELHALLE: Münzautomaten + Hinterzimmer-Safe --------------------------
  {
    id: 'spielhalle',
    name: 'Spielhalle „Goldnugget"',
    desc: 'Volle Münzautomaten und ein gut gefüllter Safe im Hinterzimmer. Der Wirt hat einen Wachmann angeheuert.',
    difficulty: 4,
    unlockCost: 1100,
    intelCosts: [260, 400, 540],
    policeBase: 32,
    baseLight: 0.55,
    xpBase: 85,
    map: [
      '######################',
      '#M....M....M..#S.....#',
      '#.............#......#',
      '#..p......q...3......#',
      '#..r..........#......#',
      '#M....M....M..#......#',
      '#.............########',
      '#......A.............#',
      '########3#############',
      '        ,E,           ',
    ],
    legend: {
      M: { obj: 'loot', name: 'Münzautomat-Kasse', icon: '🪙', value: 320, weight: 3, hot: true },
      S: { obj: 'safe', name: 'Wandtresor', level: 2, funkalarm: true, loot: [
        { name: 'Wochenumsatz', icon: '💵', value: 2200, weight: 2 },
      ] },
      A: { obj: 'camera', dir: 'N', range: 6, fov: 1.0, sweep: 0.5 },
      p: { obj: 'waypoint' },
      q: { obj: 'waypoint' },
      r: { obj: 'waypoint' },
    },
    guards: [
      {
        name: 'Türsteher Ferro',
        speed: 1.4,
        visionRange: 6,
        route: [
          { at: 'p', wait: 2 },
          { at: 'q', wait: 2 },
          { at: 'r', wait: 3 },
        ],
      },
    ],
  },

  // 7 — MUSEUM: Wachen + Kameras + Druckplatten, sperrige Beute -----------------
  {
    id: 'museum',
    name: 'Stadtmuseum',
    desc: 'Alles kombiniert: Kameras, Druckplatten unter den Exponaten, zwei Wachen. Die Beute ist wertvoll, aber sperrig.',
    difficulty: 5,
    unlockCost: 1600,
    intelCosts: [340, 520, 700],
    policeBase: 28,
    baseLight: 0.35,
    xpBase: 110,
    map: [
      '#########################',
      '#p...A.........A......q.#',
      '#.......................#',
      '#..GP......GP......GP...#',
      '#.......................#',
      '#..........F............#',
      '#..r.......P.......s....#',
      '#.......................#',
      '#..VP......VP......VP...#',
      '#.......................#',
      '#t...A.........A.......u#',
      '###########2#############',
      '          ,E,            ',
    ],
    legend: {
      G: { obj: 'loot', name: 'Antike Vase', icon: '🏺', value: 1500, weight: 8, bulky: true, hot: true },
      V: { obj: 'vitrine', name: 'Exponat-Vitrine', level: 2, loot: [
        { name: 'Goldmaske', icon: '🎭', value: 2400, weight: 3, hot: true },
      ] },
      P: { obj: 'plate' },
      A: { obj: 'camera', dir: 'S', range: 7, fov: 0.9, sweep: 0.8 },
      F: { obj: 'fusebox' },
      p: { obj: 'waypoint' },
      q: { obj: 'waypoint' },
      r: { obj: 'waypoint' },
      s: { obj: 'waypoint' },
      t: { obj: 'waypoint' },
      u: { obj: 'waypoint' },
    },
    guards: [
      {
        name: 'Museumswächter Alt',
        speed: 1.3,
        visionRange: 7,
        route: [
          { at: 'p', wait: 2 },
          { at: 'q', wait: 2 },
          { at: 'u', wait: 2 },
          { at: 't', wait: 2 },
        ],
      },
      {
        name: 'Museumswächter Jung',
        speed: 1.35,
        visionRange: 7,
        route: [
          { at: 'r', wait: 3 },
          { at: 's', wait: 3 },
        ],
      },
    ],
  },

  // 8 — STADTBANK (Finale): Tresorraum Stufe 3, Zeitdruck ----------------------
  {
    id: 'bank',
    name: 'Stadtbank – Der große Coup',
    desc: 'Das Finale. Panzerglas, Funkalarm, zwei Wachen, Kameras und ein Tresor der Stufe 3. Nur für Meisterdiebe.',
    difficulty: 5,
    unlockCost: 2500,
    intelCosts: [500, 750, 1000],
    policeBase: 24,
    baseLight: 0.4,
    xpBase: 160,
    map: [
      '##########################',
      '#p......A.......q#T.....T#',
      '#................#.......#',
      '#................3...B...#',
      '#................#T.....T#',
      '#................#.......#',
      '#................#########',
      '#F.r.........A...........#',
      '#.........s.....P........#',
      '#........................#',
      '#............A...........#',
      '#........................#',
      '########2#################',
      '       ,E,                ',
    ],
    legend: {
      B: { obj: 'safe', name: 'Haupttresor', level: 3, funkalarm: true, loot: [
        { name: 'Geldbündel-Palette', icon: '💰', value: 4500, weight: 5 },
        { name: 'Goldbarren', icon: '🥇', value: 3800, weight: 12, bulky: true },
      ] },
      T: { obj: 'loot', name: 'Schließfach-Inhalt', icon: '💎', value: 1600, weight: 1, hot: true },
      A: { obj: 'camera', dir: 'S', range: 7, fov: 0.9, sweep: 0.7 },
      P: { obj: 'plate' },
      F: { obj: 'fusebox' },
      p: { obj: 'waypoint' },
      q: { obj: 'waypoint' },
      r: { obj: 'waypoint' },
      s: { obj: 'waypoint' },
    },
    guards: [
      {
        name: 'Sicherheitsmann Kord',
        speed: 1.4,
        visionRange: 7,
        route: [
          { at: 'p', wait: 2 },
          { at: 'q', wait: 2 },
          { at: 's', wait: 2 },
          { at: 'r', wait: 2 },
        ],
      },
      {
        name: 'Sicherheitsmann Vogt',
        speed: 1.4,
        visionRange: 7,
        route: [
          { at: 'r', wait: 3 },
          { at: 's', wait: 3 },
        ],
      },
    ],
  },
];

export const HEIST_BY_ID: Record<string, HeistDef> = Object.fromEntries(
  HEISTS.map((h) => [h.id, h]),
);
