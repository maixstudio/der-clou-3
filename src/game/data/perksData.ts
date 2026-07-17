// Perk-Katalog. Jeder Perk ist ein passiver Effekt, der an einer zentralen
// Stelle (buildProfile in character.ts) in die Engine-Parameter einfließt.

export type PerkId =
  | 'katzenpfoten'
  | 'werkzeugnarr'
  | 'kaltbluetig'
  | 'bleifuss'
  | 'feilscher'
  | 'sechster_sinn'
  | 'gluckspilz'
  | 'muskelprotz'
  | 'elektronikfreak'
  | 'unauffaellig'
  | 'safeohr'
  | 'nachtsicht'
  | 'adrenalin'
  | 'langfinger'
  | 'spurenlos'
  | 'ausdauernd';

export interface PerkDef {
  id: PerkId;
  name: string;
  icon: string;
  desc: string;
  /** Mindest-Level, ab dem der Perk wählbar ist */
  minLevel: number;
}

export const PERKS: PerkDef[] = [
  { id: 'katzenpfoten', name: 'Katzenpfoten', icon: '🐾', desc: '−35 % Gehlärm beim Schleichen.', minLevel: 1 },
  { id: 'werkzeugnarr', name: 'Werkzeugnarr', icon: '🧰', desc: 'Feinarbeit (Schlösser, Safes) geht 15 % schneller.', minLevel: 1 },
  { id: 'kaltbluetig', name: 'Kaltblütig', icon: '🧊', desc: 'Kein Nerven-Malus nach Alarm – volle Kontrolle unter Druck.', minLevel: 3 },
  { id: 'bleifuss', name: 'Bleifuß', icon: '🏁', desc: '+25 % Fluchtgeschwindigkeit nach Alarm.', minLevel: 2 },
  { id: 'feilscher', name: 'Feilscher', icon: '🤝', desc: '+15 % beim Hehler für jede Beute.', minLevel: 1 },
  { id: 'sechster_sinn', name: 'Sechster Sinn', icon: '👁️', desc: 'Wachen entdecken dich langsamer (−20 % Entdeckung).', minLevel: 2 },
  { id: 'gluckspilz', name: 'Glückspilz', icon: '🍀', desc: 'Ein misslungener Versuch pro Coup wird wiederholt.', minLevel: 3 },
  { id: 'muskelprotz', name: 'Muskelprotz', icon: '💪', desc: '+12 kg Tragkraft, Türen aufbrechen geht schneller.', minLevel: 1 },
  { id: 'elektronikfreak', name: 'Elektronikfreak', icon: '💡', desc: 'Elektronik-Aktionen 25 % schneller und sicherer.', minLevel: 2 },
  { id: 'unauffaellig', name: 'Unauffällig', icon: '🕶️', desc: 'Fahndung sinkt beim Schlafen doppelt so schnell.', minLevel: 2 },
  { id: 'safeohr', name: 'Safeknacker-Gehör', icon: '👂', desc: 'Safeknacken mit Stethoskop 30 % schneller.', minLevel: 4 },
  { id: 'nachtsicht', name: 'Nachtsicht', icon: '🌙', desc: 'Im Dunkeln schwerer zu entdecken.', minLevel: 3 },
  { id: 'adrenalin', name: 'Adrenalinschub', icon: '⚡', desc: 'Nach Alarm zusätzlich +20 % Tempo bei allem.', minLevel: 4 },
  { id: 'langfinger', name: 'Langfinger', icon: '🤏', desc: 'Beute einpacken geht spürbar schneller.', minLevel: 1 },
  { id: 'spurenlos', name: 'Spurenlos', icon: '🧽', desc: 'Hinterlässt nie verwertbare Spuren – Fahndung steigt langsamer.', minLevel: 2 },
  { id: 'ausdauernd', name: 'Ausdauernd', icon: '🫁', desc: 'Bewegt sich generell 12 % schneller.', minLevel: 3 },
];

export const PERK_BY_ID: Record<PerkId, PerkDef> = Object.fromEntries(
  PERKS.map((p) => [p.id, p]),
) as Record<PerkId, PerkDef>;
