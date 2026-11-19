import { describe, expect, it } from 'vitest';

import type { DynoResultPayload } from '../protocol/messages.ts';
import { addRun, MAX_RUNS, peakOf, removeRun, toggleRun } from './dynoRuns.ts';

const result: DynoResultPayload = {
  rpm: [1000, 2000, 3000],
  torque_Nm: [250, 300, 280],
  power_kW: [26, 63, 88],
  lambda: [0.88, 0.88, null],
  map_kPa: [101, 101, 101],
  ignition_deg: [10, 20, 30],
  egt_C: [780, 790, 800],
  airflow_kg_s: [0.02, 0.05, 0.08],
};

describe('dyno runs', () => {
  it('numbers runs and gives each a free colour slot', () => {
    const runs = addRun(addRun([], 1, result), 2, result);
    expect(runs.map((run) => [run.label, run.colorSlot])).toEqual([
      ['Run 1', 0],
      ['Run 2', 1],
    ]);
  });

  it('keeps colours stable when a run is removed and reuses the freed slot', () => {
    let runs = addRun(addRun(addRun([], 1, result), 2, result), 3, result);
    runs = removeRun(runs, 2);
    expect(runs.map((run) => run.colorSlot)).toEqual([0, 2]);
    runs = addRun(runs, 4, result);
    expect(runs.at(-1)?.colorSlot).toBe(1);
  });

  it('drops the oldest run beyond the palette size', () => {
    let runs = addRun([], 1, result);
    for (let n = 2; n <= MAX_RUNS + 1; n++) runs = addRun(runs, n, result);
    expect(runs).toHaveLength(MAX_RUNS);
    expect(runs[0]?.id).toBe(2);
    expect(runs.at(-1)?.colorSlot).toBe(0);
  });

  it('toggles visibility of one run only', () => {
    const runs = toggleRun(addRun(addRun([], 1, result), 2, result), 1);
    expect(runs.map((run) => run.visible)).toEqual([false, true]);
  });
});

describe('peakOf', () => {
  it('finds the maximum and its rpm', () => {
    expect(peakOf(result.torque_Nm, result.rpm)).toEqual({ value: 300, rpm: 2000 });
  });

  it('returns null for an empty curve', () => {
    expect(peakOf([], [])).toBeNull();
  });
});
