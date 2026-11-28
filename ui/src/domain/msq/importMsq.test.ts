// @vitest-environment jsdom
// Node has no DOMParser; jsdom provides the same API the browser uses (ADR 0012).
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import type { TableData, TableId } from '../../protocol/messages.ts';
import { FakeSimSource } from '../../sim/fake/FakeSimSource.ts';
import { referenceTables } from '../../sim/fake/referenceData.ts';
import { SimStore } from '../../state/simStore.ts';
import { cellValue } from '../table2d.ts';
import { parseTuneFile, tuneChanges } from '../tuneFile.ts';
import alphaN from './fixtures/alpha-n-load.msq?raw';
import rusefiStyle from './fixtures/rusefi-style-lambda.msq?raw';
import speeduinoStyle from './fixtures/speeduino-style-4x4.msq?raw';
import { importMsq } from './importMsq.ts';
import { constant, msqXml } from './msqTestXml.ts';
import { MsqError } from './parseMsq.ts';

const ENGINE = 'i6_30_na';
const savedAt = new Date(2026, 10, 28, 12, 0);
const current: ReadonlyMap<TableId, TableData> = new Map(
  referenceTables().map((table) => [table.id, table]),
);

function ours(id: TableId): TableData {
  const table = current.get(id);
  if (table === undefined) {
    throw new Error(`reference table ${id} missing`);
  }
  return table;
}

/** Value of the imported table at our breakpoints (rpm, kPa). */
function importedAt(
  result: ReturnType<typeof importMsq>,
  id: TableId,
  rpm: number,
  map_kPa: number,
): number | undefined {
  const table = ours(id);
  const x = table.xAxis.bins.indexOf(rpm);
  const y = table.yAxis.bins.indexOf(map_kPa);
  return result.tune.tables[id]?.values[y]?.[x];
}

describe('importMsq with a Speeduino-style 4×4 file', () => {
  const result = importMsq(speeduinoStyle, ENGINE, current, savedAt);

  it('builds a tune for the current engine with our axes', () => {
    expect(result.tune.engine).toBe(ENGINE);
    expect(Object.keys(result.tune.tables)).toEqual(['ve', 'ignition', 'lambdaTarget']);
    expect(result.tune.tables.ve?.xBins).toEqual(ours('ve').xAxis.bins);
    expect(result.tune.tables.ve?.yBins).toEqual(ours('ve').yAxis.bins);
    expect(result.tune.tables.ve?.values).toHaveLength(16);
    expect(result.tune.tables.ve?.values[0]).toHaveLength(16);
    expect(result.tune.overrun).toBeUndefined();
  });

  it('resamples bilinearly between the file breakpoints', () => {
    // 2200 rpm is 60 % from 1000 to 3000, 40 kPa halfway from 30 to 50.
    expect(importedAt(result, 've', 2200, 40)).toBeCloseTo(53.5, 9);
    expect(importedAt(result, 'ignition', 2200, 40)).toBeCloseTo(31.8, 9);
    expect(importedAt(result, 've', 3000, 50)).toBe(65); // on a file breakpoint
  });

  it('holds the edge values outside the file axes', () => {
    expect(importedAt(result, 've', 800, 20)).toBe(40);
    expect(importedAt(result, 've', 7500, 105)).toBe(100);
  });

  it('converts AFR to lambda and rounds to three decimals', () => {
    expect(importedAt(result, 'lambdaTarget', 800, 20)).toBe(1);
    expect(importedAt(result, 'lambdaTarget', 5000, 80)).toBe(0.898); // 13.2 / 14.7
    expect(importedAt(result, 'lambdaTarget', 7500, 105)).toBe(0.85); // 12.5 / 14.7
  });

  it('warns once per resampled table, mentioning the clamping', () => {
    expect(result.warnings).toHaveLength(3);
    expect(result.warnings[0]).toMatch(/^ve: file axes 4×4 \(1000–7000 rpm, 30–100 kPa\)/);
    expect(result.warnings[0]).toMatch(/resampled bilinearly; outside the file range/);
  });

  it('is accepted by the normal tune-loading checks', () => {
    const roundTrip = parseTuneFile(JSON.stringify(result.tune));
    const changes = tuneChanges(roundTrip, ENGINE, current);
    expect(changes.map((change) => change.tableId)).toEqual(['ve', 'ignition', 'lambdaTarget']);
  });
});

describe('importMsq with a rusEFI-style file', () => {
  const result = importMsq(rusefiStyle, ENGINE, current, savedAt);

  it('uses the alternative constant names and lambda units', () => {
    expect(Object.keys(result.tune.tables)).toEqual(['ve', 'lambdaTarget']);
    expect(importedAt(result, 'lambdaTarget', 7500, 105)).toBe(0.85);
    expect(importedAt(result, 've', 4000, 105)).toBe(90);
  });

  it('reports the missing ignition table and leaves it out', () => {
    expect(result.warnings).toContain(
      'ignition: not in the file (advTable1 or ignitionTable), unchanged',
    );
  });
});

