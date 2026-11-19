import { findBin } from '../../domain/table2d.ts';
import type { DynoResultPayload } from '../../protocol/messages.ts';

export interface AlignedSeries {
  /** Union of all rpm points, ascending. */
  rpm: number[];
  /** One array per run, same length as rpm; null outside the run's rpm range. */
  values: (number | null)[][];
}

export type DynoField = (result: DynoResultPayload) => readonly (number | null)[];

/**
 * uPlot needs one shared x array for all series. simcore reports the measured speed of each
 * point (e.g. 4602.9 rpm), so two runs almost never share x values. Each run is therefore
 * interpolated linearly at the union of all x values, and only left empty outside its own range.
 */
export function alignRuns(results: readonly DynoResultPayload[], field: DynoField): AlignedSeries {
  const rpm = [...new Set(results.flatMap((result) => result.rpm))].sort((a, b) => a - b);
  const values = results.map((result) => {
    const curve = sortedCurve(result, field);
    return rpm.map((point) => interpolateCurve(curve, point));
  });
  return { rpm, values };
}

/** Value of a run at any rpm inside its range (linear interpolation), otherwise null. */
export function valueAtRpm(
  result: DynoResultPayload,
  field: DynoField,
  rpm: number,
): number | null {
  return interpolateCurve(sortedCurve(result, field), rpm);
}

interface Curve {
  x: number[];
  y: (number | null)[];
}

function sortedCurve(result: DynoResultPayload, field: DynoField): Curve {
  const series = field(result);
  const points = result.rpm
    .map((x, index) => ({ x, y: series[index] ?? null }))
    .sort((a, b) => a.x - b.x);
  return { x: points.map((point) => point.x), y: points.map((point) => point.y) };
}

function interpolateCurve(curve: Curve, rpm: number): number | null {
  const first = curve.x[0];
  const last = curve.x.at(-1);
  if (first === undefined || last === undefined || rpm < first || rpm > last) {
    return null;
  }
  const { index, fraction } = findBin(curve.x, rpm);
  const lower = curve.y[index] ?? null;
  const upper = curve.y[index + 1] ?? lower;
  if (lower === null || upper === null) {
    return null;
  }
  return lower + (upper - lower) * fraction;
}
