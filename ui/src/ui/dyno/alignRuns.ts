import type { DynoResultPayload } from '../../protocol/messages.ts';

export interface AlignedSeries {
  /** Union of all rpm points, ascending. */
  rpm: number[];
  /** One array per run, same length as rpm; null where the run has no point. */
  values: (number | null)[][];
}

export type DynoField = (result: DynoResultPayload) => readonly (number | null)[];

/**
 * uPlot needs one shared x array for all series. Runs can cover different rpm ranges, so this
 * builds the union of their rpm points and fills gaps with null (uPlot draws no line there).
 */
export function alignRuns(results: readonly DynoResultPayload[], field: DynoField): AlignedSeries {
  const rpm = [...new Set(results.flatMap((result) => result.rpm))].sort((a, b) => a - b);
  const values = results.map((result) => {
    const byRpm = new Map<number, number | null>();
    const series = field(result);
    result.rpm.forEach((point, index) => {
      byRpm.set(point, series[index] ?? null);
    });
    return rpm.map((point) => byRpm.get(point) ?? null);
  });
  return { rpm, values };
}

/** Value of a run at a given rpm, or null if the run has no point there. */
export function valueAtRpm(
  result: DynoResultPayload,
  field: DynoField,
  rpm: number,
): number | null {
  const index = result.rpm.indexOf(rpm);
  return index < 0 ? null : (field(result)[index] ?? null);
}
