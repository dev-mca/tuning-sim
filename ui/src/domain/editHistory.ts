import type { CellValue, TableId } from '../protocol/messages.ts';

/** One user action on one table: the cells before and after, so it can be undone and redone. */
export interface TableEdit {
  tableId: TableId;
  before: CellValue[];
  after: CellValue[];
}

/** Older edits are dropped beyond this; a tuning session rarely needs more. */
export const DEFAULT_HISTORY_LIMIT = 200;

/**
 * Undo/redo stack (command pattern without command objects: an edit is pure data).
 * A new edit clears the redo stack, as in every editor.
 */
export class EditHistory {
  private readonly undoStack: TableEdit[] = [];
  private readonly redoStack: TableEdit[] = [];
  private readonly limit: number;

  constructor(limit = DEFAULT_HISTORY_LIMIT) {
    this.limit = limit;
  }

  record(edit: TableEdit): void {
    if (edit.after.length === 0) {
      return;
    }
    this.undoStack.push(edit);
    if (this.undoStack.length > this.limit) {
      this.undoStack.shift();
    }
    this.redoStack.length = 0;
  }

  /** Returns the edit to revert (apply its `before` cells), or undefined if there is none. */
  undo(): TableEdit | undefined {
    const edit = this.undoStack.pop();
    if (edit !== undefined) {
      this.redoStack.push(edit);
    }
    return edit;
  }

  /** Returns the edit to repeat (apply its `after` cells), or undefined if there is none. */
  redo(): TableEdit | undefined {
    const edit = this.redoStack.pop();
    if (edit !== undefined) {
      this.undoStack.push(edit);
    }
    return edit;
  }

  get canUndo(): boolean {
    return this.undoStack.length > 0;
  }

  get canRedo(): boolean {
    return this.redoStack.length > 0;
  }
}
