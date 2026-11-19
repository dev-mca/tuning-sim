import type { CellValue, TableId } from '../protocol/messages.ts';

/** The cells of one table before and after a change, so it can be undone and redone. */
export interface TableEdit {
  tableId: TableId;
  before: CellValue[];
  after: CellValue[];
}

/**
 * One user action, e.g. "+0.5 on the selection" (one table) or "load tune" (several tables).
 * Undo reverts the whole step at once.
 */
export type EditStep = readonly TableEdit[];

/** Older steps are dropped beyond this; a tuning session rarely needs more. */
export const DEFAULT_HISTORY_LIMIT = 200;

/**
 * Undo/redo stack (command pattern without command objects: an edit is pure data).
 * A new step clears the redo stack, as in every editor.
 */
export class EditHistory {
  private readonly undoStack: EditStep[] = [];
  private readonly redoStack: EditStep[] = [];
  private readonly limit: number;

  constructor(limit = DEFAULT_HISTORY_LIMIT) {
    this.limit = limit;
  }

  record(step: EditStep): void {
    const effective = step.filter((edit) => edit.after.length > 0);
    if (effective.length === 0) {
      return;
    }
    this.undoStack.push(effective);
    if (this.undoStack.length > this.limit) {
      this.undoStack.shift();
    }
    this.redoStack.length = 0;
  }

  /** Returns the step to revert (apply each `before`), or undefined if there is none. */
  undo(): EditStep | undefined {
    const step = this.undoStack.pop();
    if (step !== undefined) {
      this.redoStack.push(step);
    }
    return step;
  }

  /** Returns the step to repeat (apply each `after`), or undefined if there is none. */
  redo(): EditStep | undefined {
    const step = this.redoStack.pop();
    if (step !== undefined) {
      this.undoStack.push(step);
    }
    return step;
  }

  get canUndo(): boolean {
    return this.undoStack.length > 0;
  }

  get canRedo(): boolean {
    return this.redoStack.length > 0;
  }
}
