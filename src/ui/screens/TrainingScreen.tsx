import { useGame } from '../../game/state';
import { TRAININGS, trainingResult } from '../../game/data/training';
import { NavPills, Topbar } from '../components';

export function TrainingScreen() {
  const { character: c, money, train } = useGame();

  const currentOf = (t: (typeof TRAININGS)[number]): number =>
    t.target.kind === 'skill' ? c.skills[t.target.id] : c.stats[t.target.id];

  return (
    <div className="app">
      <Topbar />
      <NavPills current="training" />
      <h1>🏋️ Training</h1>
      <p className="dim">
        Übung macht den Meister – kostet aber Geld und Zeit. Je höher ein Wert, desto weniger
        bringt weiteres Training.
      </p>

      <div className="grid cols-2">
        {TRAININGS.map((t) => {
          const cur = currentOf(t);
          const gain = trainingResult(t, cur);
          const afford = money >= t.cost && gain > 0;
          return (
            <div className="item" key={t.id}>
              <span className="icon">{t.icon}</span>
              <div className="grow">
                <b>{t.name}</b>
                <br />
                <small>{t.desc}</small>
                <br />
                <small className="dim">
                  aktuell {cur} → +{gain} · {t.days} Tag(e)
                </small>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div className="gold">{t.cost} DM</div>
                <button className="small" disabled={!afford} onClick={() => train(t.id)}>
                  Trainieren
                </button>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
