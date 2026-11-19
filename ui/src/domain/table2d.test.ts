import { describe, expect, it } from 'vitest';

import type { TableData } from '../protocol/messages.ts';
import { cellValue, findBin, fractionalCell, interpolate } from './table2d.ts';

/** 3 x 2 table with f(x, y) = x + 10 y at the breakpoints, so bilinear results are easy to check. */
const linearTable: TableData = {
  id: 'test',
  name: 'Test',
  unit: '-',
  editable: true,
  xAxis: { name: 'rpm', unit: '1/min', bins: [1000, 2000, 4000] },
  yAxis: { name: 'map', unit: 'kPa', bins: [20, 100] },
  min: 0,
  max: 10000,
  values: [
    [1000 + 200, 2000 + 200, 4000 + 200],
    [1000 + 1000, 2000 + 1000, 4000 + 1000],
  ],
};

describe('findBin', () => {
  it('finds the lower bin and fraction inside the axis', () => {
    expect(findBin([1000, 2000, 4000], 3000)).toEqual({ index: 1, fraction: 0.5 });
  });

  it('returns fraction 0 exactly on an inner breakpoint', () => {
    expect(findBin([1000, 2000, 4000], 2000)).toEqual({ index: 1, fraction: 0 });
  });

  it('clamps below and above the axis', () => {
    expect(findBin([1000, 2000, 4000], 500)).toEqual({ index: 0, fraction: 0 });
    expect(findBin([1000, 2000, 4000], 9000)).toEqual({ index: 1, fraction: 1 });
  });

  it('handles a single-bin axis', () => {
    expect(findBin([1000], 1500)).toEqual({ index: 0, fraction: 0 });
  });
});

describe('interpolate', () => {
  it('is exact at the breakpoints', () => {
    expect(interpolate(linearTable, 2000, 100)).toBe(3000);
    expect(interpolate(linearTable, 4000, 20)).toBe(4200);
  });

  it('is linear between breakpoints along each axis', () => {
    expect(interpolate(linearTable, 3000, 20)).toBeCloseTo(3200, 9);
    expect(interpolate(linearTable, 1000, 60)).toBeCloseTo(1600, 9);
  });

  it('is bilinear in the middle of a cell', () => {
    expect(interpolate(linearTable, 1500, 60)).toBeCloseTo(2100, 9);
  });

  it('clamps outside the table', () => {
    expect(interpolate(linearTable, 0, 0)).toBe(1200);
    expect(interpolate(linearTable, 99999, 999)).toBe(5000);
  });

  it('is continuous across a bin boundary', () => {
    const left = interpolate(linearTable, 2000 - 1e-6, 50);
    const right = interpolate(linearTable, 2000 + 1e-6, 50);
    expect(Math.abs(left - right)).toBeLessThan(1e-3);
  });
});

describe('fractionalCell', () => {
  it('returns fractional indices for the live cursor', () => {
    expect(fractionalCell(linearTable, 3000, 60)).toEqual({ x: 1.5, y: 0.5 });
  });
});

describe('cellValue', () => {
  it('throws for an index outside the table', () => {
    expect(() => cellValue(linearTable, 3, 0)).toThrow(RangeError);
  });
});
