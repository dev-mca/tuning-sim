import { describe, expect, it } from 'vitest';

import type { TableData } from '../protocol/messages.ts';
import { describeDifferences, tuneDifferences } from './tuneSummary.ts';

function table(id: string, values: number[][]): TableData {
  return {
    id,
    name: id,
    unit: 'deg BTDC',
    editable: true,
    xAxis: { name: 'rpm', unit: '1/min', bins: [1, 2] },
    yAxis: { name: 'map', unit: 'kPa', bins: [1] },
    min: -10,
    max: 50,
    values,
  };
}

describe('tuneDifferences', () => {
  it('counts changed cells and their mean change', () => {
    const originals = new Map([['ignition', table('ignition', [[20, 30]])]]);
    const tables = new Map([['ignition', table('ignition', [[17, 28]])]]);
    expect(tuneDifferences(tables, originals)).toEqual([
      { tableId: 'ignition', unit: 'deg BTDC', changedCells: 2, meanDelta: -2.5 },
    ]);
  });

  it('skips unchanged tables', () => {
    const originals = new Map([['ignition', table('ignition', [[20, 30]])]]);
    expect(tuneDifferences(originals, originals)).toEqual([]);
  });
});

describe('describeDifferences', () => {
  it('describes the changes in one line', () => {
    expect(
      describeDifferences([
        { tableId: 'ignition', unit: 'deg BTDC', changedCells: 12, meanDelta: -2.5 },
        { tableId: 've', unit: '%', changedCells: 1, meanDelta: 3 },
      ]),
    ).toBe('ignition: 12 cells, mean −2.5 deg BTDC · ve: 1 cell, mean +3.0 %');
  });

  it('names the original tune when nothing changed', () => {
    expect(describeDifferences([])).toBe('original tune');
  });
});
