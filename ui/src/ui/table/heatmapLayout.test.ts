import { describe, expect, it } from 'vitest';

import {
  cellAtPixel,
  cellRect,
  computeLayout,
  fractionalToPixel,
  nearestCellAtPixel,
} from './heatmapLayout.ts';

// 16 columns of 40 px plus the 40 px axis: cell width 40, height 40 * 0.72 = 28.8.
const layout = computeLayout(40 + 16 * 40, 16, 16);

describe('heatmap layout', () => {
  it('sizes cells from the available width', () => {
    expect(layout.cellWidth).toBe(40);
    expect(layout.cellHeight).toBeCloseTo(28.8, 9);
    expect(layout.height).toBeCloseTo(16 * 28.8 + layout.axisBottom, 9);
  });

  it('limits the cell height on very wide and very narrow canvases', () => {
    expect(computeLayout(4000, 16, 16).cellHeight).toBe(34);
    expect(computeLayout(200, 16, 16).cellHeight).toBe(20);
  });

  it('draws row 0 (lowest MAP) at the bottom', () => {
    expect(cellRect(layout, { x: 0, y: 0 }).y).toBeCloseTo(15 * 28.8, 9);
    expect(cellRect(layout, { x: 0, y: 15 }).y).toBe(0);
  });

  it('maps pixels back to the same cell', () => {
    const rect = cellRect(layout, { x: 3, y: 10 });
    expect(cellAtPixel(layout, rect.x + 1, rect.y + 1)).toEqual({ x: 3, y: 10 });
  });

  it('returns null on the axes and outside', () => {
    expect(cellAtPixel(layout, 10, 10)).toBeNull();
    expect(cellAtPixel(layout, 100, layout.height - 5)).toBeNull();
  });

  it('clamps to the nearest cell while dragging outside', () => {
    expect(nearestCellAtPixel(layout, -50, -50)).toEqual({ x: 0, y: 15 });
    expect(nearestCellAtPixel(layout, 9999, 9999)).toEqual({ x: 15, y: 0 });
  });

  it('puts fractional position 0 in the centre of the first cell', () => {
    const point = fractionalToPixel(layout, { x: 0, y: 0 });
    const rect = cellRect(layout, { x: 0, y: 0 });
    expect(point.x).toBeCloseTo(rect.x + rect.width / 2, 9);
    expect(point.y).toBeCloseTo(rect.y + rect.height / 2, 9);
  });
});
