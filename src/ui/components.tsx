import type { ReactNode } from 'react';
import { useGame, type Screen } from '../game/state';

export function Bar({ value, max, kind }: { value: number; max: number; kind?: 'xp' | 'wanted' }) {
  const pct = Math.max(0, Math.min(100, (value / max) * 100));
  return (
    <div className={`bar ${kind ?? ''}`}>
      <span style={{ width: `${pct}%` }} />
    </div>
  );
}

/** Kopfzeile mit den wichtigsten Kennzahlen; überall außer im Menü sichtbar. */
export function Topbar() {
  const { character, money, day, wanted, stash, go } = useGame();
  const stashValue = stash.reduce((s, l) => s + l.value, 0);
  return (
    <div className="topbar">
      <div className="stat">
        <span className="dim">🕵️</span>
        <b>{character.name}</b>
        <span className="tag lvl">Lvl {character.level}</span>
      </div>
      <div className="stat">
        <span className="dim">Geld</span>
        <b>{money.toLocaleString('de-DE')} DM</b>
      </div>
      <div className="stat">
        <span className="dim">Tag</span>
        <b>{day}</b>
      </div>
      <div className="stat" title="Fahndungslevel der Polizei">
        <span className="dim">Fahndung</span>
        <div style={{ width: 90 }}>
          <Bar value={wanted} max={100} kind="wanted" />
        </div>
        <b>{Math.round(wanted)}</b>
      </div>
      {stash.length > 0 && (
        <div className="stat">
          <span className="dim">Lager</span>
          <b className="gold">{stash.length} Stk · {stashValue.toLocaleString('de-DE')} DM</b>
        </div>
      )}
      <div className="spacer" />
      <button className="small" onClick={() => go('hub')}>
        🏠 Unterschlupf
      </button>
    </div>
  );
}

export function NavPills({ current }: { current: Screen }) {
  const go = useGame((s) => s.go);
  const items: [Screen, string][] = [
    ['hub', '🏠 Hub'],
    ['character', '🧬 Charakter'],
    ['inventory', '🎒 Ausrüstung'],
    ['shop', '🛠️ Shop'],
    ['garage', '🚗 Garage'],
    ['training', '🏋️ Training'],
    ['fence', '💰 Hehler'],
    ['informant', '🕶️ Informant'],
    ['heists', '🎯 Objekte'],
  ];
  return (
    <div className="pill-nav">
      {items.map(([s, label]) => (
        <button
          key={s}
          className={current === s ? 'primary small' : 'small'}
          onClick={() => go(s)}
        >
          {label}
        </button>
      ))}
    </div>
  );
}

export function Screen2({ children }: { children: ReactNode }) {
  return <div className="app">{children}</div>;
}
