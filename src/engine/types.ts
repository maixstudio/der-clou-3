// Kerntypen der Simulations-Engine. Die Engine ist React-frei und deterministisch:
// alle Charakter-/Ausrüstungs-Einflüsse kommen als ActorProfile herein (crew-fähig,
// V1 hat genau einen Akteur).

export interface Vec {
  x: number;
  y: number;
}

export type SkillId =
  | 'schloesser'
  | 'safes'
  | 'elektronik'
  | 'schleichen'
  | 'sport'
  | 'fahren'
  | 'handeln';

export type StatId = 'staerke' | 'geschick' | 'kondition' | 'intelligenz' | 'nerven';

export type ToolId =
  | 'dietrich1'
  | 'dietrich2'
  | 'dietrich3'
  | 'brecheisen'
  | 'glasschneider'
  | 'bohrmaschine'
  | 'winkelschleifer'
  | 'stethoskop'
  | 'drahtschere'
  | 'loetkolben'
  | 'stoersender'
  | 'rekorder'
  | 'seil'
  | 'handschuhe'
  | 'turnschuhe'
  | 'sporttasche'
  | 'rucksack';

/** Alles, was die Engine über den Akteur wissen muss. Wird vom Meta-Spiel gebaut. */
export interface ActorProfile {
  skills: Record<SkillId, number>; // 0..100
  stats: Record<StatId, number>; // 1..10
  tools: ToolId[];
  /** Tiles pro Sekunde */
  walkSpeed: number;
  sneakSpeed: number;
  /** Lärmradius in Tiles pro Bewegungsschritt */
  walkNoise: number;
  sneakNoise: number;
  /** kg Beute, die mitgenommen werden können */
  carryCapacity: number;
  /** Multiplikator auf Entdeckungs-Zuwachs (Perks: sechster Sinn) */
  detectionFactor: number;
  /** Untergrenze des Licht-Faktors bei Dunkelheit (Perk Nachtsicht senkt sie) */
  lightFloor: number;
  /** Tempo-Bonus nach Alarm (Perk Adrenalin) */
  alarmSpeedBonus: number;
  /** Nerven-Malus nach Alarm unterdrücken (Perk Kaltblütig) */
  kaltbluetig: boolean;
  /** Ein Fehlschlag pro Job wird wiederholt (Perk Glückspilz) */
  reroll: boolean;
  /** Elektronik-Aktionen schneller/sicherer (Perk Elektronikfreak) */
  elektronikFix: boolean;
  /** Safeknacken mit Stethoskop schneller (Perk Safeknacker-Gehör) */
  safeOhr: boolean;
  /** Fluchtbonus (Fahrzeug + Fahren + Perk Bleifuß), 0..1 additiv */
  escapeBonus: number;
  /** Sperrige Beute nur mit Fahrzeug */
  hasVehicle: boolean;
}

// ---------------------------------------------------------------------------
// Grundriss
// ---------------------------------------------------------------------------

export type TileKind =
  | 'void' // außerhalb, nicht begehbar
  | 'outdoor' // begehbar, draußen
  | 'floor'
  | 'wall'
  | 'door'
  | 'window'
  | 'furniture'
  | 'exit' // Start-/Fluchtpunkt (draußen)
  | 'climb'; // Kletterstelle (mit Seil passierbar)

export interface Tile {
  kind: TileKind;
  /** nur bei Türen: 0 = unverschlossen, 1..3 Schlossstufe */
  lockLevel: 0 | 1 | 2 | 3;
}

export interface LootItem {
  id: string;
  name: string;
  icon: string;
  value: number; // DM Basiswert
  weight: number; // kg
  bulky?: boolean; // nur mit Fahrzeug abtransportierbar
  hot?: boolean; // heiße Ware (Hehler-Malus bei Schnellverkauf)
  pos?: Vec; // gesetzt bei freiliegender Beute
  containerId?: string; // gesetzt bei Beute in Safe/Vitrine
}

export interface Container {
  id: string;
  kind: 'safe' | 'vitrine';
  name: string;
  pos: Vec;
  level: 1 | 2 | 3;
  /** löst beim Öffnen ohne Störsender einen stillen Funkalarm aus */
  funkalarm?: boolean;
}

export type SensorKind = 'motion' | 'camera' | 'plate';

export interface Sensor {
  id: string;
  kind: SensorKind;
  pos: Vec;
  /** Blickrichtung in Radiant (motion/camera) */
  dir: number;
  /** Öffnungswinkel in Radiant */
  fov: number;
  range: number;
  /** Kameras schwenken um dir mit dieser Amplitude (Radiant) */
  sweep: number;
}

export interface GuardRoutePoint {
  pos: Vec;
  wait: number; // Sekunden Wartezeit am Punkt
}

export interface Guard {
  id: string;
  name: string;
  route: GuardRoutePoint[];
  speed: number; // tiles/s Patrouille
  visionRange: number;
}

