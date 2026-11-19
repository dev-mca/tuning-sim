import { describe, expect, it } from 'vitest';

import { EditHistory, type EditStep, type TableEdit } from './editHistory.ts';

function edit(value: number, tableId = 've'): TableEdit {
  return {
    tableId,
    before: [{ x: 0, y: 0, value: value - 1 }],
    after: [{ x: 0, y: 0, value }],
  };
}

/** A step with a single table edit, the common case. */
function step(value: number): EditStep {
  return [edit(value)];
}

describe('EditHistory', () => {
  it('starts empty', () => {
    const history = new EditHistory();
    expect(history.canUndo).toBe(false);
    expect(history.canRedo).toBe(false);
    expect(history.undo()).toBeUndefined();
    expect(history.redo()).toBeUndefined();
  });

  it('undoes in reverse order and redoes in original order', () => {
    const history = new EditHistory();
    history.record(step(1));
    history.record(step(2));
    expect(history.undo()).toEqual(step(2));
    expect(history.undo()).toEqual(step(1));
    expect(history.redo()).toEqual(step(1));
    expect(history.redo()).toEqual(step(2));
  });

  it('clears the redo stack when a new edit is recorded', () => {
    const history = new EditHistory();
    history.record(step(1));
    history.undo();
    history.record(step(5));
    expect(history.canRedo).toBe(false);
  });

  it('ignores edits that change nothing', () => {
    const history = new EditHistory();
    history.record([{ tableId: 've', before: [], after: [] }]);
    expect(history.canUndo).toBe(false);
  });

  it('undoes a step over several tables at once', () => {
    const history = new EditHistory();
    const loadTune: EditStep = [edit(1, 've'), edit(2, 'ignition')];
    history.record(loadTune);
    expect(history.undo()).toEqual(loadTune);
    expect(history.canUndo).toBe(false);
  });

  it('drops the oldest edit beyond the limit', () => {
    const history = new EditHistory(2);
    history.record(step(1));
    history.record(step(2));
    history.record(step(3));
    expect(history.undo()).toEqual(step(3));
    expect(history.undo()).toEqual(step(2));
    expect(history.undo()).toBeUndefined();
  });
});
