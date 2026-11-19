import { describe, expect, it } from 'vitest';

import { EditHistory, type TableEdit } from './editHistory.ts';

function edit(value: number): TableEdit {
  return {
    tableId: 've',
    before: [{ x: 0, y: 0, value: value - 1 }],
    after: [{ x: 0, y: 0, value }],
  };
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
    history.record(edit(1));
    history.record(edit(2));
    expect(history.undo()).toEqual(edit(2));
    expect(history.undo()).toEqual(edit(1));
    expect(history.redo()).toEqual(edit(1));
    expect(history.redo()).toEqual(edit(2));
  });

  it('clears the redo stack when a new edit is recorded', () => {
    const history = new EditHistory();
    history.record(edit(1));
    history.undo();
    history.record(edit(5));
    expect(history.canRedo).toBe(false);
  });

  it('ignores edits that change nothing', () => {
    const history = new EditHistory();
    history.record({ tableId: 've', before: [], after: [] });
    expect(history.canUndo).toBe(false);
  });

  it('drops the oldest edit beyond the limit', () => {
    const history = new EditHistory(2);
    history.record(edit(1));
    history.record(edit(2));
    history.record(edit(3));
    expect(history.undo()).toEqual(edit(3));
    expect(history.undo()).toEqual(edit(2));
    expect(history.undo()).toBeUndefined();
  });
});
