// Herzstück: eine Tick-Simulation, die sowohl die Vorschau (Erwartungswerte,
// nur ausgekundschaftetes Wissen) als auch die Ausführung (echte RNG-Würfe)
// erzeugt. Der Unterschied liegt allein im RNG und im Wissensstand (intel).

import { ACTIONS, DISTRACTION_DELAY, DISTRACTION_LENGTH, DISTRACTION_RADIUS } from './actions';
import { findPath } from './grid';
import { initGuard, snapGuard, tickGuard, type GuardRuntime } from './guards';
import { inCone, lineOfSight } from './los';
import { noiseAt } from './noise';
import { compilePlan, type CompileCtx } from './planner';
import { makeExpectedRng, makeRng, type Rng } from './rng';
import type {
  ActorProfile,
  CompiledStep,
  HeistMap,
  LootItem,
  Plan,
  Sensor,
  SimEvent,
  SimOptions,
  SimResult,
  Snapshot,
  Vec,
} from './types';
import { clamp, dist, idx, key, TICK } from './types';

interface SimState {
  t: number;
  pos: Vec;
  guards: GuardRuntime[];
  opened: Set<number>;
  disabledSensors: Set<string>;
  powerCut: boolean;
  alarm: boolean;
  silentAlarm: boolean;
  policeEta: number | null;
  carried: LootItem[];
  weight: number;
  fleeing: boolean;
  spotted: boolean;
  leftEvidence: boolean;
  phoneCut: boolean;
  /** aktive Ablenkung: Position + Endzeit */
  distraction: { pos: Vec; until: number } | null;
  distractionArmed: { pos: Vec; at: number } | null;
}

const roundV = (v: Vec): Vec => ({ x: Math.round(v.x), y: Math.round(v.y) });

/** Sensor aktiv? (nicht deaktiviert, Strom für Kameras vorhanden) */
function sensorActive(s: Sensor, st: SimState): boolean {
  if (st.disabledSensors.has(s.id)) return false;
  if (s.kind === 'camera' && st.powerCut) return false;
  return true;
}

/** Sichtrichtung einer schwenkenden Kamera zum Zeitpunkt t. */
function sensorFacing(s: Sensor, t: number): number {
  if (s.sweep === 0) return s.dir;
  return s.dir + Math.sin(t * 0.6) * s.sweep;
}