describe('importMsq with axes equal to ours', () => {
  const ve = ours('ve');
  const shifted = ve.values.map((row) => row.map((value) => value + 1));
  const xml = msqXml([
    constant('veTable', '%', shifted),
    constant('rpmBins', 'RPM', ve.xAxis.bins),
    constant('fuelLoadBins', 'kPa', ve.yAxis.bins),
  ]);

  it('takes the values over exactly and does not warn about resampling', () => {
    const result = importMsq(xml, ENGINE, current, savedAt);
    expect(result.tune.tables.ve?.values).toEqual(shifted);
    expect(result.warnings.some((warning) => warning.includes('resampled'))).toBe(false);
  });
});

describe('importMsq errors', () => {
  const rpm = constant('rpmBins', 'RPM', [1000, 7000]);
  const load = constant('fuelLoadBins', 'kPa', [30, 100]);
  const ve2x2 = constant('veTable', '%', [
    [40, 60],
    [80, 100],
  ]);
  const run =
    (xml: string): (() => void) =>
    () =>
      importMsq(xml, ENGINE, current, savedAt);

  it('rejects a load axis that is not MAP (alpha-N)', () => {
    expect(run(alphaN)).toThrow("ve: 'fuelLoadBins' is in 'TPS', expected one of: kpa");
  });

  it('rejects a file without any supported table', () => {
    expect(run(msqXml([constant('reqFuel', 'ms', [15.5])]))).toThrow(
      /no supported table found \(ve: not in the file/,
    );
  });

  it('throws MsqError so the UI can show the reason', () => {
    expect(run(alphaN)).toThrow(MsqError);
  });

  it('rejects a missing axis constant', () => {
    expect(run(msqXml([ve2x2, rpm]))).toThrow("ve: constant 'fuelLoadBins' is missing");
  });

  it('rejects a table whose size does not match its axes', () => {
    const threeBins = constant('rpmBins', 'RPM', [1000, 4000, 7000]);
    expect(run(msqXml([ve2x2, threeBins, load]))).toThrow(
      "ve: 'veTable' is 2×2, but its axes are 2×3",
    );
  });

  it('rejects a value count that does not match rows × cols', () => {
    const short = { ...ve2x2, content: '40 60 80' };
    expect(run(msqXml([short, rpm, load]))).toThrow(
      "ve: constant 'veTable' has 3 values, expected 2×2",
    );
  });

  it('rejects axes that are not strictly increasing', () => {
    const falling = constant('rpmBins', 'RPM', [7000, 1000]);
    expect(run(msqXml([ve2x2, falling, load]))).toThrow(/'rpmBins' is not strictly increasing/);
  });

  it('rejects non-numeric content and missing units', () => {
    const words = { ...ve2x2, content: '"a" "b" "c" "d"' };
    expect(run(msqXml([words, rpm, load]))).toThrow(/does not contain numbers/);
    const noUnits = { ...ve2x2, units: '' };
    expect(run(msqXml([noUnits, rpm, load]))).toThrow(/'veTable' is in '', expected one of: %/);
  });

  it('rejects a two-dimensional axis', () => {
    const square = constant('rpmBins', 'RPM', [
      [1000, 2000],
      [3000, 4000],
    ]);
    expect(run(msqXml([ve2x2, square, load]))).toThrow(/axis 'rpmBins' is 2×2, not a list/);
  });

  it('skips tables the simulation does not have', () => {
    const withoutVe = new Map([...current].filter(([id]) => id !== 've'));
    expect(() => importMsq(msqXml([ve2x2, rpm, load]), ENGINE, withoutVe, savedAt)).toThrow(
      /ve: the simulation has no editable table/,
    );
  });
});

describe('importMsq stoich check', () => {
  const afr = (stoich: number): string =>
    msqXml([
      constant('stoich', 'AFR', [stoich]),
      constant('afrTable', 'AFR', [
        [14.7, 14.7],
        [13, 12.5],
      ]),
      constant('rpmBinsAFR', 'RPM', [1000, 7000]),
      constant('loadBinsAFR', 'kPa', [30, 100]),
    ]);

  it('warns when the file is tuned for another fuel', () => {
    const result = importMsq(afr(14.1), ENGINE, current, savedAt);
    expect(result.warnings).toContain(
      "lambdaTarget: the file's stoich is 14.1, AFR was converted with 14.7 (gasoline)",
    );
  });

  it('stays quiet for gasoline', () => {
    const result = importMsq(afr(14.7), ENGINE, current, savedAt);
    expect(result.warnings.some((warning) => warning.includes('stoich'))).toBe(false);
  });
});

describe('importMsq through the store', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('loads as one undo step like a tune file', () => {
    const store = new SimStore(new FakeSimSource({ seed: 3 }));
    store.start();
    vi.advanceTimersByTime(1); // hello
    vi.advanceTimersByTime(1); // getTable answers
    const snapshot = store.getSnapshot();
    const before = snapshot.tables.get('ve');
    const engine = snapshot.hello?.engine.id ?? '';
    const { tune } = importMsq(speeduinoStyle, engine, snapshot.tables, savedAt);

    const changed = store.importTune(JSON.stringify(tune));
    expect(changed).toBeGreaterThan(0);
    const after = store.getSnapshot().tables.get('ve');
    expect(after && cellValue(after, 0, 0)).toBe(40);

    store.undo();
    expect(store.getSnapshot().tables.get('ve')?.values).toEqual(before?.values);
  });
});
