import { describe, expect, it } from 'vitest';

import type { TableData, TableId } from '../protocol/messages.ts';
import {
  createTuneFile,
  differingCells,
  parseTuneFile,
  TuneFileError,
  tuneChanges,
  tuneFileName,
} from './tuneFile.ts';

function table(id: string, values: number[][], editable = true): TableData {
  return {
    id,
    name: id,
    unit: '%',
    editable,
    xAxis: { name: 'rpm', unit: '1/min', bins: [1000, 2000] },
    yAxis: { name: 'map', unit: 'kPa', bins: [30, 100] },
    min: 0,
    max: 100,
    values,
  };
}

const ve = table('ve', [
  [50, 60],
  [80, 90],
]);
const mbt = table(
  'ignitionMbt',
  [
    [10, 20],
    [30, 40],
  ],
  false,
);
const current: ReadonlyMap<TableId, TableData> = new Map([
  ['ve', ve],
  ['ignitionMbt', mbt],
]);
const savedAt = new Date(2026, 10, 19, 21, 5);

describe('createTuneFile', () => {
  it('stores the editable tables with their axes and skips read-only ones', () => {
    const tune = createTuneFile('i6', [ve, mbt], savedAt);
    expect(Object.keys(tune.tables)).toEqual(['ve']);
    expect(tune.tables.ve?.xBins).toEqual([1000, 2000]);
    expect(tune.engine).toBe('i6');
  });

  it('survives a JSON round trip', () => {
    const tune = createTuneFile('i6', [ve], savedAt);
    expect(parseTuneFile(JSON.stringify(tune))).toEqual(tune);
  });
});

describe('tuneFileName', () => {
  it('builds a file name without colons', () => {
    expect(tuneFileName('i6_30_na', savedAt)).toBe('i6_30_na-tune-2026-11-19-2105.json');
  });
});

describe('parseTuneFile', () => {
  it.each([
    ['invalid JSON', 'nope', /not valid JSON/],
    ['another format', '{"format":"msq"}', /not a tuning-sim tune/],
    ['a newer version', '{"format":"tuning-sim-tune","version":2}', /version 2/],
    [
      'a ragged table',
      JSON.stringify({
        format: 'tuning-sim-tune',
        version: 1,
        engine: 'i6',
        savedAt: 'x',
        tables: { ve: { unit: '%', xBins: [1, 2], yBins: [1], values: [[1]] } },
      }),
      /'ve' is malformed/,
    ],
  ])('rejects %s', (_, text, message) => {
    expect(() => parseTuneFile(text)).toThrow(message);
  });
});

describe('tuneChanges', () => {
  it('returns only the cells that differ', () => {
    const tune = createTuneFile(
      'i6',
      [
        table('ve', [
          [50, 61],
          [80, 90],
        ]),
      ],
      savedAt,
    );
    expect(tuneChanges(tune, 'i6', current)).toEqual([
      { tableId: 've', cells: [{ x: 1, y: 0, value: 61 }] },
    ]);
  });

  it('returns nothing for an identical tune', () => {
    expect(tuneChanges(createTuneFile('i6', [ve], savedAt), 'i6', current)).toEqual([]);
  });

  it('rejects a tune for another engine', () => {
    const tune = createTuneFile('v8', [ve], savedAt);
    expect(() => tuneChanges(tune, 'i6', current)).toThrow(/engine 'v8'/);
  });

  it('rejects unknown and read-only tables', () => {
    const unknown = createTuneFile(
      'i6',
      [
        table('boost', [
          [1, 1],
          [1, 1],
        ]),
      ],
      savedAt,
    );
    expect(() => tuneChanges(unknown, 'i6', current)).toThrow(/no table 'boost'/);
    const tune = createTuneFile('i6', [ve], savedAt);
    tune.tables.ignitionMbt = tune.tables.ve ?? { unit: '', xBins: [], yBins: [], values: [] };
    expect(() => tuneChanges(tune, 'i6', current)).toThrow(/read-only/);
  });

  it('rejects different axes instead of shifting values', () => {
    const tune = createTuneFile('i6', [ve], savedAt);
    const saved = tune.tables.ve;
    if (saved === undefined) throw new Error('missing');
    saved.xBins = [1000, 2500];
    expect(() => tuneChanges(tune, 'i6', current)).toThrow(TuneFileError);
  });

  it('accepts axes that differ only by floating-point noise (simcore vs fake)', () => {
    const tune = createTuneFile('i6', [ve], savedAt);
    const saved = tune.tables.ve;
    if (saved === undefined) throw new Error('missing');
    saved.xBins = [1000.0000000000002, 2000];
    expect(tuneChanges(tune, 'i6', current)).toEqual([]);
  });

  it('rejects values outside the table limits', () => {
    const tune = createTuneFile(
      'i6',
      [
        table('ve', [
          [50, 160],
          [80, 90],
        ]),
      ],
      savedAt,
    );
    expect(() => tuneChanges(tune, 'i6', current)).toThrow(/outside 0 … 100/);
  });
});

describe('differingCells', () => {
  it('lists the cells to restore an original table', () => {
    const edited = table('ve', [
      [51, 60],
      [80, 90],
    ]);
    expect(differingCells(ve, edited)).toEqual([{ x: 0, y: 0, value: 50 }]);
  });
});

describe('overrun settings in tune files', () => {
  it('are optional, round-trip and are validated', () => {
    expect(createTuneFile('i6', [ve], savedAt).overrun).toBeUndefined();
    const pops = { fuelCut: false, retard_deg: 35, enrichment_pct: 10 };
    const tune = createTuneFile('i6', [ve], savedAt, pops);
    expect(parseTuneFile(JSON.stringify(tune)).overrun).toEqual(pops);
    const broken = { ...tune, overrun: { fuelCut: 'no', retard_deg: 1, enrichment_pct: 0 } };
    expect(() => parseTuneFile(JSON.stringify(broken))).toThrow(/overrun settings/);
  });
});

describe('launch settings in tune files', () => {
  it('are optional, round-trip and are validated', () => {
    expect(createTuneFile('i6', [ve], savedAt).launch).toBeUndefined();
    const twoStep = { enabled: true, launch_rpm: 4000, retard_deg: 10 };
    const tune = createTuneFile('i6', [ve], savedAt, undefined, twoStep);
    expect(parseTuneFile(JSON.stringify(tune)).launch).toEqual(twoStep);
    const broken = { ...tune, launch: { enabled: 'yes', launch_rpm: 4000, retard_deg: 10 } };
    expect(() => parseTuneFile(JSON.stringify(broken))).toThrow(/launch settings/);
  });
});
