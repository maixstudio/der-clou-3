import { useEffect, useRef, useState } from 'react';
import type { Snapshot } from '../engine/types';

export interface Playback {
  snap: Snapshot | null;
  time: number;
  playing: boolean;
  speed: number;
  duration: number;
  play: () => void;
  pause: () => void;
  toggle: () => void;
  restart: () => void;
  setTime: (t: number) => void;
  setSpeed: (s: number) => void;
}

/** Animiert eine Snapshot-Folge über die Simulationszeit. */
export function usePlayback(snapshots: Snapshot[], autoplay = false): Playback {
  const [time, setTime] = useState(0);
  const [playing, setPlaying] = useState(autoplay);
  const [speed, setSpeed] = useState(2);
  const raf = useRef<number>(0);
  const last = useRef<number>(0);
  const duration = snapshots.length ? snapshots[snapshots.length - 1].t : 0;

  // Bei neuer Snapshot-Folge zurücksetzen
  useEffect(() => {
    setTime(0);
    setPlaying(autoplay);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [snapshots]);

  useEffect(() => {
    if (!playing) return;
    last.current = performance.now();
    const tick = (now: number): void => {
      const dt = ((now - last.current) / 1000) * speed;
      last.current = now;
      setTime((t) => {
        const nt = t + dt;
        if (nt >= duration) {
          setPlaying(false);
          return duration;
        }
        return nt;
      });
      raf.current = requestAnimationFrame(tick);
    };
    raf.current = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf.current);
  }, [playing, speed, duration]);

  // Snapshot zum aktuellen Zeitpunkt (letzter mit t <= time)
  let snap: Snapshot | null = null;
  for (const s of snapshots) {
    if (s.t <= time + 1e-6) snap = s;
    else break;
  }
  if (!snap && snapshots.length) snap = snapshots[0];

  return {
    snap,
    time,
    playing,
    speed,
    duration,
    play: () => setPlaying(true),
    pause: () => setPlaying(false),
    toggle: () => setPlaying((p) => !p),
    restart: () => {
      setTime(0);
      setPlaying(true);
    },
    setTime: (t) => {
      setPlaying(false);
      setTime(t);
    },
    setSpeed,
  };
}
