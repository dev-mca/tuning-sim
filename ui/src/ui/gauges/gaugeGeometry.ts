/**
 * Geometry of a radial gauge in SVG coordinates. Angles are in degrees, measured clockwise
 * from 12 o'clock, because that is how one thinks about a dial (0 deg = needle straight up).
 */

export interface DialRange {
  min: number;
  max: number;
  /** Angle of `min`, e.g. -135 deg = lower left. */
  startAngle_deg: number;
  /** Total sweep of the dial, e.g. 270 deg. */
  sweep_deg: number;
}

export interface Point {
  x: number;
  y: number;
}

export interface Tick {
  value: number;
  major: boolean;
}

/** Maps a value linearly onto the dial and clamps it, so the needle never leaves the scale. */
export function valueToAngle(value: number, range: DialRange): number {
  const span = range.max - range.min;
  const fraction = span === 0 ? 0 : (value - range.min) / span;
  const clamped = Math.min(Math.max(fraction, 0), 1);
  return range.startAngle_deg + clamped * range.sweep_deg;
}

export function polarToCartesian(center: Point, radius: number, angle_deg: number): Point {
  const angle_rad = (angle_deg * Math.PI) / 180;
  return {
    x: center.x + radius * Math.sin(angle_rad),
    y: center.y - radius * Math.cos(angle_rad),
  };
}

/** SVG path data for a circular arc from `from_deg` to `to_deg` (clockwise). */
export function arcPath(center: Point, radius: number, from_deg: number, to_deg: number): string {
  const start = polarToCartesian(center, radius, from_deg);
  const end = polarToCartesian(center, radius, to_deg);
  const largeArc = Math.abs(to_deg - from_deg) > 180 ? 1 : 0;
  const sweepClockwise = to_deg >= from_deg ? 1 : 0;
  return [
    `M ${fmt(start.x)} ${fmt(start.y)}`,
    `A ${fmt(radius)} ${fmt(radius)} 0 ${String(largeArc)} ${String(sweepClockwise)} ${fmt(end.x)} ${fmt(end.y)}`,
  ].join(' ');
}

/**
 * Tick marks from min to max. `majorStep` is the labelled spacing, `minorPerMajor` the number of
 * intervals between two labels (e.g. 5 = a minor tick every 1/5 of a major step).
 * Values are computed from integer counters, not by repeated addition, so 0.1 steps stay exact.
 */
export function ticks(min: number, max: number, majorStep: number, minorPerMajor: number): Tick[] {
  if (majorStep <= 0 || minorPerMajor < 1 || max <= min) {
    return [];
  }
  const minorStep = majorStep / minorPerMajor;
  const count = Math.round((max - min) / minorStep);
  const result: Tick[] = [];
  for (let i = 0; i <= count; i++) {
    result.push({ value: roundAway(min + i * minorStep), major: i % minorPerMajor === 0 });
  }
  return result;
}

/** Removes floating-point dust such as 0.30000000000000004 from tick labels. */
function roundAway(value: number): number {
  return Math.round(value * 1e9) / 1e9;
}

function fmt(value: number): string {
  return value.toFixed(3);
}
