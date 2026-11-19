import type { CellValue, TableData } from '../protocol/messages.ts';
import { type Cell, cellsInSelection, type Selection } from './selection.ts';
import { cellValue, columnCount, rowCount } from './table2d.ts';

/**
 * Pure editing operations on ECU tables. Each operation returns the list of changed cells
 * instead of mutating the table, so the same list can be sent to simcore (setTableCells),
 * applied locally and stored for undo.
 */

/** How fine a table is edited: step for +/- and the number of decimals values are rounded to. */
export interface EditPrecision {
  step: number;
  decimals: number;
}

const DEFAULT_PRECISION: EditPrecision = { step: 1, decimals: 1 };

/** Typical tuner steps: VE in 0.5 %, lambda in 0.01, ignition in 0.5 deg. */
const PRECISION_BY_TABLE: Readonly<Record<string, EditPrecision>> = {
  ve: { step: 0.5, decimals: 1 },
  lambdaTarget: { step: 0.01, decimals: 3 },
  ignition: { step: 0.5, decimals: 1 },
  ignitionMbt: { step: 0.5, decimals: 1 },
};

/** Factor between the small step (+/-) and the large step (PageUp/PageDown). */
export const LARGE_STEP_FACTOR = 5;

export function editPrecision(tableId: string): EditPrecision {
  return PRECISION_BY_TABLE[tableId] ?? DEFAULT_PRECISION;
}

/** Adds `delta` to every selected cell. */
export function adjustCells(table: TableData, selection: Selection, delta: number): CellValue[] {
  return mapSelectedCells(table, selection, (cell) => cellValue(table, cell.x, cell.y) + delta);
}

/** Sets every selected cell to the same value. */
export function setCells(table: TableData, selection: Selection, value: number): CellValue[] {
  return mapSelectedCells(table, selection, () => value);
}

/**
 * Smooths the selection: each selected cell becomes the mean of itself and its up to 8
 * neighbours (3x3 box filter). Neighbours are read from the unchanged table, so the
 * result does not depend on the order in which cells are visited.
 */
export function smoothCells(table: TableData, selection: Selection): CellValue[] {
  return mapSelectedCells(table, selection, (cell) => neighbourhoodMean(table, cell));
}

/**
 * Fills the selection by bilinear interpolation between its four corner values, the classic
 * "interpolate between corners" tool of tuning software. Needs at least 2 cells per axis to
 * interpolate along that axis.
 */
export function interpolateCorners(table: TableData, selection: Selection): CellValue[] {
  const cells = cellsInSelection(selection);
  const first = cells[0];
  const last = cells[cells.length - 1];
  if (first === undefined || last === undefined) {
    return [];
  }
  const width = last.x - first.x;
  const height = last.y - first.y;
  const bottomLeft = cellValue(table, first.x, first.y);
  const bottomRight = cellValue(table, last.x, first.y);
  const topLeft = cellValue(table, first.x, last.y);
  const topRight = cellValue(table, last.x, last.y);
  return mapSelectedCells(table, selection, (cell) => {
    const t = width === 0 ? 0 : (cell.x - first.x) / width;
    const u = height === 0 ? 0 : (cell.y - first.y) / height;
    return (
      (1 - t) * (1 - u) * bottomLeft +
      t * (1 - u) * bottomRight +
      (1 - t) * u * topLeft +
      t * u * topRight
    );
  });
}

/** Returns a new table with the changes applied; the original table stays untouched. */
export function applyCells(table: TableData, cells: readonly CellValue[]): TableData {
  const values = table.values.map((row) => [...row]);
  for (const cell of cells) {
    const row = values[cell.y];
    if (row === undefined || cell.x < 0 || cell.x >= row.length) {
      throw new RangeError(
        `cell (${String(cell.x)}, ${String(cell.y)}) is outside table '${table.id}'`,
      );
    }
    row[cell.x] = cell.value;
  }
  return { ...table, values };
}

/** The current values of the given cells, used to record the "before" state for undo. */
export function readCells(table: TableData, cells: readonly Cell[]): CellValue[] {
  return cells.map((cell) => ({ x: cell.x, y: cell.y, value: cellValue(table, cell.x, cell.y) }));
}

function mapSelectedCells(
  table: TableData,
  selection: Selection,
  newValue: (cell: Cell) => number,
): CellValue[] {
  const precision = editPrecision(table.id);
  return cellsInSelection(selection)
    .map((cell) => ({
      x: cell.x,
      y: cell.y,
      value: clampAndRound(newValue(cell), table, precision),
    }))
    .filter((change) => change.value !== cellValue(table, change.x, change.y));
}

function clampAndRound(value: number, table: TableData, precision: EditPrecision): number {
  const clamped = Math.min(Math.max(value, table.min), table.max);
  const scale = 10 ** precision.decimals;
  return Math.round(clamped * scale) / scale;
}

function neighbourhoodMean(table: TableData, center: Cell): number {
  let sum = 0;
  let count = 0;
  for (let y = center.y - 1; y <= center.y + 1; y++) {
    for (let x = center.x - 1; x <= center.x + 1; x++) {
      if (x >= 0 && x < columnCount(table) && y >= 0 && y < rowCount(table)) {
        sum += cellValue(table, x, y);
        count++;
      }
    }
  }
  return sum / count;
}
