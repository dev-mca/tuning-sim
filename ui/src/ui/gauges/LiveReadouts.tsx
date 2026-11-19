import { useRef } from 'react';

import { smoothTowards } from '../../domain/smoothing.ts';
import { kwToPs } from '../../domain/units.ts';
import type { EngineState } from '../../protocol/messages.ts';
import { useStore } from '../../state/StoreContext.ts';
import { useAnimationFrame } from '../useAnimationFrame.ts';

interface Readout {
  key: string;
  label: string;
  unit: string;
  read: (state: EngineState) => number;
  format: (value: number) => string;
}

const KG_S_TO_G_S = 1000;

const READOUTS: readonly Readout[] = [
  {
    key: 'torque',
    label: 'Torque',
    unit: 'Nm',
    read: (state) => state.torque_Nm,
    format: (value) => value.toFixed(0),
  },
  {
    key: 'power',
    label: 'Power',
    unit: 'kW',
    read: (state) => state.power_kW,
    format: (value) => `${value.toFixed(0)} · ${kwToPs(value).toFixed(0)} PS`,
  },
  {
    key: 'airflow',
    label: 'Air',
    unit: 'g/s',
    read: (state) => state.airflow_kg_s * KG_S_TO_G_S,
    format: (value) => value.toFixed(1),
  },
  {
    key: 'pedal',
    label: 'Pedal',
    unit: '%',
    read: (state) => state.pedal * 100,
    format: (value) => value.toFixed(0),
  },
];

/** Smoothing of the digital numbers; calmer than needles so they stay readable. */
const READOUT_TIME_CONSTANT_S = 0.15;

/** Digital values next to the gauges, updated per animation frame like the needles. */
export function LiveReadouts() {
  const store = useStore();
  const valueRefs = useRef(new Map<string, HTMLElement>());
  const smoothed = useRef(new Map<string, number>());

  useAnimationFrame((dt_s) => {
    const state = store.latestState();
    if (state === null) {
      return;
    }
    for (const readout of READOUTS) {
      const target = readout.read(state);
      const value = smoothTowards(
        smoothed.current.get(readout.key) ?? target,
        target,
        dt_s,
        READOUT_TIME_CONSTANT_S,
      );
      smoothed.current.set(readout.key, value);
      const element = valueRefs.current.get(readout.key);
      if (element !== undefined) {
        element.textContent = readout.format(value);
      }
    }
  });

  return (
    <dl className="col-span-2 grid grid-cols-2 gap-2 sm:col-span-5 sm:grid-cols-4 xl:col-span-1 xl:grid-cols-1">
      {READOUTS.map((readout) => (
        <div key={readout.key} className="rounded-lg border border-line bg-raised px-3 py-1.5">
          <dt className="text-[0.65rem] font-semibold tracking-[0.2em] text-muted uppercase">
            {readout.label} <span className="normal-case tracking-normal">({readout.unit})</span>
          </dt>
          <dd
            ref={(element) => {
              if (element !== null) {
                valueRefs.current.set(readout.key, element);
              } else {
                valueRefs.current.delete(readout.key);
              }
            }}
            className="font-mono text-lg font-semibold text-ink tabular-nums"
          >
            —
          </dd>
        </div>
      ))}
    </dl>
  );
}
