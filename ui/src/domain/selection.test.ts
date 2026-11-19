import { describe, expect, it } from 'vitest';

import {
  cellsInSelection,
  isSelected,
  moveSelection,
  selectionRect,
  singleCell,
} from './selection.ts';

describe('selection', () => {
  it('normalises anchor and focus into a rectangle', () => {
    expect(selectionRect({ anchor: { x: 3, y: 1 }, focus: { x: 1, y: 2 } })).toEqual({
      xMin: 1,
      xMax: 3,
      yMin: 1,
      yMax: 2,
    });
  });

  it('lists the cells row by row, starting at the lowest corner', () => {
    expect(cellsInSelection({ anchor: { x: 1, y: 1 }, focus: { x: 0, y: 0 } })).toEqual([
      { x: 0, y: 0 },
      { x: 1, y: 0 },
      { x: 0, y: 1 },
      { x: 1, y: 1 },
    ]);
  });

  it('knows whether a cell is selected', () => {
    const selection = { anchor: { x: 1, y: 1 }, focus: { x: 2, y: 2 } };
    expect(isSelected(selection, { x: 2, y: 1 })).toBe(true);
    expect(isSelected(selection, { x: 0, y: 1 })).toBe(false);
  });

  it('collapses to one cell when moving without Shift', () => {
    const selection = { anchor: { x: 0, y: 0 }, focus: { x: 2, y: 2 } };
    expect(moveSelection(selection, 1, 0, false, 4, 4)).toEqual(singleCell({ x: 3, y: 2 }));
  });

  it('keeps the anchor and moves the focus with Shift', () => {
    const selection = singleCell({ x: 1, y: 1 });
    expect(moveSelection(selection, 0, 1, true, 4, 4)).toEqual({
      anchor: { x: 1, y: 1 },
      focus: { x: 1, y: 2 },
    });
  });

  it('stays inside the table', () => {
    expect(moveSelection(singleCell({ x: 0, y: 3 }), -1, 1, false, 4, 4)).toEqual(
      singleCell({ x: 0, y: 3 }),
    );
  });
});
