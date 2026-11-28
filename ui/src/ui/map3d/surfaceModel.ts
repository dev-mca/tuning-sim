import { cellValue, columnCount, interpolate, rowCount } from '../../domain/table2d.ts';
import type { TableAxis, TableData } from '../../protocol/messages.ts';
import { normalise, parseHex, sequentialColor } from '../theme/colorScale.ts';
import { valueRange } from '../table/heatmapRenderer.ts';

/**
 * Pure geometry for the 3D table view. Everything here works on plain arrays, so it can be
 * tested without WebGL; the three.js components only copy these arrays into buffers.
 *
 * World coordinates (three.js is Y-up): rpm runs along X (left → right), MAP along Z with the
 * highest MAP at the back (seen from the front, that matches the 2D editor, where the highest
 * MAP row is on top) and the table value along Y. The surface fills a box centred on the
 * origin in X/Z with its floor at Y = 0.
 */
export interface SceneBox {
  width: number;
  depth: number;
  height: number;
}

/** World size of the plotted volume; the camera and controls are tuned to it. */
export const DEFAULT_BOX: SceneBox = { width: 10, depth: 7, height: 4 };

/** Maps a value from [domainMin, domainMax] linearly onto [rangeMin, rangeMax]. */
export type Scale = (value: number) => number;

export function linearScale(
  domainMin: number,
  domainMax: number,
  rangeMin: number,
  rangeMax: number,
): Scale {
  return (value) => rangeMin + normalise(value, domainMin, domainMax) * (rangeMax - rangeMin);
}

/** The three scales of one table in one box; heights use the table's own value range. */
export interface SurfaceScales {
  x: Scale;
  y: Scale;
  z: Scale;
  valueMin: number;
  valueMax: number;
}

export function surfaceScales(table: TableData, box: SceneBox = DEFAULT_BOX): SurfaceScales {
  const range = valueRange(table);
  return {
    // Axis values (not bin indices) on X/Z, so the uneven rpm breakpoints stay visible.
    x: axisScale(table.xAxis, -box.width / 2, box.width / 2),
    z: axisScale(table.yAxis, box.depth / 2, -box.depth / 2),
    y: linearScale(range.min, range.max, 0, box.height),
    valueMin: range.min,
    valueMax: range.max,
  };
}

/** Vertex data of the surface: one vertex per table cell, two triangles per grid square. */
export interface SurfaceGeometry {
  /** xyz per vertex; vertex index = y * columns + x (same order as values[y][x]). */
  positions: Float32Array;
  /** Linear RGB per vertex (three.js works in linear colour space). */
  colors: Float32Array;
  indices: Uint32Array;
}

export function surfaceGeometry(table: TableData, box: SceneBox = DEFAULT_BOX): SurfaceGeometry {
  const columns = columnCount(table);
  const rows = rowCount(table);
  const scales = surfaceScales(table, box);
  const positions = new Float32Array(columns * rows * 3);
  const colors = new Float32Array(columns * rows * 3);
  for (let y = 0; y < rows; y++) {
    for (let x = 0; x < columns; x++) {
      const value = cellValue(table, x, y);
      const offset = (y * columns + x) * 3;
      positions[offset] = scales.x(binAt(table.xAxis, x));
      positions[offset + 1] = scales.y(value);
      positions[offset + 2] = scales.z(binAt(table.yAxis, y));
      // Same colour as the 2D heatmap cell, so both views read the same.
      const [r, g, b] = srgbHexToLinear(
        sequentialColor(normalise(value, scales.valueMin, scales.valueMax)).fill,
      );
      colors[offset] = r;
      colors[offset + 1] = g;
      colors[offset + 2] = b;
    }
  }
  return { positions, colors, indices: gridTriangles(columns, rows) };
}

/**
 * Two counter-clockwise triangles (seen from above, +Y) per grid square. Because MAP runs
 * towards -Z, "row + 1" lies behind "row", which decides the winding.
 */
export function gridTriangles(columns: number, rows: number): Uint32Array {
  const squares = Math.max(columns - 1, 0) * Math.max(rows - 1, 0);
  const indices = new Uint32Array(squares * 6);
  let i = 0;
  for (let y = 0; y < rows - 1; y++) {
    for (let x = 0; x < columns - 1; x++) {
      const a = y * columns + x;
      const b = a + 1;
      const c = a + columns;
      const d = c + 1;
      indices.set([a, b, d, a, d, c], i);
      i += 6;
    }
  }
  return indices;
}