export interface Lamp {
  pos: Vec;
  radius: number;
  level: number; // Helligkeit 0..1 im Zentrum
}

/** Vollständig geparster, simulationsbereiter Grundriss. */
export interface HeistMap {
  w: number;
  h: number;
  tiles: Tile[]; // index = y * w + x
  exit: Vec;
  loot: LootItem[];
  containers: Container[];
  sensors: Sensor[];
  guards: Guard[];
  lamps: Lamp[];
  fusebox?: Vec;
  phone?: Vec;
  baseLight: number; // Umgebungslicht 0..1 (nachts niedrig)
  policeBase: number; // Sekunden bis Polizei nach Alarm
}

// ---------------------------------------------------------------------------
// Plan & Aktionen
// ---------------------------------------------------------------------------

export type ActionId =
  | 'moveTo'
  | 'wait'
  | 'openDoor'
  | 'pickLock'
  | 'forceDoor'
  | 'cutWindow'
  | 'smashWindow'
  | 'climb'
  | 'crackSafe'
  | 'drillSafe'
  | 'grindSafe'
  | 'cutVitrine'
  | 'smashVitrine'
  | 'grabLoot'
  | 'disableSensor'
  | 'disableCamera'
  | 'cutPower'
  | 'cutPhone'
  | 'placeDistraction';

export interface PlanStep {
  id: string;
  action: ActionId;
  /** Ziel-Tile (bei moveTo das Ziel, sonst das Objekt-Tile) */
  target?: Vec;
  /** Objekt-Referenz (Container-, Sensor-, Beute-Id) */
  targetId?: string;
  tool?: ToolId;
  mode?: 'walk' | 'sneak';
  waitTime?: number; // nur bei wait
}

export interface Plan {
  heistId: string;
  steps: PlanStep[];
}

/** Ein kompilierter Schritt: Pfad + Zeiten + Risiko, Grundlage für Timeline & Sim. */
export interface CompiledStep {
  step: PlanStep;
  path: Vec[]; // inkl. Start- und Zieltile; leer bei Nicht-Bewegung
  moveDuration: number;
  actionDuration: number;
  noise: number; // Lärmradius der Aktion
  failChance: number; // 0..1
  startT: number;
  endT: number;
  warnings: string[];
}

export interface CompiledPlan {
  heistId: string;
  steps: CompiledStep[];
  totalTime: number;
  warnings: string[]; // planweite Warnungen
  valid: boolean;
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

export type GuardState = 'patrol' | 'investigate' | 'alert' | 'chase' | 'phone';

export interface GuardSnap {
  id: string;
  pos: Vec;
  facing: number;
  state: GuardState;
  suspicion: number; // 0..1
}

export interface Snapshot {
  t: number;
  player: Vec;
  playerAction?: string; // Label der laufenden Aktion
  stepIndex: number;
  guards: GuardSnap[];
  doorsOpen: number[]; // Tile-Indizes geöffneter Türen/Fenster
  alarm: boolean;
  silentAlarm: boolean;
  policeEta: number | null;
  powerCut: boolean;
  carriedWeight: number;
  fleeing: boolean;
}

export type SimEventKind =
  | 'info'
  | 'action'
  | 'fail'
  | 'noise'
  | 'suspicion'
  | 'spotted'
  | 'alarm'
  | 'police'
  | 'loot'
  | 'flee'
  | 'outcome';

export interface SimEvent {
  t: number;
  kind: SimEventKind;
  msg: string;
  pos?: Vec;
}

export type SimOutcome =
  | 'success' // sauber entkommen, keine Verhaftung
  | 'escaped' // nach Alarm entkommen
  | 'caught' // verhaftet
  | 'aborted'; // Plan abgebrochen, aber entkommen

export interface SimResult {
  outcome: SimOutcome;
  events: SimEvent[];
  snapshots: Snapshot[];
  lootTaken: LootItem[];
  alarmTriggered: boolean;
  wasSpotted: boolean;
  /** Sekunden Gesamtdauer */
  duration: number;
  /** true, wenn der Spieler Spuren hinterließ (keine Handschuhe) */
  leftEvidence: boolean;
}

export interface SimOptions {
  mode: 'preview' | 'exec';
  seed: number;
  /** Auskundschaftungs-Stufe 0..3; in der Vorschau werden nur bekannte
   *  Wachen/Sensoren simuliert. Die Ausführung simuliert immer alles. */
  intel: number;
  /** Ab diesem Zeitpunkt bricht der Spieler ab und flieht (Notabbruch-Knopf). */
  abortAt?: number;
}

export const TICK = 0.5; // Sekunden pro Simulationstick

export const idx = (p: Vec, w: number): number => p.y * w + p.x;
export const key = (p: Vec): string => `${p.x},${p.y}`;
export const dist = (a: Vec, b: Vec): number => Math.hypot(a.x - b.x, a.y - b.y);
export const clamp = (v: number, lo: number, hi: number): number =>
  Math.max(lo, Math.min(hi, v));
