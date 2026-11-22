/**
 * End-to-end test against a running simcore (acceptance criterion of iteration 6):
 * change the ignition table, start a sweep and see a different torque curve.
 *
 *   ./build/debug/core/simcore --no-audio &   # from the repository root
 *   cd ui && npm run test:e2e             # VITE_SIMCORE_URL overrides ws://127.0.0.1:47800
 *
 * It uses the real WebSocketSimSource and SimStore, i.e. the same code as the browser
 * (Node 22 has a built-in WebSocket client).
 */
import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { selectAll } from '../domain/selection.ts';
import { adjustCells } from '../domain/tableEdit.ts';
import { simcoreUrl } from '../sim/createSource.ts';
import { WebSocketSimSource } from '../sim/live/WebSocketSimSource.ts';
import { peakOf } from '../state/dynoRuns.ts';
import { SimStore } from '../state/simStore.ts';

const URL = simcoreUrl();
const SWEEP = { rpmFrom: 2000, rpmTo: 4500, rate_rpm_s: 1000, pedal: 1 };
/** 5 deg later ignition costs a few percent of torque (research.md §2.7). */
const RETARD_DEG = -5;
const POLL_MS = 50;
const TIMEOUT_MS = 20_000;

async function waitFor<T>(what: string, read: () => T | undefined | null | false): Promise<T> {
  const deadline = Date.now() + TIMEOUT_MS;
  for (;;) {
    const value = read();
    if (value !== undefined && value !== null && value !== false) {
      return value;
    }
    if (Date.now() > deadline) {
      throw new Error(`timed out waiting for ${what} (is simcore running at ${URL}?)`);
    }
    await new Promise((resolve) => setTimeout(resolve, POLL_MS));
  }
}

describe('UI store against a live simcore', () => {
  const store = new SimStore(new WebSocketSimSource({ url: URL }));

  beforeAll(async () => {
    store.start();
    await waitFor('all tables', () => {
      const snapshot = store.getSnapshot();
      return snapshot.hello !== null && snapshot.tables.size === snapshot.hello.tables.length;
    });
    // Start from the delivered tune, even if an earlier run left simcore modified.
    store.revertToOriginal();
  });

  afterAll(() => {
    store.revertToOriginal();
    store.stop();
  });

  it('is told whether simcore plays sound', () => {
    const audio = store.getSnapshot().hello?.audio;
    expect(audio).toBeDefined();
    expect(typeof audio?.device).toBe('string');
  });

  it('receives state at about 60 Hz', async () => {
    const first = await waitFor('state', () => store.latestState());
    await new Promise((resolve) => setTimeout(resolve, 500));
    const later = await waitFor('newer state', () => store.latestState());
    expect(later.t_s - first.t_s).toBeGreaterThan(0.3);
    expect(later.rpm).toBeGreaterThan(500);
  });

  it(
    'shows less torque after retarding the ignition',
    async () => {
      store.startSweep(SWEEP);
      const baseline = await waitFor('run 1', () => store.getSnapshot().runs[0]);

      store.editTableWith('ignition', (table) =>
        adjustCells(table, selectAll(table.xAxis.bins.length, table.yAxis.bins.length), RETARD_DEG),
      );
      // One socket keeps the order: simcore applies the edit before it starts the sweep.
      store.startSweep(SWEEP);
      const retarded = await waitFor('run 2', () => store.getSnapshot().runs[1]);

      const peak = (run: typeof baseline) =>
        peakOf(run.result.torque_Nm, run.result.rpm)?.value ?? 0;
      expect(baseline.note).toBe('original tune');
      expect(retarded.note).toMatch(/^ignition: \d+ cells, mean −5\.0 deg BTDC$/);
      expect(peak(retarded)).toBeLessThan(peak(baseline) * 0.99);
      expect(peak(retarded)).toBeGreaterThan(peak(baseline) * 0.85);
    },
    2 * TIMEOUT_MS,
  );
});
