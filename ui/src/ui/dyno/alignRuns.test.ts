import { describe, expect, it } from 'vitest';

import type { DynoResultPayload } from '../../protocol/messages.ts';
import { alignRuns, valueAtRpm } from './alignRuns.ts';

function run(rpm: number[], torque: number[]): DynoResultPayload {
  const zeros = rpm.map(() => 0);
  return {
    kind: 'engine',
    gear: null,
    rpm,
    torque_Nm: torque,
    power_kW: zeros,
    lambda: rpm.map(() => null),
    map_kPa: zeros,
    ignition_deg: zeros,
    egt_C: zeros,
    airflow_kg_s: zeros,
  };
}

const torque = (result: DynoResultPayload) => result.torque_Nm;

describe('alignRuns', () => {
  it('interpolates runs with different measured speeds onto one x axis', () => {
    const aligned = alignRuns(
      [run([1000, 1100], [250, 260]), run([1050.5, 1150], [300, 310])],
      torque,
    );
    expect(aligned.rpm).toEqual([1000, 1050.5, 1100, 1150]);
    expect(aligned.values[0]?.[1]).toBeCloseTo(255.05, 9);
    expect(aligned.values[1]?.[2]).toBeCloseTo(300 + (10 * 49.5) / 99.5, 9);
  });

  it('leaves values outside a run range empty', () => {
    const aligned = alignRuns(
      [run([1000, 1100], [250, 252]), run([1100, 1200], [260, 262])],
      torque,
    );
    expect(aligned.values[0]?.[2]).toBeNull();
    expect(aligned.values[1]?.[0]).toBeNull();
  });

  it('returns empty arrays without runs', () => {
    expect(alignRuns([], torque)).toEqual({ rpm: [], values: [] });
  });

  it('keeps null values (e.g. lambda in fuel cut) as gaps', () => {
    const aligned = alignRuns([run([1000, 1100], [1, 2])], (result) => result.lambda);
    expect(aligned.values[0]).toEqual([null, null]);
  });
});

describe('valueAtRpm', () => {
  it('interpolates inside the run and returns null outside', () => {
    const result = run([1000, 1100], [250, 252]);
    expect(valueAtRpm(result, torque, 1100)).toBe(252);
    expect(valueAtRpm(result, torque, 1050)).toBeCloseTo(251, 9);
    expect(valueAtRpm(result, torque, 5000)).toBeNull();
  });
});
