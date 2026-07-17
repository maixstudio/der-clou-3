import { useGame } from '../../game/state';
import { haggleFactor } from '../../game/character';
import { coolDaysLeft, fencePrice } from '../../game/economy';
import { NavPills, Topbar } from '../components';

export function FenceScreen() {
  const { stash, day, wanted, character, sellItem, sellAll } = useGame();
  const haggle = haggleFactor(character);
  const total = stash.reduce((s, l) => s + fencePrice(l, day, haggle, wanted), 0);

  return (
    <div className="app">
      <Topbar />
      <NavPills current="fence" />
      <h1>💰 Goldfinger-Gerd</h1>
      <p className="dim">
        „Ich zahl faire Preise – für meine Verhältnisse. Frische heiße Ware? Lass die erst mal
        abkühlen, dann gibt's mehr." (Handelsfaktor: {Math.round(haggle * 100)} %)
      </p>

      {stash.length === 0 ? (
        <div className="card">
          <p className="dim">Dein Lager ist leer. Erst klauen, dann verticken.</p>
        </div>
      ) : (
        <>
          <div className="card">
            <div className="list">
              {stash.map((l, i) => {
                const price = fencePrice(l, day, haggle, wanted);
                const cool = coolDaysLeft(l, day);
                return (
                  <div className="item" key={i}>
                    <span className="icon">{l.icon}</span>
                    <div className="grow">
                      <b>{l.name}</b>{' '}
                      {l.hot && <span className="tag hot">heiß</span>}
                      <br />
                      <small className="dim">
                        Basiswert {l.value} DM
                        {cool > 0 && ` · noch ${cool} Tag(e) bis abgekühlt`}
                      </small>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div className="gold">{price} DM</div>
                      <button className="small" onClick={() => sellItem(i)}>
                        Verkaufen
                      </button>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
          <div className="row between" style={{ marginTop: 10 }}>
            <b className="gold">Gesamtwert jetzt: {total.toLocaleString('de-DE')} DM</b>
            <button className="primary" onClick={sellAll}>
              Alles verkaufen
            </button>
          </div>
        </>
      )}
    </div>
  );
}
