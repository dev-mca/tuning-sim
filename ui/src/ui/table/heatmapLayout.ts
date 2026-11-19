import type { Cell } from '../../domain/selection.ts';

/**
 * Pixel layout of the heatmap canvas. Rows are drawn upside down: the highest MAP row is at the
 * top, as in TunerStudio, so "more load" means "higher up". Table row 0 (lowest MAP) is
 * therefore the bottom screen row.
 */
export interface HeatmapLayout {
  columns: number;
  rows: number;
  /** Space for the MAP axis labels on the left. */
  axisLeft: number;
  /** Space for the rpm axis labels at the bottom. */
  axisBottom: number;
  cellWidth: number;
  cellHeight: number;
  width: number;
  height: number;
}

export interface PixelRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

const AXIS_LEFT_PX = 40;
const AXIS_BOTTOM_PX = 22;
const MIN_CELL_HEIGHT_PX = 20;
const MAX_CELL_HEIGHT_PX = 34;
/** Cells a bit wider than high, so a value like "105.5" fits. */
const CELL_ASPECT = 0.72;

export function computeLayout(width: number, columns: number, rows: number): HeatmapLayout {
  const cellWidth = Math.max((width - AXIS_LEFT_PX) / columns, 1);
  const cellHeight = Math.min(
    Math.max(cellWidth * CELL_ASPECT, MIN_CELL_HEIGHT_PX),
    MAX_CELL_HEIGHT_PX,
  );
  return {
    columns,
    rows,
    axisLeft: AXIS_LEFT_PX,
    axisBottom: AXIS_BOTTOM_PX,
    cellWidth,
    cellHeight,
    width,
    height: cellHeight * rows + AXIS_BOTTOM_PX,
  };
}

/** Screen rectangle of a table cell. */
export function cellRect(layout: HeatmapLayout, cell: Cell): PixelRect {
  return {
    x: layout.axisLeft + cell.x * layout.cellWidth,
    y: (layout.rows - 1 - cell.y) * layout.cellHeight,
    width: layout.cellWidth,
    height: layout.cellHeight,
  };
}

/** Table cell under a pixel, or null if the pixel is on an axis or outside the grid. */
export function cellAtPixel(layout: HeatmapLayout, px: number, py: number): Cell | null {
  const column = Math.floor((px - layout.axisLeft) / layout.cellWidth);
  const screenRow = Math.floor(py / layout.cellHeight);
  if (column < 0 || column >= layout.columns || screenRow < 0 || screenRow >= layout.rows) {
    return null;
  }
  return { x: column, y: layout.rows - 1 - screenRow };
}

/** Like cellAtPixel, but clamps to the nearest cell; used while dragging past the edge. */
export function nearestCellAtPixel(layout: HeatmapLayout, px: number, py: number): Cell {
  const column = Math.floor((px - layout.axisLeft) / layout.cellWidth);
  const screenRow = Math.floor(py / layout.cellHeight);
  const clamp = (value: number, count: number): number => Math.min(Math.max(value, 0), count - 1);
  return { x: clamp(column, layout.columns), y: layout.rows - 1 - clamp(screenRow, layout.rows) };
}

/**
 * Centre pixel of a fractional cell position (from fractionalCell). Position 0 is the centre of
 * the first cell, because the bin value belongs to the cell centre.
 */
export function fractionalToPixel(
  layout: HeatmapLayout,
  position: { x: number; y: number },
): { x: number; y: number } {
  return {
    x: layout.axisLeft + (position.x + 0.5) * layout.cellWidth,
    y: (layout.rows - 1 - position.y + 0.5) * layout.cellHeight,
  };
}
