import type { TableData, TableId } from '../protocol/messages.ts';

/** What differed from the original tune when a run was measured, per table. */
export interface TableDifference {
  tableId: TableId;
  unit: string;
  changedCells: number;
  /** Mean change of the changed cells, in the table's unit. */
  meanDelta: number;
}

/**
 * Compares the current tables with the originals. Shown next to each dyno run, so the overlay
 * says which tune produced which curve.
 */
export function tuneDifferences(
  tables: ReadonlyMap<TableId, TableData>,
  originals: ReadonlyMap<TableId, TableData>,
): TableDifference[] {
  const result: TableDifference[] = [];
  for (const [id, table] of tables) {
    const original = originals.get(id);
    if (original === undefined) {
      continue;
    }
    let changedCells = 0;
    let deltaSum = 0;
    table.values.forEach((row, y) => {
      row.forEach((value, x) => {
        const before = original.values[y]?.[x];
        if (before !== undefined && before !== value) {
          changedCells++;
          deltaSum += value - before;
        }
      });
    });
    if (changedCells > 0) {
      result.push({
        tableId: id,
        unit: table.unit,
        changedCells,
        meanDelta: deltaSum / changedCells,
      });
    }
  }
  return result;
}

/** Short text such as "ignition: 12 cells, mean −2.5 deg BTDC"; "original tune" if nothing changed. */
export function describeDifferences(differences: readonly TableDifference[]): string {
  if (differences.length === 0) {
    return 'original tune';
  }
  return differences
    .map((difference) => {
      const sign = difference.meanDelta >= 0 ? '+' : '−';
      const cells = difference.changedCells === 1 ? 'cell' : 'cells';
      const magnitude = Math.abs(difference.meanDelta).toPrecision(2);
      return `${difference.tableId}: ${String(difference.changedCells)} ${cells}, mean ${sign}${magnitude} ${difference.unit}`;
    })
    .join(' · ');
}