export function simulate(
  plan: Plan,
  map: HeistMap,
  profile: ActorProfile,
  opts: SimOptions,
): SimResult {
  const ctx: CompileCtx = { map, profile };
  const compiled = compilePlan(plan, ctx);
  const rng: Rng = opts.mode === 'preview' ? makeExpectedRng() : makeRng(opts.seed);
  const exec = opts.mode === 'exec';

  // In der Vorschau nur bekannte Wachen/Sensoren simulieren.
  const knownGuards = exec || opts.intel >= 2 ? map.guards : [];
  const st: SimState = {
    t: 0,
    pos: roundV(map.exit),
    guards: knownGuards.map(initGuard),
    opened: new Set(),
    disabledSensors: new Set(),
    powerCut: false,
    alarm: false,
    silentAlarm: false,
    policeEta: null,
    carried: [],
    weight: 0,
    fleeing: false,
    spotted: false,
    leftEvidence: false,
    phoneCut: false,
    distraction: null,
    distractionArmed: null,
  };
  const sensorsKnown = exec || opts.intel >= 3 ? map.sensors : [];

  const events: SimEvent[] = [];
  const snapshots: Snapshot[] = [];
  const log = (kind: SimEvent['kind'], msg: string, pos?: Vec): void => {
    events.push({ t: round1(st.t), kind, msg, pos });
  };

  let rerollLeft = profile.reroll ? 1 : 0;

  const takeSnap = (stepIndex: number, actionLabel?: string): void => {
    snapshots.push({
      t: round1(st.t),
      player: { x: round1(st.pos.x), y: round1(st.pos.y) },
      playerAction: actionLabel,
      stepIndex,
      guards: st.guards.map(snapGuard),
      doorsOpen: [...st.opened],
      alarm: st.alarm,
      silentAlarm: st.silentAlarm,
      policeEta: st.policeEta,
      powerCut: st.powerCut,
      carriedWeight: st.weight,
      fleeing: st.fleeing,
    });
  };

  log('info', exec ? 'Einbruch beginnt …' : 'Vorschau – so ist der Plan gedacht.');

  // Aktive Lärmquelle für Wachen (Position + Stärke) im aktuellen Tick
  let noisePos: Vec | null = null;
  let noiseStrength = 0;

  /** Führt Wachen + Sensoren für dt Sekunden fort und prüft Entdeckung. */
  const advanceWorld = (dt: number, playerMoving: boolean): void => {
    // Ablenkung aktivieren/auslaufen lassen
    if (st.distractionArmed && st.t >= st.distractionArmed.at) {
      st.distraction = { pos: st.distractionArmed.pos, until: st.t + DISTRACTION_LENGTH };
      log('noise', 'Der Kassettenrekorder lärmt los und lockt die Wachen.', st.distractionArmed.pos);
      st.distractionArmed = null;
    }
    let effNoisePos = noisePos;
    let effNoise = noiseStrength;
    if (st.distraction) {
      if (st.t < st.distraction.until) {
        effNoisePos = st.distraction.pos;
        effNoise = 1;
      } else {
        st.distraction = null;
      }
    }

    const stealth = clamp(1 - profile.skills.schleichen / 130, 0.15, 1);
    for (const g of st.guards) {
      const perceivedNoise = effNoisePos
        ? noiseAt(map, effNoisePos, effNoise > 0.9 ? DISTRACTION_RADIUS : effNoise * 12, roundV(g.pos), st.opened)
        : 0;
      const r = tickGuard(
        g,
        map,
        dt,
        {
          player: st.fleeing && st.pos.x === map.exit.x && st.pos.y === map.exit.y ? null : st.pos,
          playerMoving,
          stealth,
          powerCut: st.powerCut,
          opened: st.opened,
          noiseSource: effNoisePos,
          noiseLevel: perceivedNoise,
          detectionFactor: profile.detectionFactor,
        },
        st.phoneCut ? undefined : map.phone,
        st.alarm,
      );
      if (r.sawPlayer && !st.spotted) {
        st.spotted = true;
        log('spotted', `${g.def.name} hat dich entdeckt!`, roundV(g.pos));
        if (profile.detectionFactor < 1) {
          /* sechster Sinn: früher gewarnt (nur Preview-Flag) */
        }
      }
      if (r.reachedPhone && !st.alarm) {
        triggerAlarm(`${g.def.name} hat die Polizei gerufen!`);
      }
      // Wache in Nahkontakt während Alarm → Verhaftung
      if (st.alarm && dist(g.pos, st.pos) < 0.9 && !st.fleeing) {
        // gefasst mitten im Raum
        st.pos = { ...st.pos };
      }
    }

    // Sensoren
    for (const s of sensorsKnown) {
      if (!sensorActive(s, st)) continue;
      if (s.kind === 'plate') {
        if (roundV(st.pos).x === s.pos.x && roundV(st.pos).y === s.pos.y && !st.alarm) {
          triggerAlarm('Du bist auf eine Druckplatte getreten – Alarm!');
        }
        continue;
      }
      const facing = sensorFacing(s, st.t);
      if (
        inCone(s.pos, facing, s.fov, s.range, st.pos) &&
        lineOfSight(map, s.pos, roundV(st.pos), st.opened)
      ) {
        if (!st.alarm) {
          triggerAlarm(
            s.kind === 'camera'
              ? 'Eine Kamera hat dich erfasst – Alarm!'
              : 'Ein Bewegungsmelder hat ausgelöst – Alarm!',
            s.pos,
          );
        }
      }
    }

    // Polizei-Countdown
    if (st.policeEta !== null) {
      st.policeEta -= dt;
      if (st.policeEta <= 0) {
        st.policeEta = 0;
      }
    }
    st.t += dt;
    noisePos = null;
    noiseStrength = 0;
  };

  const triggerAlarm = (msg: string, pos?: Vec): void => {
    if (st.alarm) return;
    st.alarm = true;
    st.spotted = true;
    const factor = 1; // Fahndung fließt über policeBase bereits ein
    st.policeEta = map.policeBase * factor;
    log('alarm', msg, pos);
    log('police', `Die Polizei ist in ~${Math.round(st.policeEta)}s da. Nichts wie raus!`);
  };

  const emitNoise = (pos: Vec, radius: number): void => {
    if (radius <= 0) return;
    noisePos = pos;
    noiseStrength = Math.max(noiseStrength, radius / 12);
    // sofortige Wach-Reaktion wird im nächsten advanceWorld verarbeitet
  };

  // ---- Schritte abarbeiten -------------------------------------------------
  let aborted = false;
  for (let i = 0; i < compiled.steps.length; i++) {
    const cs = compiled.steps[i];
    if (checkAbort()) {
      aborted = true;
      break;
    }
    // Bewegung des Schrittes
    if (cs.path.length > 1 && cs.moveDuration > 0) {
      walkPath(cs, i);
      if (checkAbort()) {
        aborted = true;
        break;
      }
    }
    // Aktion des Schrittes
    if (cs.step.action !== 'moveTo') {
      doAction(cs, i);
    }
    if (st.alarm && shouldBail()) {
      // nach Alarm sofort zur Flucht übergehen
      break;
    }
  }

  // ---- Flucht --------------------------------------------------------------
  flee(compiled.steps.length);

  // ---- Ergebnis ------------------------------------------------------------
  const atExit = key(roundV(st.pos)) === key(roundV(map.exit));
  let outcome: SimResult['outcome'];
  if (st.alarm) {
    if (atExit && (st.policeEta === null || st.policeEta > 0) && !caughtByGuard()) {
      outcome = 'escaped';
      log('flee', 'Mit quietschenden Reifen entkommen – knapp!');
    } else {
      outcome = 'caught';
      st.carried = [];
      log('outcome', 'Geschnappt! Die Beute ist futsch.');
    }
  } else if (aborted) {
    outcome = 'aborted';
    log('flee', 'Einbruch abgebrochen und unbemerkt verschwunden.');
  } else {
    outcome = 'success';
    log('outcome', 'Sauberer Coup – niemand hat etwas gemerkt.');
  }
  takeSnap(compiled.steps.length);

  return {
    outcome,
    events,
    snapshots,
    lootTaken: st.carried,
    alarmTriggered: st.alarm,
    wasSpotted: st.spotted,
    duration: round1(st.t),
    leftEvidence: st.leftEvidence,
  };

  // ---- Hilfsfunktionen -----------------------------------------------------
  function checkAbort(): boolean {
    return opts.abortAt !== undefined && st.t >= opts.abortAt && !st.fleeing;
  }
  function shouldBail(): boolean {
    return st.alarm; // nach Alarm restliche Plan-Schritte abbrechen
  }
  function caughtByGuard(): boolean {
    return st.guards.some((g) => g.state === 'chase' && dist(g.pos, st.pos) < 1.2);
  }

  function walkPath(cs: CompiledStep, stepIndex: number): void {
    const mode = cs.step.mode ?? 'sneak';
    const speed = mode === 'sneak' ? profile.sneakSpeed : profile.walkSpeed;
    const perStep = 1 / speed;
    for (let p = 1; p < cs.path.length; p++) {
      st.pos = { ...cs.path[p] };
      // Bewegungslärm
      const noise = mode === 'sneak' ? profile.sneakNoise : profile.walkNoise;
      if (noise > 0) emitNoise(st.pos, noise);
      // Tür/Fenster betreten = evtl. bereits offen (über Aktion), sonst Warnung ignorieren
      subTicks(perStep, true, stepIndex, mode === 'sneak' ? 'Schleicht' : 'Geht');
      if (checkAbort()) return;
      if (st.alarm && st.fleeing) return;
    }
  }

  function doAction(cs: CompiledStep, stepIndex: number): void {
    const spec = ACTIONS[cs.step.action as keyof typeof ACTIONS];
    if (cs.step.action === 'wait') {
      const dur = cs.step.waitTime ?? 5;
      subTicks(dur, false, stepIndex, 'Wartet');
      return;
    }
    if (!spec) return;

    // Rekorder: Ablenkung scharf stellen (wirkt verzögert)
    if (cs.step.action === 'placeDistraction') {
      const pos = cs.step.target ? roundV(cs.step.target) : { ...st.pos };
      st.distractionArmed = { pos, at: st.t + DISTRACTION_DELAY };
      subTicks(spec.duration({ profile }), false, stepIndex, spec.label);
      log('action', 'Kassettenrekorder abgestellt (löst gleich Lärm aus).', pos);
      return;
    }

    // Fehlschlag würfeln
    let fail = rng.chance(cs.failChance);
    if (fail && rerollLeft > 0 && exec) {
      rerollLeft--;
      fail = rng.chance(cs.failChance);
      log('info', 'Glückspilz: Zweiter Versuch!');
    }

    const baseDur = cs.actionDuration;
    const dur = fail ? baseDur * 1.4 : baseDur;
    // Aktionslärm über die Dauer verteilt
    subTicks(dur, false, stepIndex, spec.label, () => {
      if (cs.noise > 0) emitNoise(actionPos(cs), cs.noise);
    });

    if (fail) {
      log('fail', `${spec.label} misslingt – lautes Missgeschick!`, actionPos(cs));
      emitNoise(actionPos(cs), spec.failNoise);
      // Fehlschlag-Lärm sofort verarbeiten
      advanceWorld(TICK, false);
    } else {
      applySuccess(cs);
    }
  }

  function actionPos(cs: CompiledStep): Vec {
    if (cs.step.target) return roundV(cs.step.target);
    return roundV(st.pos);
  }

  function applySuccess(cs: CompiledStep): void {
    const step = cs.step;
    const spec = ACTIONS[step.action as keyof typeof ACTIONS];
    switch (step.action) {
      case 'openDoor':
      case 'pickLock':
      case 'forceDoor':
      case 'cutWindow':
      case 'smashWindow':
      case 'climb':
        if (step.target) {
          st.opened.add(idx(roundV(step.target), map.w));
          log('action', `${spec?.label} geschafft.`, roundV(step.target));
        }
        if (step.action === 'smashWindow' || step.action === 'forceDoor') st.leftEvidence = true;
        break;
      case 'crackSafe':
      case 'drillSafe':
      case 'grindSafe': {
        const c = map.containers.find((x) => x.id === step.targetId);
        if (c) {
          st.disabledSensors.add('open:' + c.id);
          log('action', `${c.name} ist offen.`, c.pos);
          if (c.funkalarm && !profile.tools.includes('stoersender')) {
            st.silentAlarm = true;
            triggerAlarm('Stiller Funkalarm am Safe ausgelöst!', c.pos);
          }
        }
        break;
      }
      case 'cutVitrine':
      case 'smashVitrine': {
        const c = map.containers.find((x) => x.id === step.targetId);
        if (c) {
          st.disabledSensors.add('open:' + c.id);
          log('action', `${c.name} geöffnet.`, c.pos);
          if (step.action === 'smashVitrine') st.leftEvidence = true;
          if (c.funkalarm && !profile.tools.includes('stoersender')) {
            triggerAlarm('Funkalarm an der Vitrine!', c.pos);
          }
        }
        break;
      }
      case 'grabLoot': {
        const loot = map.loot.find((x) => x.id === step.targetId);
        if (loot) {
          const inContainer = loot.containerId ? st.disabledSensors.has('open:' + loot.containerId) : true;
          if (!inContainer) {
            log('fail', `${loot.name}: Behälter ist noch zu.`);
            break;
          }
          if (st.weight + loot.weight > profile.carryCapacity) {
            log('fail', `${loot.name} zu schwer – Tragkraft erschöpft.`);
            break;
          }
          if (loot.bulky && !profile.hasVehicle) {
            log('fail', `${loot.name} zu sperrig ohne Fahrzeug.`);
            break;
          }
          st.carried.push(loot);
          st.weight += loot.weight;
          log('loot', `${loot.icon} ${loot.name} eingesackt (${loot.value} DM).`, loot.pos);
        }
        break;
      }
      case 'disableSensor':
      case 'disableCamera':
        if (step.targetId) {
          st.disabledSensors.add(step.targetId);
          log('action', `${spec?.label} erfolgreich.`, step.target ? roundV(step.target) : undefined);
        }
        break;
      case 'cutPower':
        st.powerCut = true;
        log('action', 'Strom gekappt – Kameras und Licht sind aus.', map.fusebox);
        break;
      case 'cutPhone':
        log('action', 'Telefonleitung durchtrennt – kein Anruf mehr möglich.', map.phone);
        // Wachen können nicht mehr telefonieren (Flag statt Map-Mutation)
        st.phoneCut = true;
        break;
    }
  }

  /** Lässt dt Sekunden in TICK-Schritten verstreichen und aktualisiert die Welt. */
  function subTicks(
    dt: number,
    moving: boolean,
    stepIndex: number,
    label: string,
    onTick?: () => void,
  ): void {
    let left = dt;
    while (left > 1e-6) {
      const step = Math.min(TICK, left);
      onTick?.();
      advanceWorld(step, moving);
      left -= step;
      takeSnap(stepIndex, label);
      if (st.alarm && !st.fleeing && shouldBail()) return;
      if (checkAbort()) return;
    }
  }

  function flee(stepIndex: number): void {
    if (key(roundV(st.pos)) === key(roundV(map.exit))) return;
    st.fleeing = true;
    if (st.alarm || aborted) log('flee', 'Auf zum Fluchtwagen!');
    const path = findPath(map, roundV(st.pos), roundV(map.exit), {
      opened: st.opened,
      passDoors: false,
      passWindows: false,
    });
    if (!path) {
      log('fail', 'Kein Fluchtweg frei!');
      return;
    }
    const speed = profile.walkSpeed * (st.alarm ? 1 + profile.alarmSpeedBonus + profile.escapeBonus : 1);
    const perStep = 1 / speed;
    for (let p = 1; p < path.length; p++) {
      st.pos = { ...path[p] };
      subTicks(perStep, true, stepIndex, 'Flieht');
      if (caughtByGuard() && st.alarm) {
        log('spotted', 'Eine Wache stellt dich auf der Flucht!');
        return;
      }
      if (st.policeEta !== null && st.policeEta <= 0) {
        log('police', 'Die Polizei riegelt ab, bevor du raus bist!');
        return;
      }
    }
  }
}

const round1 = (n: number): number => Math.round(n * 10) / 10;
