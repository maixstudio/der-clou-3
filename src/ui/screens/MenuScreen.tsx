import { useState } from 'react';
import { useGame } from '../../game/state';

export function MenuScreen() {
  const { started, newGame, go, character } = useGame();
  const [name, setName] = useState('');
  const [creating, setCreating] = useState(false);

  return (
    <div className="app menu">
      <div className="title-huge">DER CLOU 3</div>
      <div className="subtitle">Die Einbrecher-Simulation · Anno 1986</div>

      <div className="card" style={{ maxWidth: 460, margin: '0 auto' }}>
        {!creating ? (
          <div className="list">
            {started && (
              <button className="primary" onClick={() => go('hub')}>
                ▶ Weiterspielen ({character.name}, Lvl {character.level})
              </button>
            )}
            <button className="cyan" onClick={() => setCreating(true)}>
              ✦ Neue Verbrecherkarriere
            </button>
            <p className="dim" style={{ fontSize: 12, marginTop: 10 }}>
              Plane auf dem Grundriss deinen Einbruch – Schritt für Schritt, Werkzeug für
              Werkzeug. Teste ihn in der Vorschau, führe ihn nachts aus und hoffe, dass alles
              glattgeht. Werde vom kleinen Ganoven zum Meisterdieb.
            </p>
          </div>
        ) : (
          <div className="list">
            <h3>Wie heißt der neue Boss?</h3>
            <input
              type="text"
              value={name}
              autoFocus
              placeholder="Dein Deckname …"
              onChange={(e) => setName(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && newGame(name)}
            />
            <div className="row">
              <button className="primary" onClick={() => newGame(name)}>
                Loslegen
              </button>
              <button className="ghost" onClick={() => setCreating(false)}>
                Zurück
              </button>
            </div>
            {started && (
              <p className="red" style={{ fontSize: 12 }}>
                Achtung: Ein neues Spiel überschreibt deinen aktuellen Spielstand.
              </p>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
