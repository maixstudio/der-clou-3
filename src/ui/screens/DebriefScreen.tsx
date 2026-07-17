import { useGame } from '../../game/state';
import { Topbar } from '../components';

const OUTCOME_LABEL: Record<string, string> = {
  success: '✓ SAUBERER COUP',
  escaped: '≈ KNAPP ENTKOMMEN',
  caught: '✗ GESCHNAPPT',
  aborted: '⊘ ABGEBROCHEN',
};

export function DebriefScreen() {
  const { lastReport, go } = useGame();

  if (!lastReport) {
    return (
      <div className="app">
        <Topbar />
        <p>Kein Ergebnis vorhanden.</p>
        <button onClick={() => go('hub')}>Zum Unterschlupf</button>
      </div>
    );
  }
  const r = lastReport;

  return (
    <div className="app">
      <Topbar />
      <h1>Nachbesprechung</h1>

      <div className="row" style={{ marginBottom: 12 }}>
        <span className={`badge-outcome ${r.outcome}`}>{OUTCOME_LABEL[r.outcome]}</span>
      </div>

      <div className="grid cols-2">
        <div className="card">
          <h3>Bilanz</h3>
          <div className="list">
            <div className="row between">
              <span>Beutewert</span>
              <b className="gold">{r.lootValue.toLocaleString('de-DE')} DM</b>
            </div>
            <div className="row between">
              <span>Erfahrung</span>
              <b className="green">+{r.xp} XP</b>
            </div>
            <div className="row between">
              <span>Fahndung</span>
              <b className={r.wantedDelta > 0 ? 'red' : 'green'}>
                {r.wantedDelta > 0 ? '+' : ''}
                {r.wantedDelta}
              </b>
            </div>
            {r.fineDM > 0 && (
              <div className="row between">
                <span>Geldstrafe</span>
                <b className="red">−{r.fineDM.toLocaleString('de-DE')} DM</b>
              </div>
            )}
            {r.jailDays > 0 && (
              <div className="row between">
                <span>Haft</span>
                <b className="red">{r.jailDays} Tage</b>
              </div>
            )}
          </div>
        </div>

        <div className="card">
          <h3>Erbeutet</h3>
          {r.loot.length === 0 ? (
            <p className="dim">Nichts mitgenommen{r.outcome === 'caught' ? ' – alles beschlagnahmt.' : '.'}</p>
          ) : (
            <div className="list">
              {r.loot.map((l, i) => (
                <div className="item" key={i}>
                  <span className="icon">{l.icon}</span>
                  <div className="grow">
                    <b>{l.name}</b> {l.hot && <span className="tag hot">heiß</span>}
                  </div>
                  <span className="gold">{l.value} DM</span>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      <div className="newspaper" style={{ marginTop: 12 }}>
        <div style={{ fontSize: 11, letterSpacing: 2 }}>DER STADT-ANZEIGER · NACHTAUSGABE</div>
        <h2>{r.headline}</h2>
        <p style={{ margin: 0, fontSize: 13 }}>
          {r.success
            ? 'Die Polizei tappt im Dunkeln. Von den Tätern fehlt jede Spur.'
            : r.outcome === 'caught'
              ? 'Dank aufmerksamer Sicherheitskräfte konnte der Einbrecher gefasst werden.'
              : 'Zeugen berichten von einer verdächtigen Gestalt in der Nacht.'}
        </p>
      </div>

      <div className="row" style={{ marginTop: 14 }}>
        {r.loot.length > 0 && (
          <button className="primary" onClick={() => go('fence')}>
            💰 Beute zum Hehler
          </button>
        )}
        <button className="cyan" onClick={() => go('hub')}>
          🏠 Zum Unterschlupf
        </button>
        <button onClick={() => go('character')}>🧬 Charakter</button>
      </div>
    </div>
  );
}
