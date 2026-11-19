import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { singleCell } from '../domain/selection.ts';
import { adjustCells } from '../domain/tableEdit.ts';
import { FakeSimSource } from '../sim/fake/FakeSimSource.ts';
import { SimStore } from './simStore.ts';

/** The store is tested together with the real fake source: both are plain TypeScript. */
function startedStore(): SimStore {
  const store = new SimStore(new FakeSimSource({ seed: 3 }));
  store.start();
  // Fake timers run a setTimeout(0) that is scheduled inside another timer only after 1 ms.
  vi.advanceTimersByTime(1); // hello
  vi.advanceTimersByTime(1); // getTable answers
  return store;
}

describe('SimStore', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('loads all tables after hello', () => {
    const store = startedStore();
    const snapshot = store.getSnapshot();
    expect(snapshot.status).toBe('connected');
    expect(snapshot.hello?.engine.cylinders).toBe(6);
    expect([...snapshot.tables.keys()]).toEqual(['ve', 'lambdaTarget', 'ignition', 'ignitionMbt']);
  });

  it('keeps the 60 Hz state out of the React snapshot', () => {
    const store = startedStore();
    const listener = vi.fn();
    store.subscribe(listener);
    vi.advanceTimersByTime(1000);
    expect(store.latestState()?.rpm).toBeGreaterThan(0);
    expect(listener.mock.calls.length).toBeLessThan(5);
  });

  it('edits a table optimistically, sends it and can undo and redo', () => {
    const store = startedStore();
    const before = store.getSnapshot().tables.get('ve')?.values[0]?.[0];
    store.editTable('ve', [{ x: 0, y: 0, value: 70 }]);
    expect(store.getSnapshot().tables.get('ve')?.values[0]?.[0]).toBe(70);
    expect(store.getSnapshot().canUndo).toBe(true);

    store.undo();
    expect(store.getSnapshot().tables.get('ve')?.values[0]?.[0]).toBe(before);
    expect(store.getSnapshot().canRedo).toBe(true);

    store.redo();
    expect(store.getSnapshot().tables.get('ve')?.values[0]?.[0]).toBe(70);
    expect(store.getSnapshot().originalTables.get('ve')?.values[0]?.[0]).toBe(before);
  });

  it('computes each edit from the current table, so quick edits add up', () => {
    const store = startedStore();
    const before = store.getSnapshot().tables.get('ve')?.values[0]?.[0] ?? 0;
    const selection = singleCell({ x: 0, y: 0 });
    store.editTableWith('ve', (table) => adjustCells(table, selection, 0.5));
    store.editTableWith('ve', (table) => adjustCells(table, selection, 0.5));
    expect(store.getSnapshot().tables.get('ve')?.values[0]?.[0]).toBe(before + 1);
  });

  it('turns a finished sweep into a dyno run', () => {
    const store = startedStore();
    store.startSweep({ rpmFrom: 2000, rpmTo: 2500, rate_rpm_s: 1000 });
    vi.advanceTimersByTime(300);
    expect(store.getSnapshot().sweep.active).toBe(true);
    vi.advanceTimersByTime(500);
    const snapshot = store.getSnapshot();
    expect(snapshot.sweep.active).toBe(false);
    expect(snapshot.runs).toHaveLength(1);
    expect(snapshot.runs[0]?.result.rpm[0]).toBe(2000);
  });

  it('shows errors from the simulation', () => {
    const store = startedStore();
    store.startSweep({ rpmFrom: 3000, rpmTo: 1000 });
    vi.advanceTimersByTime(0);
    expect(store.getSnapshot().toast?.text).toMatch(/sweep range/);
    store.dismissToast();
    expect(store.getSnapshot().toast).toBeNull();
  });
});
