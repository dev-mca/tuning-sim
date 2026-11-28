import { describe, expect, it } from 'vitest';

import ignitionTable from '../../../../shared/tables/i6_30_na/ignition.json';
import type { TableData } from '../../protocol/messages.ts';
import { HEATMAP_RAMP } from '../theme/palette.ts';
import {
  axisLabelBins,
  axisTitle,
  DEFAULT_BOX,
  gridLineIndices,
  gridTriangles,
  linearScale,
  niceTicks,
  operatingPoint,
  srgbHexToLinear,
  surfaceGeometry,
} from './surfaceModel.ts';

/** 3 x 2 table: low MAP row 0..2, high MAP row 10..12, uneven rpm breakpoints. */
const smallTable: TableData = {
  id: 'test',
  name: 'Test',
  unit: '%',
  editable: true,
  xAxis: { name: 'rpm', unit: '1/min', bins: [1000, 2000, 5000] },
  yAxis: { name: 'map', unit: 'kPa', bins: [20, 100] },
  min: 0,
  max: 100,
  values: [
    [0, 1, 2],
    [10, 11, 12],
  ],
};

const box = { width: 8, depth: 4, height: 2 };

function vertex(positions: Float32Array, index: number): number[] {
  return Array.from(positions.subarray(index * 3, index * 3 + 3));
}

describe('linearScale', () => {
  it('maps the domain ends onto the range ends, also reversed', () => {
    const scale = linearScale(20, 100, 2, -2);
    expect(scale(20)).toBe(2);
    expect(scale(100)).toBe(-2);
    expect(scale(60)).toBe(0);
  });

  it('puts a flat domain in the middle of the range instead of dividing by zero', () => {
    expect(linearScale(5, 5, 0, 4)(5)).toBe(2);
  });
});

describe('surfaceGeometry', () => {
  const geometry = surfaceGeometry(smallTable, box);

  it('has one vertex per cell and two triangles per grid square', () => {
    expect(geometry.positions).toHaveLength(6 * 3);
    expect(geometry.colors).toHaveLength(6 * 3);
    expect(geometry.indices).toHaveLength(2 * 6);
  });

  it('places rpm on X by axis value, MAP on Z (high MAP at the back) and value on Y', () => {
    expect(vertex(geometry.positions, 0)).toEqual([-4, 0, 2]); // 1000 rpm, 20 kPa, min value
    expect(vertex(geometry.positions, 5)).toEqual([4, 2, -2]); // 5000 rpm, 100 kPa, max value
    // 2000 rpm is a quarter of the way from 1000 to 5000, not halfway (uneven breakpoints).
    expect(vertex(geometry.positions, 1)[0]).toBeCloseTo(-2);
  });

  it('colours the lowest and highest cell with the ends of the heatmap ramp', () => {
    const toFloat32 = (rgb: number[]) => rgb.map((c) => Math.fround(c));
    expect(Array.from(geometry.colors.subarray(0, 3))).toEqual(
      toFloat32(srgbHexToLinear(HEATMAP_RAMP[0])),
    );
    expect(Array.from(geometry.colors.subarray(15, 18))).toEqual(
      toFloat32(srgbHexToLinear(HEATMAP_RAMP[HEATMAP_RAMP.length - 1] ?? '#000000')),
    );
  });

  it('keeps a flat table in the middle of the box height', () => {
    const flat = { ...smallTable, values: smallTable.values.map((row) => row.map(() => 7)) };
    const positions = surfaceGeometry(flat, box).positions;
    for (let i = 1; i < positions.length; i += 3) {
      expect(positions[i]).toBe(1);
    }
  });

  it('builds the real 16 x 16 ignition table inside the default box', () => {
    const real = surfaceGeometry(ignitionTable as TableData);
    expect(real.positions).toHaveLength(256 * 3);
    expect(real.indices).toHaveLength(15 * 15 * 6);
    for (let i = 0; i < real.positions.length; i += 3) {
      expect(Math.abs(real.positions[i] ?? NaN)).toBeLessThanOrEqual(DEFAULT_BOX.width / 2);
      expect(real.positions[i + 1]).toBeGreaterThanOrEqual(0);
      expect(real.positions[i + 1]).toBeLessThanOrEqual(DEFAULT_BOX.height);
      expect(Math.abs(real.positions[i + 2] ?? NaN)).toBeLessThanOrEqual(DEFAULT_BOX.depth / 2);
    }
    expect(Math.max(...real.indices)).toBe(255);
  });
});

