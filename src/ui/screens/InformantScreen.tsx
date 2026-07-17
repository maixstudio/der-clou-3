import { useGame } from '../../game/state';
import { HEISTS } from '../../game/data/heists';
import { NavPills, Topbar } from '../components';

const INTEL_LABEL = ['nichts', 'Grundriss-Details', 'Wachrouten', 'Sensorpläne'];

export function InformantScreen() {
  const { unlocked, intel, money, buyTip, buyIntel, selectHeist } = useGame();

  return (
    <div className="app">
      <Topbar />
      <NavPills current="informant" />
      <h1>🕶️ Informant Kalle</h1>
      <p className="dim">
        „Ich weiß, wo was zu holen ist – und was dich dort erwartet. Aber Infos kosten,
        Freundchen. Je mehr du weißt, desto besser deine Vorschau."
      </p>

      <div className="list">
        {HEISTS.map((h) => {
          const isUnlocked = unlocked.includes(h.id);
          const lvl = intel[h.id] ?? 0;
          const nextCost = lvl < 3 ? h.intelCosts[lvl] : null;
          return (
            <div className="card" key={h.id}>
              <div className="row between wrap">
                <div>
                  <b className={isUnlocked ? '' : 'dim'}>
                    {h.name} {'★'.repeat(h.difficulty)}
                  </b>
                  <br />
                  <small className="dim">
                    {isUnlocked ? h.desc : 'Kalle hat da einen Tipp … gegen Bezahlung.'}
                  </small>
                </div>
                <div style={{ textAlign: 'right', minWidth: 150 }}>
                  {!isUnlocked ? (
                    <button
                      className="small"
                      disabled={money < h.unlockCost}
                      onClick={() => buyTip(h.id)}
                    >
                      Tipp kaufen ({h.unlockCost} DM)
                    </button>
                  ) : (
                    <>
                      <div className="tag ok" style={{ marginBottom: 4 }}>
                        Wissen: {INTEL_LABEL[lvl]}
                      </div>
                      <br />
                      {nextCost !== null ? (
                        <button
                          className="small"
                          disabled={money < nextCost}
                          onClick={() => buyIntel(h.id)}
                        >
                          {INTEL_LABEL[lvl + 1]} ({nextCost} DM)
                        </button>
                      ) : (
                        <span className="tag lvl">voll ausgekundschaftet</span>
                      )}
                      <br />
                      <button
                        className="small primary"
                        style={{ marginTop: 4 }}
                        onClick={() => selectHeist(h.id)}
                      >
                        Planen →
                      </button>
                    </>
                  )}
                </div>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
