import { useGame } from '../../game/state';
import { NavPills, Topbar } from '../components';

export function HubScreen() {
  const { go, sleep, character, log, resetGame, completed } = useGame();

  return (
    <div className="app">
      <Topbar />
      <NavPills current="hub" />
      <h1>Unterschlupf</h1>
      <p className="dim">Von hier aus planst du deine großen Coups. Wähle dein nächstes Ziel.</p>

      <div className="grid cols-3">
        <div className="card">
          <h3>🎯 Ans Werk</h3>
          <p className="dim">Objekte auskundschaften, Plan schmieden, Bruch ausführen.</p>
          <div className="list">
            <button className="primary" onClick={() => go('heists')}>
              Objekte ansehen ({completed.length} erledigt)
            </button>
            <button onClick={() => go('informant')}>🕶️ Zum Informanten Kalle</button>
          </div>
        </div>

        <div className="card">
          <h3>🧬 Ausbau</h3>
          <p className="dim">Werde besser: Skills, Werkzeug, fahrbarer Untersatz.</p>
          <div className="list">
            <button onClick={() => go('character')}>Charakterbogen</button>
            <button onClick={() => go('shop')}>🛠️ Werkzeug-Willi</button>
            <button onClick={() => go('garage')}>🚗 Bennos Karren</button>
            <button onClick={() => go('training')}>🏋️ Training</button>
          </div>
        </div>

        <div className="card">
          <h3>💰 Geschäfte</h3>
          <p className="dim">Beute zu Geld machen und ausruhen.</p>
          <div className="list">
            <button onClick={() => go('fence')}>Zum Hehler Goldfinger-Gerd</button>
            <button className="cyan" onClick={() => sleep(1)} title="1 Nacht schlafen">
              🛏️ Schlafen (1 Tag, Fahndung sinkt)
            </button>
            <div className="row">
              <span className="dim">Ausdauer</span>
              <div style={{ flex: 1 }}>
                <div className="bar">
                  <span style={{ width: `${character.stamina}%` }} />
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <div className="card" style={{ marginTop: 12 }}>
        <h3>📰 Was so passiert ist</h3>
        <div className="log">
          {log.length === 0 && <div className="dim">Noch nichts geschehen. Auf geht's!</div>}
          {log.map((e, i) => (
            <div className="ev" key={i}>
              <span className="dim">Tag {e.day}:</span> {e.text}
            </div>
          ))}
        </div>
      </div>

      <div style={{ marginTop: 12 }}>
        <button
          className="ghost small danger"
          onClick={() => {
            if (confirm('Wirklich alles löschen und neu anfangen?')) resetGame();
          }}
        >
          Spielstand löschen
        </button>
      </div>
    </div>
  );
}
