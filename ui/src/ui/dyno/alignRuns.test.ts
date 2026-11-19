import { describe, expect, it } from 'vitest';

import type { DynoResultPayload } from '../../protocol/messages.ts';
import { alignRuns, valueAtRpm } from './alignRuns.ts';

function run(rpm: number[], torque: number[]): DynoResultPayload {
  const zeros = rpm.map(() => 0);
  return {
    rpm,
    torque_Nm: torque,
    power_kW: zeros,
    lambda: zeros,
    map_kPa: zeros,
    ignition_deg: zeros,
    egt_C: zeros,
    airflow_kg_s: zeros,
  };
}

describe('alignRuns', () => {
  it('builds the union of rpm points and fills gaps with null', () => {
    const aligned = alignRuns(
      [run([1000, 1100], [250, 252]), run([1100, 1200], [260, 262])],
      (result) => result.torque_Nm,
    );
    expect(aligned.rpm).toEqual([1000, 1100, 1200]);
    expect(aligned.values).toEqual([
      [250, 252, null],
      [null, 260, 262],
    ]);
  });

  it('returns empty arrays without runs', () => {
    expect(alignRuns([], (result) => result.torque_Nm)).toEqual({ rpm: [], values: [] });
  });
});

describe('valueAtRpm', () => {
  it('reads a run at an rpm and returns null outside its range', () => {
    const result = run([1000, 1100], [250, 252]);
    expect(valueAtRpm(result, (r) => r.torque_Nm, 1100)).toBe(252);
    expect(valueAtRpm(result, (r) => r.torque_Nm, 5000)).toBeNull();
  });
});
