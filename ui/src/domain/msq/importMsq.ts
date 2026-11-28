import type { TableData, TableId } from '../../protocol/messages.ts';
import { interpolate } from '../table2d.ts';
import { TUNE_FORMAT, TUNE_FORMAT_VERSION, type TuneFile, type TuneTable } from '../tuneFile.ts';
import {
  findConversion,
  GASOLINE_STOICH_AFR,
  LAMBDA,
  MSQ_TABLE_MAPPINGS,
  type MsqTableMapping,
  type MsqTableSource,
  type Quantity,
  X_AXIS_QUANTITY,
  Y_AXIS_QUANTITY,
} from './msqMapping.ts';
import { type MsqConstant, type MsqDocument, MsqError, parseMsq } from './parseMsq.ts';

/**
 * Turns a TunerStudio .msq into a tuning-sim tune (ADR 0012). The result goes through the
 * normal tune-loading path (validation, one undo step, setTableCells), so this module never
 * talks to the store or simcore.
 */

export interface MsqImport {
  tune: TuneFile;
  /** Things the user should know, e.g. that a table was resampled onto our axes. */
  warnings: string[];
}

/**
 * Resampled values are rounded to this many decimals, so the table editor does not show
 * interpolation noise like 61.23333333.
 */
const RESAMPLED_DECIMALS = 3;

/** Axes closer than this (relative) count as equal; msq files store rounded bins. */
const AXIS_TOLERANCE = 1e-6;

/** An stoich value further than this from 14.7 means the file is not tuned for gasoline. */
const STOICH_TOLERANCE_AFR = 0.05;

/**
 * Builds a tune for `engineId` from an .msq file. Our axes are kept: a table with different
 * msq axes is resampled bilinearly (clamped at the edges) and reported as a warning.
 * Throws MsqError if the file is unusable or contains no supported table.
 */
export function importMsq(
  text: string,
  engineId: string,
  current: ReadonlyMap<TableId, TableData>,
  savedAt: Date,
  parser?: DOMParser,
): MsqImport {
  const document = parseMsq(text, parser);
  const warnings: string[] = [];
  const tables: Record<TableId, TuneTable> = {};
  let afrSourceUsed = false;
  for (const mapping of MSQ_TABLE_MAPPINGS) {
    const target = current.get(mapping.tableId);
    const source = findSource(document, mapping);
    if (source === undefined) {
      warnings.push(`${mapping.tableId}: not in the file (${expectedNames(mapping)}), unchanged`);
    } else if (target?.editable !== true) {
      warnings.push(`${mapping.tableId}: the simulation has no editable table, skipped`);
    } else {
      tables[mapping.tableId] = convertTable(document, mapping, source, target, warnings);
      afrSourceUsed ||= mapping.quantity === LAMBDA && isAfr(document.constants.get(source.values));
    }
  }
  if (Object.keys(tables).length === 0) {
    throw new MsqError(`no supported table found (${warnings.join('; ')})`);
  }
  warnings.push(...stoichWarnings(document, afrSourceUsed));
  return {
    tune: {
      format: TUNE_FORMAT,
      version: TUNE_FORMAT_VERSION,
      engine: engineId,
      savedAt: savedAt.toISOString(),
      tables,
    },
    warnings,
  };
}

/**
 * Prefers a source whose three constants all exist (veTable is shared by several firmware
 * families, only the axis names differ); otherwise the first with the values constant, so the
 * user gets an error about the missing axis.
 */
function findSource(document: MsqDocument, mapping: MsqTableMapping): MsqTableSource | undefined {
  const has = (name: string): boolean => document.constants.has(name);
  return (
    mapping.sources.find(
      (source) => has(source.values) && has(source.xBins) && has(source.yBins),
    ) ?? mapping.sources.find((source) => has(source.values))
  );
}

function expectedNames(mapping: MsqTableMapping): string {
  return [...new Set(mapping.sources.map((source) => source.values))].join(' or ');
}

/** Source table in our units, shaped like TableData so table2d's interpolation applies. */
function convertTable(
  document: MsqDocument,
  mapping: MsqTableMapping,
  source: MsqTableSource,
  target: TableData,
  warnings: string[],
): TuneTable {
  const xBins = readBins(document, source.xBins, X_AXIS_QUANTITY, mapping.tableId);
  const yBins = readBins(document, source.yBins, Y_AXIS_QUANTITY, mapping.tableId);
  const values = readValues(document, source.values, mapping, xBins.length, yBins.length);
  const msqTable: TableData = {
    ...target,
    xAxis: { ...target.xAxis, bins: xBins },
    yAxis: { ...target.yAxis, bins: yBins },
    values,
  };
  const ourX = target.xAxis.bins;
  const ourY = target.yAxis.bins;
  if (!sameNumbers(xBins, ourX) || !sameNumbers(yBins, ourY)) {
    warnings.push(resampleWarning(mapping.tableId, msqTable, target));
  }
  return {
    unit: target.unit,
    xBins: [...ourX],
    yBins: [...ourY],
    // With equal axes the interpolation hits the breakpoints and returns the cells exactly.
    values: ourY.map((y) => ourX.map((x) => round(interpolate(msqTable, x, y)))),
  };
}

