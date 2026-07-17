import { useMemo, useState } from 'react';
import { useGame } from '../../game/state';
import { buildProfile, totalCarry } from '../../game/character';
import { HEIST_BY_ID } from '../../game/data/heists';
import { parseHeist } from '../../engine/parse';
import { compilePlan } from '../../engine/planner';
import { simulate } from '../../engine/simulate';
import { ACTION_LABELS } from '../../engine/actions';
import type { Plan, PlanStep, SimResult, Vec } from '../../engine/types';
import { FloorplanCanvas } from '../canvas/FloorplanCanvas';
import { actionsAt, describeTile, type ActionChoice } from '../planActions';
import { usePlayback } from '../usePlayback';
import { Topbar } from '../components';

let stepSeq = 0;
const newId = (): string => `s${Date.now()}_${stepSeq++}`;

interface Ctx {
  tile: Vec;
  screen: { x: number; y: number };
  choices: ActionChoice[];
  title: string;
}

export function PlanScreen() {
  const { selectedHeist, plans, savePlan, go, character, intel: intelMap } = useGame();
  const def = selectedHeist ? HEIST_BY_ID[selectedHeist] : null;
  const map = useMemo(() => (def ? parseHeist(def) : null), [def]);
  const profile = useMemo(() => buildProfile(character), [character]);
  const intel = def ? intelMap[def.id] ?? 0 : 0;

  const [steps, setSteps] = useState<PlanStep[]>(() =>
    def && plans[def.id] ? plans[def.id].steps.map((s) => ({ ...s })) : [],
  );
  const [ctx, setCtx] = useState<Ctx | null>(null);
  const [preview, setPreview] = useState<SimResult | null>(null);

  const plan: Plan = useMemo(() => ({ heistId: def?.id ?? '', steps }), [def, steps]);
  const compiled = useMemo(
    () => (map ? compilePlan(plan, { map, profile }) : null),
    [map, plan, profile],
  );
  const pb = usePlayback(preview?.snapshots ?? []);

  if (!def || !map) {
    return (
      <div className="app">
        <Topbar />
        <p>Kein Objekt gewählt.</p>
        <button onClick={() => go('heists')}>Zu den Objekten</button>
      </div>
    );
  }

  const plannedWeight = steps
    .filter((s) => s.action === 'grabLoot')
    .reduce((sum, s) => sum + (map.loot.find((l) => l.id === s.targetId)?.weight ?? 0), 0);
  const capacity = totalCarry(character);

  const onPick = (tile: Vec, screen: { x: number; y: number }): void => {
    const choices = actionsAt(map, tile, profile);
    if (choices.length === 0) {
      setCtx(null);
      return;
    }
    setCtx({ tile, screen, choices, title: describeTile(map, tile) });
  };

  const addStep = (ch: ActionChoice): void => {
    setSteps((prev) => [
      ...prev,
      {
        id: newId(),
        action: ch.action,
        target: ch.target,
        targetId: ch.targetId,
        tool: ch.tool,
        mode: 'sneak',
        waitTime: ch.waitTime,
      },
    ]);
    setCtx(null);
    setPreview(null);
  };

  const addWait = (): void => {
    setSteps((prev) => [...prev, { id: newId(), action: 'wait', waitTime: 5 }]);
    setPreview(null);
  };

  const move = (i: number, dir: -1 | 1): void => {
    setSteps((prev) => {
      const next = [...prev];
      const j = i + dir;
      if (j < 0 || j >= next.length) return prev;
      [next[i], next[j]] = [next[j], next[i]];
      return next;
    });
    setPreview(null);
  };
  const remove = (i: number): void => {
    setSteps((prev) => prev.filter((_, k) => k !== i));
    setPreview(null);
  };

  const runPreview = (): void => {
    const res = simulate(plan, map, profile, { mode: 'preview', seed: 1, intel });
    setPreview(res);
  };

  const execute = (): void => {
    savePlan(plan);
    go('exec');
  };

  const activeStep = preview ? (pb.snap?.stepIndex ?? -1) : -1;

  return (
    <div className="app">
      <Topbar />
      <div className="row between wrap">
        <h1 style={{ margin: 0 }}>
          {def.name} <span className="gold">{'★'.repeat(def.difficulty)}</span>
        </h1>
        <div className="row wrap">
          <button className="small ghost" onClick={() => go('heists')}>
            ← Objekte
          </button>
          <button className="small" onClick={() => go('informant')}>
            🕶️ Mehr auskundschaften (Stufe {intel}/3)
          </button>
        </div>
      </div>

      <div className="plan-layout" style={{ marginTop: 10 }}>
        <div className="canvas-wrap" style={{ position: 'relative' }}>
          <FloorplanCanvas
            map={map}
            compiled={compiled}
            snapshot={preview ? pb.snap : null}
            intel={intel}
            activeStep={activeStep}
            selected={ctx?.tile ?? null}
            onPick={onPick}
          />
          {ctx && (
            <div
              className="context-menu"
              style={{ left: Math.min(ctx.screen.x, window.innerWidth - 220), top: ctx.screen.y }}
              onMouseLeave={() => setCtx(null)}
            >
              <div className="ctx-title">{ctx.title}</div>
              {ctx.choices.map((c, i) => (
                <button
                  key={i}
                  className="small"
                  disabled={!!c.disabledReason}
                  title={c.disabledReason ?? ''}
                  onClick={() => addStep(c)}
                >
                  {c.label}
                  {c.disabledReason && <span className="red"> · {c.disabledReason}</span>}
                </button>
              ))}
              <button className="small ghost" onClick={() => setCtx(null)}>
                Abbrechen
              </button>
            </div>
          )}
        </div>

        <div className="sidebar">
          <div className="card">
            <div className="row between">
              <b>Beute-Traglast</b>
              <span className={plannedWeight > capacity ? 'red' : 'green'}>
                {plannedWeight} / {capacity} kg
              </span>
            </div>
            <div className="help" style={{ marginTop: 6 }}>
              Klick auf ein Feld, ein Schloss, einen Safe oder Sensor öffnet die Aktionen.
              Grüne Punkte = geplante Schritte. Erst planen, dann per Vorschau testen.
            </div>
          </div>

          <div className="card">
            <div className="row between">
              <h3 style={{ margin: 0 }}>Plan ({steps.length})</h3>
              <div className="row">
                <button className="small" onClick={addWait} title="Warteschritt einfügen">
                  + Warten
                </button>
                <button className="small ghost danger" onClick={() => { setSteps([]); setPreview(null); }}>
                  Leeren
                </button>
              </div>
            </div>
            <div className="steplist" style={{ marginTop: 6 }}>
              {steps.length === 0 && <div className="dim">Noch keine Schritte geplant.</div>}
              {steps.map((s, i) => {
                const warn = compiled?.steps[i]?.warnings ?? [];
                const loot = s.targetId ? map.loot.find((l) => l.id === s.targetId) : null;
                return (
                  <div className={`step ${warn.length ? 'warn' : ''}`} key={s.id}>
                    <span className="num">{i + 1}</span>
                    <span className="grow">
                      {ACTION_LABELS[s.action]}
                      {s.action === 'wait' && ` ${s.waitTime}s`}
                      {loot && ` – ${loot.icon}${loot.name}`}
                      {warn.length > 0 && <span className="red"> ⚠ {warn[0]}</span>}
                    </span>
                    <button className="small ghost" onClick={() => move(i, -1)} title="hoch">
                      ▲
                    </button>
                    <button className="small ghost" onClick={() => move(i, 1)} title="runter">
                      ▼
                    </button>
                    <button className="small ghost danger" onClick={() => remove(i)} title="löschen">
                      ✕
                    </button>
                  </div>
                );
              })}
            </div>
          </div>

          {compiled && (
            <div className="card">
              <div className="row between">
                <b>Geplante Dauer</b>
                <span>{Math.round(compiled.totalTime)}s</span>
              </div>
              <div className="timeline" style={{ marginTop: 6 }}>
                {compiled.steps.map((cs, i) => {
                  const dur = cs.moveDuration + cs.actionDuration;
                  const pct = compiled.totalTime ? (dur / compiled.totalTime) * 100 : 0;
                  const col = cs.warnings.length
                    ? '#ff5470'
                    : cs.step.action === 'moveTo'
                      ? '#23e0e0'
                      : cs.step.action === 'wait'
                        ? '#8a5cff'
                        : '#ffce54';
                  return (
                    <div
                      key={i}
                      className="seg"
                      style={{ width: `${pct}%`, background: col }}
                      title={`${ACTION_LABELS[cs.step.action]} · ${Math.round(dur)}s`}
                    >
                      {pct > 8 ? i + 1 : ''}
                    </div>
                  );
                })}
              </div>
              {compiled.warnings.length > 0 && (
                <div style={{ marginTop: 6 }}>
                  {compiled.warnings.map((w, i) => (
                    <div key={i} className="red" style={{ fontSize: 12 }}>
                      ⚠ {w}
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}

          <div className="card">
            <div className="row">
              <button className="cyan" onClick={runPreview} disabled={steps.length === 0}>
                👁 Vorschau
              </button>
              <button className="primary" onClick={execute} disabled={steps.length === 0}>
                🌙 Ausführen
              </button>
            </div>
            {preview && (
              <>
                <div className="transport" style={{ marginTop: 8 }}>
                  <button className="small" onClick={pb.toggle}>
                    {pb.playing ? '⏸' : '▶'}
                  </button>
                  <button className="small" onClick={pb.restart}>
                    ⏮
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
                  <span className="dim" style={{ fontSize: 11 }}>
                    {pb.time.toFixed(1)}/{pb.duration.toFixed(0)}s
                  </span>
                </div>
                <div className="row" style={{ marginTop: 4 }}>
                  <span className="dim" style={{ fontSize: 11 }}>
                    Tempo:
                  </span>
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
                <PreviewVerdict preview={preview} intel={intel} />
              </>
            )}
          </div>

          {preview && (
            <div className="card">
              <h3>Ablauf (Vorschau)</h3>
              <div className="log">
                {preview.events
                  .filter((e) => e.t <= pb.time + 0.01)
                  .slice(-40)
                  .map((e, i) => (
                    <div className={`ev ${e.kind}`} key={i}>
                      <span className="dim">{e.t.toFixed(0)}s</span> {e.msg}
                    </div>
                  ))}
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

function PreviewVerdict({ preview, intel }: { preview: SimResult; intel: number }) {
  const value = preview.lootTaken.reduce((s, l) => s + l.value, 0);
  const good = preview.outcome === 'success';
  return (
    <div style={{ marginTop: 8, fontSize: 12 }}>
      <div className={good ? 'green' : preview.alarmTriggered ? 'red' : 'gold'}>
        Erwartung: {labelOutcome(preview.outcome)} · Beute ~{value} DM
      </div>
      {intel < 3 && (
        <div className="dim">
          Achtung: Bei Auskundschaftung {intel}/3 kennst du noch nicht alle Gefahren –
          die echte Ausführung kann überraschen.
        </div>
      )}
    </div>
  );
}

function labelOutcome(o: SimResult['outcome']): string {
  switch (o) {
    case 'success':
      return 'sauberer Coup';
    case 'escaped':
      return 'Flucht nach Alarm';
    case 'caught':
      return 'erwischt!';
    case 'aborted':
      return 'Abbruch';
  }
}
