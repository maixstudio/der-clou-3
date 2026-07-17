import { useMemo, useState } from 'react';
import { useGame } from '../../game/state';
import { buildProfile } from '../../game/character';
import { HEIST_BY_ID } from '../../game/data/heists';
import { parseHeist } from '../../engine/parse';
import { simulate } from '../../engine/simulate';
import type { Plan } from '../../engine/types';
import { FloorplanCanvas } from '../canvas/FloorplanCanvas';
import { usePlayback } from '../usePlayback';
import { Topbar } from '../components';

export function ExecScreen() {
  const { selectedHeist, plans, character, intel: intelMap, executeHeist, go } = useGame();
  const def = selectedHeist ? HEIST_BY_ID[selectedHeist] : null;
  const map = useMemo(() => (def ? parseHeist(def) : null), [def]);
  const profile = useMemo(() => buildProfile(character), [character]);
  const plan: Plan | null = def ? plans[def.id] ?? null : null;
  const intel = def ? intelMap[def.id] ?? 0 : 0;

  const seed = useMemo(() => Math.floor(Math.random() * 1_000_000_000), []);
  const [abortAt, setAbortAt] = useState<number | undefined>(undefined);

  const result = useMemo(() => {
    if (!map || !plan) return null;
    // Ausführung simuliert IMMER die volle Wirklichkeit (alle Wachen/Sensoren).
    return simulate(plan, map, profile, { mode: 'exec', seed, intel, abortAt });
  }, [map, plan, profile, seed, intel, abortAt]);

  const pb = usePlayback(result?.snapshots ?? [], true);

  if (!def || !map || !plan || !result) {
    return (
      <div className="app">
        <Topbar />
        <p>Kein ausführbarer Plan. Zurück zum Planen.</p>
        <button onClick={() => go('plan')}>Zur Planung</button>
      </div>
    );
  }

  const finished = pb.time >= pb.duration - 0.01;
  const snap = pb.snap;

  const abort = (): void => {
    if (!snap?.fleeing) setAbortAt(pb.time);
  };
  const finish = (): void => {
    executeHeist(seed, abortAt);
  };

  return (
    <div className="app">
      <Topbar />
      <div className="row between wrap">
        <h1 style={{ margin: 0 }}>🌙 Ausführung: {def.name}</h1>
        <div className="row">
          <span className="tag">Tempo</span>
          {[1, 2, 4].map((sp) => (
            <button
              key={sp}
              className={pb.speed === sp ? 'small primary' : 'small ghost'}
              onClick={() => pb.setSpeed(sp)}
            >
              {sp}×
            </button>
          ))}
        </div>
      </div>

      <div className="plan-layout" style={{ marginTop: 10 }}>
        <div className="canvas-wrap">
          <FloorplanCanvas map={map} snapshot={snap} intel={3} />
        </div>

        <div className="sidebar">
          <div className="card">
            <div className="row between">
              <b>Status</b>
              <span className={snap?.alarm ? 'red' : 'green'}>
                {snap?.alarm ? '🚨 ALARM!' : snap?.fleeing ? 'auf der Flucht' : 'unauffällig'}
              </span>
            </div>
            {snap?.policeEta != null && (
              <div className="red" style={{ marginTop: 4 }}>
                Polizei in {Math.max(0, Math.round(snap.policeEta))}s
              </div>
            )}
            <div className="row between" style={{ marginTop: 4 }}>
              <span className="dim">Zeit</span>
              <span>
                {pb.time.toFixed(1)} / {pb.duration.toFixed(0)}s
              </span>
            </div>
            <div className="transport" style={{ marginTop: 8 }}>
              <button className="small" onClick={pb.toggle}>
                {pb.playing ? '⏸' : '▶'}
              </button>
              <input
                type="range"
                min={0}
                max={pb.duration}
                step={0.1}
                value={pb.time}
                style={{ flex: 1 }}
                onChange={(e) => pb.setTime(Number(e.target.value))}
              />
            </div>
            <div className="row" style={{ marginTop: 8 }}>
              <button
                className="danger"
                onClick={abort}
                disabled={!!snap?.fleeing || finished}
                title="Sofort abbrechen und zum Fluchtwagen"
              >
                🏃 Notabbruch
              </button>
              <button className="primary" onClick={finish} disabled={!finished}>
                Zum Ergebnis →
              </button>
            </div>
            {!finished && (
              <p className="dim" style={{ fontSize: 11, marginTop: 6 }}>
                Lass die Nacht ablaufen … oder brich ab, wenn's brenzlig wird.
              </p>
            )}
            {finished && (
              <p className="green" style={{ fontSize: 12, marginTop: 6 }}>
                Der Bruch ist gelaufen. Sieh dir das Ergebnis an.
              </p>
            )}
          </div>

          <div className="card">
            <h3>Live-Protokoll</h3>
            <div className="log">
              {result.events
                .filter((e) => e.t <= pb.time + 0.01)
                .slice(-40)
                .map((e, i) => (
                  <div className={`ev ${e.kind}`} key={i}>
                    <span className="dim">{e.t.toFixed(0)}s</span> {e.msg}
                  </div>
                ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
