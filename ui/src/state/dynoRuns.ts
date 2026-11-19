import type { DynoResultPayload } from '../protocol/messages.ts';

/**
 * Dyno runs shown in the overlay. Each run keeps its colour slot for its whole life
 * ("colour follows the entity, not its rank"), so deleting run 2 does not repaint run 3.
 */
export interface DynoRun {
  id: number;
  label: string;
  /** Index into the categorical palette (0..MAX_RUNS-1). */
  colorSlot: number;
  visible: boolean;
  /** Which tune produced the run, e.g. "ignition: 12 cells, mean −2.5 deg BTDC". */
  note: string;
  result: DynoResultPayload;
}

/** The palette has 8 distinguishable colours; more lines would no longer be readable. */
export const MAX_RUNS = 8;

/** Adds a run in the first free colour slot; beyond MAX_RUNS the oldest run is dropped. */
export function addRun(
  runs: readonly DynoRun[],
  runNumber: number,
  result: DynoResultPayload,
  note = '',
): DynoRun[] {
  const kept = runs.length >= MAX_RUNS ? runs.slice(runs.length - MAX_RUNS + 1) : [...runs];
  const usedSlots = new Set(kept.map((run) => run.colorSlot));
  const colorSlot = firstFreeSlot(usedSlots);
  return [
    ...kept,
    { id: runNumber, label: `Run ${String(runNumber)}`, colorSlot, visible: true, note, result },
  ];
}

export function removeRun(runs: readonly DynoRun[], runId: number): DynoRun[] {
  return runs.filter((run) => run.id !== runId);
}

export function toggleRun(runs: readonly DynoRun[], runId: number): DynoRun[] {
  return runs.map((run) => (run.id === runId ? { ...run, visible: !run.visible } : run));
}

export interface Peak {
  value: number;
  rpm: number;
}

/** Highest value of a curve and the rpm where it occurs, e.g. peak torque at 4400 rpm. */
export function peakOf(values: readonly number[], rpm: readonly number[]): Peak | null {
  let best: Peak | null = null;
  values.forEach((value, index) => {
    const at = rpm[index];
    if (at !== undefined && (best === null || value > best.value)) {
      best = { value, rpm: at };
    }
  });
  return best;
}

function firstFreeSlot(used: ReadonlySet<number>): number {
  for (let slot = 0; slot < MAX_RUNS; slot++) {
    if (!used.has(slot)) {
      return slot;
    }
  }
  return 0;
}
