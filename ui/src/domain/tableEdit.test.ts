import { describe, expect, it } from 'vitest';

import type { TableData } from '../protocol/messages.ts';
import { selectAll, singleCell } from './selection.ts';
import {
  adjustCells,
  applyCells,
  interpolateCorners,
  readCells,
  setCells,
  smoothCells,
} from './tableEdit.ts';

function makeTable(values: number[][], id = 've'): TableData {
  const rows = values.length;
  const columns = values[0]?.length ?? 0;
  return {
    id,
    name: 'Test',
    unit: '%',
    editable: true,
    xAxis: { name: 'rpm', unit: '1/min', bins: Array.from({ length: columns }, (_, i) => i) },
    yAxis: { name: 'map', unit: 'kPa', bins: Array.from({ length: rows }, (_, i) => i) },
    min: 0,
    max: 100,
    values,
  };
}

const grid = makeTable([
  [10, 20, 30],
  [40, 50, 60],
  [70, 80, 90],
]);

describe('adjustCells', () => {
  it('adds the delta to every selected cell', () => {
    const selection = { anchor: { x: 0, y: 0 }, focus: { x: 1, y: 0 } };
    expect(adjustCells(grid, selection, 0.5)).toEqual([
      { x: 0, y: 0, value: 10.5 },
      { x: 1, y: 0, value: 20.5 },
    ]);
  });

  it('clamps to the table limits and skips cells that do not change', () => {
    const changes = adjustCells(makeTable([[99, 100]]), selectAll(2, 1), 5);
    expect(changes).toEqual([{ x: 0, y: 0, value: 100 }]);
  });

  it('rounds to the table precision, so repeated steps do not collect float dust', () => {
    const lambda = makeTable([[0.88]], 'lambdaTarget');
    expect(adjustCells(lambda, singleCell({ x: 0, y: 0 }), 0.01)[0]?.value).toBe(0.89);
  });
});

describe('setCells', () => {
  it('sets all selected cells to one value', () => {
    const changes = setCells(grid, { anchor: { x: 2, y: 1 }, focus: { x: 2, y: 2 } }, 55);
    expect(changes).toEqual([
      { x: 2, y: 1, value: 55 },
      { x: 2, y: 2, value: 55 },
    ]);
  });
});

describe('smoothCells', () => {
  it('replaces a spike by the mean of its 3x3 neighbourhood', () => {
    const spike = makeTable([
      [10, 10, 10],
      [10, 100, 10],
      [10, 10, 10],
    ]);
    expect(smoothCells(spike, singleCell({ x: 1, y: 1 }))).toEqual([{ x: 1, y: 1, value: 20 }]);
  });

  it('uses only existing neighbours at the edge', () => {
    expect(smoothCells(grid, singleCell({ x: 0, y: 0 }))[0]?.value).toBe((10 + 20 + 40 + 50) / 4);
  });

  it('reads neighbours from the unchanged table, independent of visiting order', () => {
    const changes = smoothCells(grid, selectAll(3, 3));
    const center = changes.find((cell) => cell.x === 1 && cell.y === 1);
    // A linear field is unchanged in the middle by a symmetric box filter.
    expect(center).toBeUndefined();
  });
});

describe('interpolateCorners', () => {
  it('fills the selection bilinearly from its four corners', () => {
    const corners = makeTable([
      [0, 99, 20],
      [99, 99, 99],
      [40, 99, 60],
    ]);
    const filled = applyCells(corners, interpolateCorners(corners, selectAll(3, 3)));
    expect(filled.values).toEqual([
      [0, 10, 20],
      [20, 30, 40],
      [40, 50, 60],
    ]);
  });

  it('interpolates along one axis when the selection is a single row', () => {
    const row = makeTable([[0, 99, 99, 30]]);
    const filled = applyCells(row, interpolateCorners(row, selectAll(4, 1)));
    expect(filled.values).toEqual([[0, 10, 20, 30]]);
  });
});

describe('applyCells', () => {
  it('returns a new table and leaves the original untouched', () => {
    const changed = applyCells(grid, [{ x: 0, y: 0, value: 11 }]);
    expect(changed.values[0]?.[0]).toBe(11);
    expect(grid.values[0]?.[0]).toBe(10);
  });

  it('rejects cells outside the table', () => {
    expect(() => applyCells(grid, [{ x: 3, y: 0, value: 1 }])).toThrow(RangeError);
  });
});

describe('readCells', () => {
  it('reads the current values for undo', () => {
    expect(readCells(grid, [{ x: 2, y: 2 }])).toEqual([{ x: 2, y: 2, value: 90 }]);
  });
});
