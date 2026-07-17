import { useGame } from '../../game/state';
import { CATEGORY_LABEL, ITEMS, type ItemCategory } from '../../game/data/items';
import { NavPills, Topbar } from '../components';

export function ShopScreen() {
  const { character: c, money, buyItem } = useGame();
  const cats = [...new Set(ITEMS.map((i) => i.category))] as ItemCategory[];

  return (
    <div className="app">
      <Topbar />
      <NavPills current="shop" />
      <h1>🛠️ Werkzeug-Willi</h1>
      <p className="dim">„Für jeden Job das richtige Eisen. Alles ehrlich geklaut, versteht sich."</p>

      {cats.map((cat) => (
        <div className="card" key={cat} style={{ marginBottom: 10 }}>
          <h3>{CATEGORY_LABEL[cat]}</h3>
          <div className="grid cols-2">
            {ITEMS.filter((i) => i.category === cat).map((item) => {
              const owned = c.owned.includes(item.id);
              const afford = money >= item.price;
              return (
                <div className="item" key={item.id}>
                  <span className="icon">{item.icon}</span>
                  <div className="grow">
                    <b>{item.name}</b>
                    <br />
                    <small>{item.desc}</small>
                    <br />
                    <small className="dim">Gewicht {item.weight} kg</small>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div className="gold">{item.price} DM</div>
                    {owned ? (
                      <span className="tag ok">im Besitz</span>
                    ) : (
                      <button className="small" disabled={!afford} onClick={() => buyItem(item.id)}>
                        Kaufen
                      </button>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      ))}
    </div>
  );
}
