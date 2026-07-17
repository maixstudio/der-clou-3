// Fluchtfahrzeuge für „Bennos Karren". Das Fahrzeug bestimmt Beutelimit,
// Fluchtchance nach Alarm und wie auffällig man unterwegs ist.

export interface VehicleDef {
  id: string;
  name: string;
  icon: string;
  price: number;
  /** zusätzliche Tragkraft für Beute (kg) */
  capacity: number;
  /** Fluchtbonus 0..1 (addiert sich zum Fahren-Skill nach Alarm) */
  escape: number;
  /** Zuverlässigkeit: Chance, dass der Wagen anspringt (0..1) */
  reliability: number;
  /** Auffälligkeit erhöht Fahndung nach lauten Coups (0..1) */
  conspicuous: number;
  /** erlaubt sperrige Beute */
  bulky: boolean;
  desc: string;
}

export const VEHICLES: VehicleDef[] = [
  {
    id: 'none',
    name: 'Zu Fuß / Fahrrad',
    icon: '🚲',
    price: 0,
    capacity: 0,
    escape: 0,
    reliability: 1,
    conspicuous: 0.1,
    bulky: false,
    desc: 'Kein Fluchtwagen. Nur leichte Beute, schlechte Fluchtchance.',
  },
  {
    id: 'kadett',
    name: 'Klappriger Kadett',
    icon: '🚗',
    price: 1200,
    capacity: 25,
    escape: 0.15,
    reliability: 0.8,
    conspicuous: 0.2,
    bulky: false,
    desc: 'Rostlaube, springt nicht immer an – aber besser als nichts.',
  },
  {
    id: 'kombi',
    name: 'Geräumiger Kombi',
    icon: '🚙',
    price: 3200,
    capacity: 60,
    escape: 0.25,
    reliability: 0.9,
    conspicuous: 0.25,
    bulky: true,
    desc: 'Viel Platz für sperrige Beute, unauffällig genug.',
  },
  {
    id: 'transporter',
    name: 'Lieferwagen „Blitz"',
    icon: '🚐',
    price: 5000,
    capacity: 120,
    escape: 0.2,
    reliability: 0.92,
    conspicuous: 0.4,
    bulky: true,
    desc: 'Riesiger Laderaum, aber langsam und auffällig.',
  },
  {
    id: 'gti',
    name: 'Getunter GTI',
    icon: '🏎️',
    price: 7500,
    capacity: 30,
    escape: 0.55,
    reliability: 0.95,
    conspicuous: 0.5,
    bulky: false,
    desc: 'Rennmaschine – top Fluchtchance, aber fällt auf.',
  },
  {
    id: 'benz',
    name: 'Unscheinbarer alter Benz',
    icon: '🚘',
    price: 9800,
    capacity: 70,
    escape: 0.45,
    reliability: 0.98,
    conspicuous: 0.05,
    bulky: true,
    desc: 'Solide, geräumig, absolut unauffällig – die Allzweckwaffe.',
  },
];

export const VEHICLE_BY_ID: Record<string, VehicleDef> = Object.fromEntries(
  VEHICLES.map((v) => [v.id, v]),
);
