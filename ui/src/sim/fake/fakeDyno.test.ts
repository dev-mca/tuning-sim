import { describe, expect, it } from 'vitest';

import { applyCells } from '../../domain/tableEdit.ts';
import type { TableData, TableId } from '../../protocol/messages.ts';
import {
  fakeDynoResult,
  goldenAt,
  ignitionEfficiency,
  lambdaEfficiency,
  measuredLambda,
  powerFromTorque_kW,
  requireTable,
  type TableSet,
} from './fakeDyno.ts';
import { GOLDEN_SWEEP, parseGoldenSweep, referenceTables } from './referenceData.ts';

function tableSet(): Map<TableId, TableData> {
  return new Map(referenceTables().map((table) => [table.id, table]));
}

/** Shifts every cell of a table by `delta`, e.g. the whole ignition map 5 deg later. */
function shiftTable(set: TableSet, id: TableId, delta: number): Map<TableId, TableData> {
  const table = requireTable(set, id);
  const cells = table.values.flatMap((row, y) =>
    row.map((value, x) => ({ x, y, value: value + delta })),
  );
  return new Map(set).set(id, applyCells(table, cells));
}

describe('golden reference data', () => {
  it('parses the shared golden sweep with plausible values (research.md §10)', () => {
    expect(GOLDEN_SWEEP.length).toBeGreaterThan(50);
    const peakTorque = Math.max(...GOLDEN_SWEEP.map((point) => point.torque_Nm));
    const peakPower = Math.max(...GOLDEN_SWEEP.map((point) => point.power_kW));
    expect(peakTorque).toBeGreaterThan(280);
    expect(peakTorque).toBeLessThan(330);
    expect(peakPower).toBeGreaterThan(190);
    expect(peakPower).toBeLessThan(230);
  });

  it('finds columns by name and rejects files without them', () => {
    const csv = '# c\nspeedSetpoint_rpm,torque_Nm\n1000,1\n';
    expect(() => parseGoldenSweep(csv)).toThrow(/power_kW/);
  });

  it('interpolates linearly between golden points and clamps at the ends', () => {
    const first = GOLDEN_SWEEP[0];
    const second = GOLDEN_SWEEP[1];
    if (first === undefined || second === undefined) throw new Error('golden data missing');
    const middle = goldenAt((first.rpm + second.rpm) / 2);
    expect(middle.torque_Nm).toBeCloseTo((first.torque_Nm + second.torque_Nm) / 2, 9);
    expect(goldenAt(0).torque_Nm).toBe(first.torque_Nm);
  });
});

describe('efficiency factors', () => {
  it('is 1 at MBT and loses about 5 % at 10 deg off (research.md §2.7)', () => {
    expect(ignitionEfficiency(30, 30)).toBe(1);
    expect(ignitionEfficiency(20, 30)).toBeCloseTo(0.95, 9);
  });

  it('has its lambda maximum at the optimum', () => {
    expect(lambdaEfficiency(0.88)).toBe(1);
    expect(lambdaEfficiency(1.0)).toBeLessThan(1);
  });

  it('measures lean lambda when the ECU believes in too much VE', () => {
    expect(measuredLambda(1.0, 90, 100)).toBeCloseTo(0.9, 9);
    expect(measuredLambda(1.0, 100, 90)).toBeCloseTo(1.111, 3);
  });

  it('converts torque to power: 300 Nm at 6000 rpm are about 188 kW', () => {
    expect(powerFromTorque_kW(300, 6000)).toBeCloseTo(188.5, 1);
  });
});

describe('fakeDynoResult', () => {
  it('reproduces the golden curve with the unchanged tune', () => {
    const reference = tableSet();
    const result = fakeDynoResult(reference, reference, 1000, 7000);
    expect(result.rpm[0]).toBe(1000);
    expect(result.rpm.at(-1)).toBe(7000);
    expect(result.rpm).toHaveLength(61);
    const index = result.rpm.indexOf(4000);
    expect(result.torque_Nm[index]).toBeCloseTo(goldenAt(4000).torque_Nm, 6);
  });

  it('loses torque and runs hotter with retarded ignition', () => {
    const reference = tableSet();
    const retarded = shiftTable(reference, 'ignition', -5);
    const base = fakeDynoResult(reference, reference, 2000, 6000);
    const late = fakeDynoResult(retarded, reference, 2000, 6000);
    const sum = (values: number[]): number => values.reduce((a, b) => a + b, 0);
    expect(sum(late.torque_Nm)).toBeLessThan(sum(base.torque_Nm));
    expect(sum(late.egt_C)).toBeGreaterThan(sum(base.egt_C));
  });

  it('reports a richer lambda when the ECU VE table is raised', () => {
    const reference = tableSet();
    const richer = shiftTable(reference, 've', 10);
    const result = fakeDynoResult(richer, reference, 3000, 3000);
    expect(result.lambda[0]).toBeLessThan(
      fakeDynoResult(reference, reference, 3000, 3000).lambda[0] ?? 0,
    );
  });

  it('starts at the next multiple of 100 rpm', () => {
    const reference = tableSet();
    expect(fakeDynoResult(reference, reference, 1050, 1300).rpm).toEqual([1100, 1200, 1300]);
  });
});
