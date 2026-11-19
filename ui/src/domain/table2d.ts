import type { TableData } from '../protocol/messages.ts';

/** Position of a value between two breakpoints: bins[index] + fraction * (bins[index+1] - bins[index]). */
export interface BinPosition {
  index: number;
  fraction: number;
}

/**
 * Finds the bin of `value` with binary search and clamps outside the axis
 * (research.md §2.3, same rule as Table2D in simcore and lab).
 */
export function findBin(bins: readonly number[], value: number): BinPosition {
  const last = bins.length - 1;
  if (last < 1) {
    return { index: 0, fraction: 0 };
  }
  if (value <= binAt(bins, 0)) {
    return { index: 0, fraction: 0 };
  }
  if (value >= binAt(bins, last)) {
    return { index: last - 1, fraction: 1 };
  }
  let low = 0;
  let high = last;
  while (high - low > 1) {
    const middle = (low + high) >> 1;
    if (binAt(bins, middle) <= value) {
      low = middle;
    } else {
      high = middle;
    }
  }
  const lower = binAt(bins, low);
  return { index: low, fraction: (value - lower) / (binAt(bins, low + 1) - lower) };
}

/** Bilinear interpolation with clamping at the edges (research.md §2.3). */
export function interpolate(table: TableData, x: number, y: number): number {
  const column = findBin(table.xAxis.bins, x);
  const row = findBin(table.yAxis.bins, y);
  const t = column.fraction;
  const u = row.fraction;
  const nextColumn = Math.min(column.index + 1, table.xAxis.bins.length - 1);
  const nextRow = Math.min(row.index + 1, table.yAxis.bins.length - 1);
  return (
    (1 - t) * (1 - u) * cellValue(table, column.index, row.index) +
    t * (1 - u) * cellValue(table, nextColumn, row.index) +
    (1 - t) * u * cellValue(table, column.index, nextRow) +
    t * u * cellValue(table, nextColumn, nextRow)
  );
}

/**
 * Fractional cell coordinates of an operating point, e.g. column 3.5 = halfway between the
 * 4th and 5th rpm bin. The table editor draws the live cursor there.
 */
export function fractionalCell(table: TableData, x: number, y: number): { x: number; y: number } {
  const column = findBin(table.xAxis.bins, x);
  const row = findBin(table.yAxis.bins, y);
  return { x: column.index + column.fraction, y: row.index + row.fraction };
}

/** values[y][x]; throws on an index outside the table because that is always a programming error. */
export function cellValue(table: TableData, x: number, y: number): number {
  const value = table.values[y]?.[x];
  if (value === undefined) {
    throw new RangeError(`cell (${String(x)}, ${String(y)}) is outside table '${table.id}'`);
  }
  return value;
}

export function columnCount(table: TableData): number {
  return table.xAxis.bins.length;
}

export function rowCount(table: TableData): number {
  return table.yAxis.bins.length;
}

function binAt(bins: readonly number[], index: number): number {
  const bin = bins[index];
  if (bin === undefined) {
    throw new RangeError(`bin ${String(index)} is outside the axis`);
  }
  return bin;
}
