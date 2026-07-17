import { useGame } from '../../game/state';
import { totalCarry } from '../../game/character';
import { ITEM_BY_ID } from '../../game/data/items';
import { VEHICLE_BY_ID } from '../../game/data/vehicles';
import { NavPills, Topbar } from '../components';

export function InventoryScreen() {
  const { character: c, toggleLoadout, go } = useGame();
  const veh = VEHICLE_BY_ID[c.activeVehicle];

  return (
    <div className="app">
      <Topbar />
      <NavPills current="inventory" />
      <h1>🎒 Ausrüstung & Loadout</h1>
      <p className="dim">
        Wähle, welche Werkzeuge du zum nächsten Bruch mitnimmst. Nur eingepackte Werkzeuge
        stehen beim Planen zur Verfügung.
      </p>

      <div className="row wrap" style={{ marginBottom: 10 }}>
        <span className="tag lvl">Tragkraft Beute: {totalCarry(c)} kg</span>
        <span className="tag">
          Fluchtwagen: {veh.icon} {veh.name}
        </span>
      </div>

      <div className="card">
        <h3>Werkzeugkoffer</h3>
        {c.owned.length === 0 && <p className="dim">Noch kein Werkzeug. Ab zum Shop!</p>}
        <div className="grid cols-2">
          {c.owned.map((id) => {
            const item = ITEM_BY_ID[id];
            const packed = c.loadout.includes(id);
            return (
              <div
                className="item"
                key={id}
                style={{ borderColor: packed ? 'var(--green)' : undefined }}
              >
                <span className="icon">{item.icon}</span>
                <div className="grow">
                  <b>{item.name}</b>
                  <br />
                  <small>{item.desc}</small>
                </div>
                <button
                  className={packed ? 'small cyan' : 'small ghost'}
                  onClick={() => toggleLoadout(id)}
                >
                  {packed ? '✓ eingepackt' : 'einpacken'}
                </button>
              </div>
            );
          })}
        </div>
      </div>

      <div className="row" style={{ marginTop: 12 }}>
        <button onClick={() => go('shop')}>🛠️ Mehr Werkzeug kaufen</button>
        <button className="primary" onClick={() => go('heists')}>
          🎯 Objekt auswählen
        </button>
      </div>
    </div>
  );
}
