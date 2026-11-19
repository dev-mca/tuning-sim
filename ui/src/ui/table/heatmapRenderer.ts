import { isSelected, type Selection } from '../../domain/selection.ts';
import { cellValue } from '../../domain/table2d.ts';
import { editPrecision } from '../../domain/tableEdit.ts';
import type { TableData } from '../../protocol/messages.ts';
import { normalise, sequentialColor } from '../theme/colorScale.ts';
import { SURFACE } from '../theme/palette.ts';
import { cellRect, fractionalToPixel, type HeatmapLayout } from './heatmapLayout.ts';

const CELL_GAP_PX = 1;
const VALUE_FONT = '600 11px "JetBrains Mono", ui-monospace, monospace';
const AXIS_FONT = '10px "JetBrains Mono", ui-monospace, monospace';
const MODIFIED_MARK_PX = 6;
const CURSOR_RADIUS_PX = 5;

/** Range of the values actually in the table, so the colour scale uses its full contrast. */
export function valueRange(table: TableData): { min: number; max: number } {
  const all = table.values.flat();
  return { min: Math.min(...all), max: Math.max(...all) };
}

/** Prepares a canvas for crisp drawing on HiDPI screens and returns its 2D context. */
export function prepareCanvas(
  canvas: HTMLCanvasElement,
  width: number,
  height: number,
): CanvasRenderingContext2D | null {
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  canvas.style.width = `${String(width)}px`;
  canvas.style.height = `${String(height)}px`;
  const context = canvas.getContext('2d');
  context?.setTransform(ratio, 0, 0, ratio, 0, 0);
  return context;
}

/** Draws cells, values, selection, edit marks and axis labels. Called only when something changes. */
export function drawHeatmap(
  context: CanvasRenderingContext2D,
  layout: HeatmapLayout,
  table: TableData,
  original: TableData | undefined,
  selection: Selection,
): void {
  context.clearRect(0, 0, layout.width, layout.height);
  const range = valueRange(table);
  const decimals = Math.min(editPrecision(table.id).decimals, 2);
  context.font = VALUE_FONT;
  context.textAlign = 'center';
  context.textBaseline = 'middle';

  for (let y = 0; y < layout.rows; y++) {
    for (let x = 0; x < layout.columns; x++) {
      const value = cellValue(table, x, y);
      const rect = cellRect(layout, { x, y });
      const colors = sequentialColor(normalise(value, range.min, range.max));
      context.fillStyle = colors.fill;
      context.fillRect(
        rect.x + CELL_GAP_PX,
        rect.y + CELL_GAP_PX,
        rect.width - CELL_GAP_PX,
        rect.height - CELL_GAP_PX,
      );
      context.fillStyle = colors.text;
      context.fillText(
        value.toFixed(decimals),
        rect.x + rect.width / 2,
        rect.y + rect.height / 2 + 0.5,
      );
      if (original !== undefined && cellValue(original, x, y) !== value) {
        drawModifiedMark(context, rect);
      }
    }
  }
  drawSelection(context, layout, selection);
  drawAxes(context, layout, table);
}

/** Live cursor: a trail of recent operating points and a ring at the current one. */
export function drawCursor(
  context: CanvasRenderingContext2D,
  layout: HeatmapLayout,
  trail: readonly { x: number; y: number }[],
): void {
  context.clearRect(0, 0, layout.width, layout.height);
  trail.forEach((position, index) => {
    const point = fractionalToPixel(layout, position);
    const age = (index + 1) / trail.length;
    context.fillStyle = `rgba(255, 138, 31, ${(age * 0.45).toFixed(3)})`;
    context.beginPath();
    context.arc(point.x, point.y, 2 + age * 1.5, 0, Math.PI * 2);
    context.fill();
  });
  const current = trail.at(-1);
  if (current === undefined) {
    return;
  }
  const point = fractionalToPixel(layout, current);
  context.lineWidth = 2;
  context.strokeStyle = SURFACE.canvas;
  context.beginPath();
  context.arc(point.x, point.y, CURSOR_RADIUS_PX + 1.5, 0, Math.PI * 2);
  context.stroke();
  context.strokeStyle = SURFACE.accent;
  context.beginPath();
  context.arc(point.x, point.y, CURSOR_RADIUS_PX, 0, Math.PI * 2);
  context.stroke();
}

function drawModifiedMark(
  context: CanvasRenderingContext2D,
  rect: { x: number; y: number; width: number },
): void {
  const right = rect.x + rect.width;
  context.fillStyle = SURFACE.accent;
  context.beginPath();
  context.moveTo(right - MODIFIED_MARK_PX, rect.y + CELL_GAP_PX);
  context.lineTo(right, rect.y + CELL_GAP_PX);
  context.lineTo(right, rect.y + CELL_GAP_PX + MODIFIED_MARK_PX);
  context.closePath();
  context.fill();
}

function drawSelection(
  context: CanvasRenderingContext2D,
  layout: HeatmapLayout,
  selection: Selection,
): void {
  const corners = [selection.anchor, selection.focus].map((cell) => cellRect(layout, cell));
  const left = Math.min(...corners.map((rect) => rect.x));
  const top = Math.min(...corners.map((rect) => rect.y));
  const right = Math.max(...corners.map((rect) => rect.x + rect.width));
  const bottom = Math.max(...corners.map((rect) => rect.y + rect.height));
  context.fillStyle = 'rgba(255, 255, 255, 0.10)';
  for (let y = 0; y < layout.rows; y++) {
    for (let x = 0; x < layout.columns; x++) {
      if (isSelected(selection, { x, y })) {
        const rect = cellRect(layout, { x, y });
        context.fillRect(rect.x, rect.y, rect.width, rect.height);
      }
    }
  }
  context.lineWidth = 2;
  context.strokeStyle = SURFACE.ink;
  context.strokeRect(left + 1, top + 1, right - left - 1, bottom - top - 1);
}

function drawAxes(
  context: CanvasRenderingContext2D,
  layout: HeatmapLayout,
  table: TableData,
): void {
  context.font = AXIS_FONT;
  context.fillStyle = SURFACE.muted;
  context.textAlign = 'right';
  context.textBaseline = 'middle';
  table.yAxis.bins.forEach((bin, y) => {
    const rect = cellRect(layout, { x: 0, y });
    context.fillText(String(bin), layout.axisLeft - 6, rect.y + rect.height / 2);
  });
  context.textAlign = 'center';
  context.textBaseline = 'top';
  table.xAxis.bins.forEach((bin, x) => {
    const rect = cellRect(layout, { x, y: 0 });
    context.fillText(String(bin), rect.x + rect.width / 2, rect.y + rect.height + 6);
  });
}
