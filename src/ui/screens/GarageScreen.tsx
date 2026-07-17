import { useGame } from '../../game/state';
import { VEHICLES } from '../../game/data/vehicles';
import { NavPills, Topbar } from '../components';

export function GarageScreen() {
  const { character: c, money, buyVehicle, setVehicle } = useGame();

  return (
    <div className="app">
      <Topbar />
      <NavPills current="garage" />
      <h1>🚗 Bennos Karren</h1>
      <p className="dim">
        „Der Fluchtwagen macht den Meister. Viel Platz für Beute oder viel Speed für die
        Flucht – such's dir aus."
      </p>

      <div className="grid cols-2">
        {VEHICLES.map((v) => {
          const owned = c.vehicles.includes(v.id);
          const active = c.activeVehicle === v.id;
          const afford = money >= v.price;
          return (
            <div className="item" key={v.id} style={{ borderColor: active ? 'var(--neon)' : undefined }}>
              <span className="icon" style={{ fontSize: 28 }}>
                {v.icon}
              </span>
              <div className="grow">
                <b>{v.name}</b> {active && <span className="tag ok">aktiv</span>}
                <br />
                <small>{v.desc}</small>
                <br />
                <small className="dim">
                  Beute +{v.capacity} kg · Flucht {Math.round(v.escape * 100)}% · Zuverl.{' '}
                  {Math.round(v.reliability * 100)}% {v.bulky && '· sperrige Beute ✓'}
                </small>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div className="gold">{v.price ? `${v.price} DM` : 'gratis'}</div>
                {owned ? (
                  <button className="small" disabled={active} onClick={() => setVehicle(v.id)}>
                    {active ? 'gewählt' : 'Wählen'}
                  </button>
                ) : (
                  <button className="small" disabled={!afford} onClick={() => buyVehicle(v.id)}>
                    Kaufen
                  </button>
                )}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
