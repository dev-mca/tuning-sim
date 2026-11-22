import type { CellValue, OverrunSettings, TableData, TableId } from '../protocol/messages.ts';

/**
 * Tune file: the editable ECU tables of one engine as JSON (ADR 0006). The axes are stored
 * too, so a tune made for a different table layout is rejected instead of silently shifted.
 */
export const TUNE_FORMAT = 'tuning-sim-tune';
export const TUNE_FORMAT_VERSION = 1;

export interface TuneTable {
  unit: string;
  xBins: number[];
  yBins: number[];
  /** values[y][x], like TableData. */
  values: number[][];
}

export interface TuneFile {
  format: typeof TUNE_FORMAT;
  version: typeof TUNE_FORMAT_VERSION;
  engine: string;
  savedAt: string;
  tables: Record<TableId, TuneTable>;
  /** ECU overrun (pops & bangs) settings; optional, older tune files have none. */
  overrun?: OverrunSettings;
}

/** A tune file that cannot be used; the message is shown to the user. */
export class TuneFileError extends Error {
  override readonly name = 'TuneFileError';
}

/** Builds a tune file from the editable tables; read-only tables (engine properties) are skipped. */
export function createTuneFile(
  engineId: string,
  tables: Iterable<TableData>,
  savedAt: Date,
  overrun?: OverrunSettings,
): TuneFile {
  const entries = [...tables]
    .filter((table) => table.editable)
    .map((table): [TableId, TuneTable] => [
      table.id,
      {
        unit: table.unit,
        xBins: [...table.xAxis.bins],
        yBins: [...table.yAxis.bins],
        values: table.values.map((row) => [...row]),
      },
    ]);
  return {
    format: TUNE_FORMAT,
    version: TUNE_FORMAT_VERSION,
    engine: engineId,
    savedAt: savedAt.toISOString(),
    tables: Object.fromEntries(entries),
    ...(overrun === undefined ? {} : { overrun: { ...overrun } }),
  };
}

/** File name like "i6_30_na-tune-2026-11-19-2130.json" (no characters that are invalid on Windows). */
export function tuneFileName(engineId: string, savedAt: Date): string {
  const pad = (value: number): string => String(value).padStart(2, '0');
  const date = `${String(savedAt.getFullYear())}-${pad(savedAt.getMonth() + 1)}-${pad(savedAt.getDate())}`;
  const time = `${pad(savedAt.getHours())}${pad(savedAt.getMinutes())}`;
  return `${engineId}-tune-${date}-${time}.json`;
}

/** Parses and structurally validates a tune file; throws TuneFileError with a readable reason. */
export function parseTuneFile(text: string): TuneFile {
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    throw new TuneFileError('the file is not valid JSON');
  }
  if (!isRecord(data) || data.format !== TUNE_FORMAT) {
    throw new TuneFileError('this is not a tuning-sim tune file');
  }
  if (data.version !== TUNE_FORMAT_VERSION) {
    throw new TuneFileError(`unsupported tune file version ${String(data.version)}`);
  }
  if (typeof data.engine !== 'string' || typeof data.savedAt !== 'string') {
    throw new TuneFileError('engine or savedAt is missing');
  }
  if (!isRecord(data.tables)) {
    throw new TuneFileError('tables are missing');
  }
  for (const [id, table] of Object.entries(data.tables)) {
    if (!isTuneTable(table)) {
      throw new TuneFileError(`table '${id}' is malformed`);
    }
  }
  if (data.overrun !== undefined && !isOverrunSettings(data.overrun)) {
    throw new TuneFileError('overrun settings are malformed');
  }
  return data as unknown as TuneFile;
}

export interface TableCells {
  tableId: TableId;
  cells: CellValue[];
}

/**
 * Compares a tune with the current tables and returns the cells that differ, per table.
 * All checks happen first, so either the whole tune is applicable or nothing is changed.
 */
export function tuneChanges(
  tune: TuneFile,
  engineId: string,
  current: ReadonlyMap<TableId, TableData>,
): TableCells[] {
  if (tune.engine !== engineId) {
    throw new TuneFileError(`the tune is for engine '${tune.engine}', not '${engineId}'`);
  }
  const result: TableCells[] = [];
  for (const [id, saved] of Object.entries(tune.tables)) {
    const table = current.get(id);
    if (table === undefined) {
      throw new TuneFileError(`the simulation has no table '${id}'`);
    }
    if (!table.editable) {
      throw new TuneFileError(`table '${id}' is read-only`);
    }
    if (
      !sameNumbers(saved.xBins, table.xAxis.bins) ||
      !sameNumbers(saved.yBins, table.yAxis.bins)
    ) {
      throw new TuneFileError(`table '${id}' has different axes than the simulation`);
    }
    result.push({ tableId: id, cells: changedCells(saved, table) });
  }
  return result.filter((change) => change.cells.length > 0);
}

/** Cells in which `target` differs from `table`; used for loading a tune and for "revert". */
export function differingCells(target: TableData, table: TableData): CellValue[] {
  return changedCells(
    {
      unit: target.unit,
      xBins: target.xAxis.bins,
      yBins: target.yAxis.bins,
      values: target.values,
    },
    table,
  );
}

function changedCells(saved: TuneTable, table: TableData): CellValue[] {
  const cells: CellValue[] = [];
  saved.values.forEach((row, y) => {
    row.forEach((value, x) => {
      if (value < table.min || value > table.max) {
        throw new TuneFileError(
          `table '${table.id}': ${String(value)} at (${String(x)}, ${String(y)}) is outside ${String(table.min)} … ${String(table.max)}`,
        );
      }
      if (table.values[y]?.[x] !== value) {
        cells.push({ x, y, value });
      }
    });
  });
  return cells;
}

function isTuneTable(value: unknown): value is TuneTable {
  if (!isRecord(value) || typeof value.unit !== 'string') {
    return false;
  }
  const { xBins, yBins, values } = value;
  return (
    isNumberArray(xBins) &&
    isNumberArray(yBins) &&
    Array.isArray(values) &&
    values.length === yBins.length &&
    values.every((row) => isNumberArray(row) && row.length === xBins.length)
  );
}

function isOverrunSettings(value: unknown): value is OverrunSettings {
  return (
    isRecord(value) &&
    typeof value.fuelCut === 'boolean' &&
    typeof value.retard_deg === 'number' &&
    Number.isFinite(value.retard_deg) &&
    typeof value.enrichment_pct === 'number' &&
    Number.isFinite(value.enrichment_pct)
  );
}

function isNumberArray(value: unknown): value is number[] {
  return (
    Array.isArray(value) && value.every((item) => typeof item === 'number' && Number.isFinite(item))
  );
}

/**
 * Relative tolerance for axis comparison: simcore's bins carry floating-point noise from the
 * rad/s -> rpm conversion, the fake's do not; both describe the same table.
 */
const AXIS_TOLERANCE = 1e-9;

function sameNumbers(a: readonly number[], b: readonly number[]): boolean {
  return (
    a.length === b.length &&
    a.every((value, index) => {
      const other = b[index];
      return (
        other !== undefined &&
        Math.abs(value - other) <= AXIS_TOLERANCE * Math.max(1, Math.abs(other))
      );
    })
  );
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}
