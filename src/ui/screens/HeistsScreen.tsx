import { useGame } from '../../game/state';
import { HEISTS } from '../../game/data/heists';
import { NavPills, Topbar } from '../components';

export function HeistsScreen() {
  const { unlocked, intel, completed, selectHeist, go } = useGame();
  const known = HEISTS.filter((h) => unlocked.includes(h.id));
  const locked = HEISTS.filter((h) => !unlocked.includes(h.id));

  return (
    <div className="app">
      <Topbar />
      <NavPills current="heists" />
      <h1>🎯 Objekte</h1>
      <p className="dim">Wähle ein Ziel, um deinen Einbruch zu planen.</p>

      <div className="grid cols-2">
        {known.map((h) => {
          const lvl = intel[h.id] ?? 0;
          const done = completed.includes(h.id);
          return (
            <div className="card" key={h.id} style={{ borderColor: done ? 'var(--green)' : undefined }}>
              <div className="row between">
                <h3 style={{ margin: 0 }}>
                  {h.name} {done && <span className="tag ok">erledigt</span>}
                </h3>
                <span className="gold">{'★'.repeat(h.difficulty)}</span>
              </div>
              <p className="dim" style={{ fontSize: 12 }}>
                {h.desc}
              </p>
              <div className="row between">
                <span className="tag">Auskundschaftung: Stufe {lvl}/3</span>
                <button className="primary small" onClick={() => selectHeist(h.id)}>
                  Planen →
                </button>
              </div>
            </div>
          );
        })}
      </div>

      {locked.length > 0 && (
        <div className="card" style={{ marginTop: 12 }}>
          <h3>Noch nicht spruchreif</h3>
          <p className="dim" style={{ fontSize: 12 }}>
            {locked.length} weitere Objekte warten auf einen Tipp vom Informanten.
          </p>
          <button onClick={() => go('informant')}>🕶️ Zu Kalle</button>
        </div>
      )}
    </div>
  );
}
