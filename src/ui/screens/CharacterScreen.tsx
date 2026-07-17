import { useGame } from '../../game/state';
import { XP_PER_LEVEL } from '../../game/character';
import { PERKS, PERK_BY_ID } from '../../game/data/perksData';
import type { SkillId, StatId } from '../../engine/types';
import { Bar, NavPills, Topbar } from '../components';

const STAT_LABEL: Record<StatId, string> = {
  staerke: 'Stärke',
  geschick: 'Geschick',
  kondition: 'Kondition',
  intelligenz: 'Intelligenz',
  nerven: 'Nervenstärke',
};
const SKILL_LABEL: Record<SkillId, string> = {
  schloesser: 'Schlösserknacken',
  safes: 'Safeknacken',
  elektronik: 'Elektronik',
  schleichen: 'Schleichen',
  sport: 'Sportlichkeit',
  fahren: 'Fahren',
  handeln: 'Handeln',
};

export function CharacterScreen() {
  const { character: c, raiseSkill, choosePerk } = useGame();

  return (
    <div className="app">
      <Topbar />
      <NavPills current="character" />
      <h1>Charakterbogen</h1>

      <div className="row between wrap" style={{ marginBottom: 10 }}>
        <div className="row wrap">
          <span className="tag lvl">Stufe {c.level}</span>
          <span className="tag">
            XP {c.xp} / {XP_PER_LEVEL(c.level)}
          </span>
          <div style={{ width: 160 }}>
            <Bar value={c.xp} max={XP_PER_LEVEL(c.level)} kind="xp" />
          </div>
        </div>
        <div className="row wrap">
          <span className="tag ok">Skillpunkte: {c.skillPoints}</span>
          <span className="tag ok">Fähigkeits-Slots: {c.perkSlots}</span>
        </div>
      </div>

      <div className="grid cols-2">
        <div className="card">
          <h3>Attribute</h3>
          {(Object.keys(STAT_LABEL) as StatId[]).map((id) => (
            <div className="stat-line" key={id}>
              <span>{STAT_LABEL[id]}</span>
              <Bar value={c.stats[id]} max={10} />
              <b className="gold">{c.stats[id]}</b>
            </div>
          ))}
          <p className="dim" style={{ fontSize: 12 }}>
            Attribute steigen durch Training (unter „Training").
          </p>
        </div>

        <div className="card">
          <h3>Fertigkeiten</h3>
          {(Object.keys(SKILL_LABEL) as SkillId[]).map((id) => (
            <div className="stat-line" key={id}>
              <span>{SKILL_LABEL[id]}</span>
              <Bar value={c.skills[id]} max={100} />
              <div className="row" style={{ gap: 4 }}>
                <b>{c.skills[id]}</b>
                <button
                  className="small"
                  disabled={c.skillPoints <= 0 || c.skills[id] >= 100}
                  onClick={() => raiseSkill(id)}
                  title="+5 (1 Skillpunkt)"
                >
                  +
                </button>
              </div>
            </div>
          ))}
        </div>
      </div>

      <div className="card" style={{ marginTop: 12 }}>
        <h3>Fähigkeiten (Perks) — {c.perkSlots} frei</h3>
        <div className="grid cols-3">
          {PERKS.map((p) => {
            const owned = c.perks.includes(p.id);
            const locked = c.level < p.minLevel;
            return (
              <div className="item" key={p.id} style={{ opacity: owned ? 1 : locked ? 0.5 : 1 }}>
                <span className="icon">{p.icon}</span>
                <div className="grow">
                  <b className={owned ? 'green' : ''}>{p.name}</b>
                  <br />
                  <small>{p.desc}</small>
                  {locked && <div className="tag">ab Stufe {p.minLevel}</div>}
                </div>
                {owned ? (
                  <span className="tag ok">gelernt</span>
                ) : (
                  <button
                    className="small"
                    disabled={locked || c.perkSlots <= 0}
                    onClick={() => choosePerk(p.id)}
                  >
                    Lernen
                  </button>
                )}
              </div>
            );
          })}
        </div>
      </div>

      {c.perks.length > 0 && (
        <p className="dim" style={{ marginTop: 8, fontSize: 12 }}>
          Aktive Perks: {c.perks.map((id) => PERK_BY_ID[id].name).join(', ')}
        </p>
      )}
    </div>
  );
}