function readBins(
  document: MsqDocument,
  name: string,
  quantity: Quantity,
  tableId: TableId,
): number[] {
  const constant = requireNumeric(document, name, tableId);
  if (constant.rows !== 1 && constant.cols !== 1) {
    throw new MsqError(`${tableId}: axis '${name}' is ${dimensions(constant)}, not a list`);
  }
  const bins = convertAll(constant, quantity, tableId);
  if (bins.length < 2) {
    throw new MsqError(`${tableId}: axis '${name}' needs at least 2 bins`);
  }
  if (!bins.every((bin, index) => index === 0 || bin > (bins[index - 1] ?? bin))) {
    throw new MsqError(`${tableId}: axis '${name}' is not strictly increasing`);
  }
  return bins;
}

/** values[y][x]; the file stores rows from the lowest load bin up (controller memory order). */
function readValues(
  document: MsqDocument,
  name: string,
  mapping: MsqTableMapping,
  columns: number,
  rows: number,
): number[][] {
  const constant = requireNumeric(document, name, mapping.tableId);
  if (constant.rows !== rows || constant.cols !== columns) {
    throw new MsqError(
      `${mapping.tableId}: '${name}' is ${dimensions(constant)}, but its axes are ${String(rows)}×${String(columns)}`,
    );
  }
  const flat = convertAll(constant, mapping.quantity, mapping.tableId);
  return Array.from({ length: rows }, (_, row) => flat.slice(row * columns, (row + 1) * columns));
}

function requireNumeric(
  document: MsqDocument,
  name: string,
  tableId: TableId,
): MsqConstant & { values: number[] } {
  const constant = document.constants.get(name);
  if (constant === undefined) {
    throw new MsqError(`${tableId}: constant '${name}' is missing`);
  }
  const { values } = constant;
  if (values === null) {
    throw new MsqError(`${tableId}: constant '${name}' does not contain numbers`);
  }
  if (values.length !== constant.rows * constant.cols) {
    throw new MsqError(
      `${tableId}: constant '${name}' has ${String(values.length)} values, expected ${dimensions(constant)}`,
    );
  }
  return { ...constant, values };
}

function convertAll(
  constant: MsqConstant & { values: number[] },
  quantity: Quantity,
  tableId: TableId,
): number[] {
  const conversion = findConversion(quantity, constant.units);
  if (conversion === undefined) {
    const accepted = quantity.conversions.flatMap((entry) => entry.fileUnits).join(', ');
    throw new MsqError(
      `${tableId}: '${constant.name}' is in '${constant.units}', expected one of: ${accepted}`,
    );
  }
  return constant.values.map(conversion.convert);
}

function resampleWarning(tableId: TableId, msqTable: TableData, target: TableData): string {
  const clamped =
    extendsBeyond(target.xAxis.bins, msqTable.xAxis.bins) ||
    extendsBeyond(target.yAxis.bins, msqTable.yAxis.bins);
  return (
    `${tableId}: file axes ${describeAxes(msqTable)} differ from ours ${describeAxes(target)}, ` +
    `resampled bilinearly${clamped ? '; outside the file range the edge values are held' : ''}`
  );
}

function extendsBeyond(ours: readonly number[], theirs: readonly number[]): boolean {
  return (
    (ours[0] ?? 0) < (theirs[0] ?? 0) ||
    (ours[ours.length - 1] ?? 0) > (theirs[theirs.length - 1] ?? 0)
  );
}

function describeAxes(table: TableData): string {
  const range = (bins: readonly number[]): string =>
    `${String(bins[0])}–${String(bins[bins.length - 1])}`;
  return (
    `${String(table.yAxis.bins.length)}×${String(table.xAxis.bins.length)} ` +
    `(${range(table.xAxis.bins)} rpm, ${range(table.yAxis.bins)} kPa)`
  );
}

/** If lambda came from AFR, the file's stoich should be the gasoline value we converted with. */
function stoichWarnings(document: MsqDocument, afrSourceUsed: boolean): string[] {
  const stoich = document.constants.get('stoich')?.values?.[0];
  if (!afrSourceUsed || stoich === undefined) {
    return [];
  }
  return Math.abs(stoich - GASOLINE_STOICH_AFR) > STOICH_TOLERANCE_AFR
    ? [
        `lambdaTarget: the file's stoich is ${String(stoich)}, AFR was converted with ${String(GASOLINE_STOICH_AFR)} (gasoline)`,
      ]
    : [];
}

function isAfr(constant: MsqConstant | undefined): boolean {
  return constant?.units.trim().toLowerCase() === 'afr';
}

function dimensions(constant: MsqConstant): string {
  return `${String(constant.rows)}×${String(constant.cols)}`;
}

function round(value: number): number {
  const factor = 10 ** RESAMPLED_DECIMALS;
  return Math.round(value * factor) / factor;
}

function sameNumbers(a: readonly number[], b: readonly number[]): boolean {
  return (
    a.length === b.length &&
    a.every((value, index) => {
      const other = b[index] ?? Number.NaN;
      return Math.abs(value - other) <= AXIS_TOLERANCE * Math.max(1, Math.abs(other));
    })
  );
}