describe('gridTriangles', () => {
  it('winds counter-clockwise seen from above (normal points to +Y)', () => {
    const { positions } = surfaceGeometry(smallTable, box);
    const [a = 0, b = 0, c = 0] = gridTriangles(3, 2);
    const [ax = 0, , az = 0] = vertex(positions, a);
    const [bx = 0, , bz = 0] = vertex(positions, b);
    const [cx = 0, , cz = 0] = vertex(positions, c);
    // Y component of (b - a) x (c - a).
    const normalY = (bz - az) * (cx - ax) - (bx - ax) * (cz - az);
    expect(normalY).toBeGreaterThan(0);
  });

  it('is empty for a table with a single row or column', () => {
    expect(gridTriangles(1, 5)).toHaveLength(0);
    expect(gridTriangles(5, 1)).toHaveLength(0);
  });
});

describe('gridLineIndices', () => {
  it('connects horizontal and vertical neighbours once each', () => {
    // 3 x 2: 2 segments per row * 2 rows + 1 segment per column * 3 columns = 7 segments.
    expect(gridLineIndices(3, 2)).toHaveLength(7 * 2);
    expect(Array.from(gridLineIndices(2, 2))).toEqual([0, 1, 0, 2, 1, 3, 2, 3]);
  });
});

describe('operatingPoint', () => {
  it('lies on the interpolated surface', () => {
    // 3000 rpm is halfway between 1000 and 5000 in value, a third into the 2000..5000 cell.
    const [x, y, z] = operatingPoint(smallTable, 3000, 60, box);
    expect(x).toBeCloseTo(0);
    expect(z).toBeCloseTo(0);
    // Value at (3000, 60) = 1 + 1/3 + 5 = 6.333..., of range 0..12 at height 2.
    expect(y).toBeCloseTo(((1 + 1 / 3 + 5) / 12) * 2);
  });

  it('clamps to the table edges like the ECU lookup', () => {
    expect(operatingPoint(smallTable, 200, 5, box)).toEqual([-4, 0, 2]);
    expect(operatingPoint(smallTable, 9000, 250, box)).toEqual([4, 2, -2]);
  });
});

describe('axisLabelBins', () => {
  const rpmBins = ignitionTable.xAxis.bins;

  it('keeps all bins when they fit', () => {
    expect(axisLabelBins([1, 2, 3], 8)).toEqual([1, 2, 3]);
  });

  it('thins 16 rpm bins to at most the limit, keeping first and last', () => {
    const labels = axisLabelBins(rpmBins, 8);
    expect(labels.length).toBeLessThanOrEqual(8);
    expect(labels[0]).toBe(800);
    expect(labels.at(-1)).toBe(7500);
  });

  it('drops a label that would crowd the last one', () => {
    // Step 3 over 0..10 picks 0, 3, 6, 9; 9 is one bin from 10, so 10 replaces it.
    expect(axisLabelBins([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 5)).toEqual([0, 3, 6, 10]);
  });
});

describe('niceTicks', () => {
  it('uses 1/2/5 steps inside the range', () => {
    expect(niceTicks(2.3, 38.7, 5)).toEqual([10, 20, 30]);
    expect(niceTicks(0.8, 1.02, 5)).toEqual([0.8, 0.85, 0.9, 0.95, 1]);
  });

  it('includes a tick exactly at the range end and survives degenerate input', () => {
    expect(niceTicks(0, 1, 2)).toEqual([0, 0.5, 1]);
    expect(niceTicks(4, 4, 5)).toEqual([4]);
    expect(niceTicks(Number.NaN, 1, 5)).toEqual([]);
  });
});

describe('axisTitle', () => {
  it('appends the unit in brackets', () => {
    expect(axisTitle('rpm', '1/min')).toBe('rpm [1/min]');
    expect(axisTitle('lambda', '')).toBe('lambda');
  });
});

describe('srgbHexToLinear', () => {
  it('maps black, white and mid grey through the sRGB curve', () => {
    expect(srgbHexToLinear('#000000')).toEqual([0, 0, 0]);
    expect(srgbHexToLinear('#ffffff')).toEqual([1, 1, 1]);
    // sRGB 50 % grey is about 21 % linear light.
    expect(srgbHexToLinear('#808080')[0]).toBeCloseTo(0.2159, 4);
  });
});