/**
 * Index pairs for line segments along every row and column, drawn on top of the surface:
 * they show the cell grid and give the shape depth cues without lighting (lighting would
 * change the colours and break the match with the 2D heatmap).
 */
export function gridLineIndices(columns: number, rows: number): Uint32Array {
  const pairs: number[] = [];
  for (let y = 0; y < rows; y++) {
    for (let x = 0; x < columns; x++) {
      const index = y * columns + x;
      if (x < columns - 1) {
        pairs.push(index, index + 1);
      }
      if (y < rows - 1) {
        pairs.push(index, index + columns);
      }
    }
  }
  return Uint32Array.from(pairs);
}

/**
 * World position of the operating point on the surface. Outside the axes it is clamped to the
 * edge, the same way the ECU clamps its lookup (research.md §2.3).
 */
export function operatingPoint(
  table: TableData,
  rpm: number,
  map_kPa: number,
  box: SceneBox = DEFAULT_BOX,
): [number, number, number] {
  const scales = surfaceScales(table, box);
  const clampedRpm = clampToAxis(table.xAxis, rpm);
  const clampedMap = clampToAxis(table.yAxis, map_kPa);
  return [
    scales.x(clampedRpm),
    scales.y(interpolate(table, clampedRpm, clampedMap)),
    scales.z(clampedMap),
  ];
}

/**
 * Breakpoints to label, at most `maxLabels`, always including the first and the last one
 * (16 rpm labels on a 10-unit axis would overlap).
 */
export function axisLabelBins(bins: readonly number[], maxLabels: number): number[] {
  if (bins.length <= maxLabels || maxLabels < 2) {
    return [...bins];
  }
  const step = Math.ceil((bins.length - 1) / (maxLabels - 1));
  const picked = bins.filter((_, index) => index % step === 0);
  const last = bins.at(-1);
  if (last !== undefined && picked.at(-1) !== last) {
    // Replace a label that would sit too close to the last one instead of crowding it.
    if (bins.length - 1 - (picked.length - 1) * step < step / 2) {
      picked.pop();
    }
    picked.push(last);
  }
  return picked;
}

/**
 * "Nice" tick values (steps of 1, 2 or 5 × 10^n) covering [min, max], roughly `count` of them.
 * Used for the value axis, whose range comes from the data and is rarely round.
 */
export function niceTicks(min: number, max: number, count: number): number[] {
  if (!Number.isFinite(min) || !Number.isFinite(max) || count < 1) {
    return [];
  }
  if (max <= min) {
    return [min];
  }
  const rawStep = (max - min) / count;
  const magnitude = 10 ** Math.floor(Math.log10(rawStep));
  const step = ([1, 2, 5, 10].find((factor) => factor * magnitude >= rawStep) ?? 10) * magnitude;
  const ticks: number[] = [];
  // Small epsilon so a tick exactly at max survives floating-point error.
  const epsilon = step * 1e-9;
  for (let tick = Math.ceil(min / step) * step; tick <= max + epsilon; tick += step) {
    // Rounding to the step's precision removes noise such as 0.30000000000000004.
    ticks.push(Number(tick.toPrecision(12)));
  }
  return ticks;
}

/** Axis title with unit, e.g. "rpm [1/min]". */
export function axisTitle(name: string, unit: string): string {
  return unit === '' ? name : `${name} [${unit}]`;
}

/** sRGB #rrggbb → linear RGB in [0, 1] (IEC 61966-2-1 transfer function). */
export function srgbHexToLinear(hex: string): [number, number, number] {
  const { r, g, b } = parseHex(hex);
  return [srgbChannelToLinear(r), srgbChannelToLinear(g), srgbChannelToLinear(b)];
}

function srgbChannelToLinear(value: number): number {
  const c = value / 255;
  return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
}

function axisScale(axis: TableAxis, rangeMin: number, rangeMax: number): Scale {
  return linearScale(axis.bins[0] ?? 0, axis.bins.at(-1) ?? 0, rangeMin, rangeMax);
}

function clampToAxis(axis: TableAxis, value: number): number {
  const first = axis.bins[0] ?? value;
  const last = axis.bins.at(-1) ?? value;
  return Math.min(Math.max(value, first), last);
}

function binAt(axis: TableAxis, index: number): number {
  const bin = axis.bins[index];
  if (bin === undefined) {
    throw new RangeError(`bin ${String(index)} is outside axis '${axis.name}'`);
  }
  return bin;
}
