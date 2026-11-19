/** A cell coordinate in table indices (x = column/rpm bin, y = row/MAP bin). */
export interface Cell {
  x: number;
  y: number;
}

/**
 * Rectangular selection between an anchor (where the drag started) and a focus (where it is now).
 * Keeping both instead of min/max lets Shift+arrow grow or shrink the selection from the anchor,
 * like in a spreadsheet.
 */
export interface Selection {
  anchor: Cell;
  focus: Cell;
}

export interface CellRect {
  xMin: number;
  xMax: number;
  yMin: number;
  yMax: number;
}

export function singleCell(cell: Cell): Selection {
  return { anchor: cell, focus: cell };
}

export function selectAll(columns: number, rows: number): Selection {
  return { anchor: { x: 0, y: 0 }, focus: { x: columns - 1, y: rows - 1 } };
}

export function selectionRect(selection: Selection): CellRect {
  return {
    xMin: Math.min(selection.anchor.x, selection.focus.x),
    xMax: Math.max(selection.anchor.x, selection.focus.x),
    yMin: Math.min(selection.anchor.y, selection.focus.y),
    yMax: Math.max(selection.anchor.y, selection.focus.y),
  };
}

export function cellsInSelection(selection: Selection): Cell[] {
  const rect = selectionRect(selection);
  const cells: Cell[] = [];
  for (let y = rect.yMin; y <= rect.yMax; y++) {
    for (let x = rect.xMin; x <= rect.xMax; x++) {
      cells.push({ x, y });
    }
  }
  return cells;
}

export function isSelected(selection: Selection, cell: Cell): boolean {
  const rect = selectionRect(selection);
  return cell.x >= rect.xMin && cell.x <= rect.xMax && cell.y >= rect.yMin && cell.y <= rect.yMax;
}

/** Moves a cell by (dx, dy) and keeps it inside the table. */
export function moveCell(cell: Cell, dx: number, dy: number, columns: number, rows: number): Cell {
  return {
    x: clampIndex(cell.x + dx, columns),
    y: clampIndex(cell.y + dy, rows),
  };
}

/**
 * Arrow-key navigation: without Shift the selection collapses to the moved cell,
 * with Shift only the focus moves and the anchor stays.
 */
export function moveSelection(
  selection: Selection,
  dx: number,
  dy: number,
  extend: boolean,
  columns: number,
  rows: number,
): Selection {
  const focus = moveCell(selection.focus, dx, dy, columns, rows);
  return extend ? { anchor: selection.anchor, focus } : singleCell(focus);
}

function clampIndex(index: number, count: number): number {
  return Math.min(Math.max(index, 0), count - 1);
}
