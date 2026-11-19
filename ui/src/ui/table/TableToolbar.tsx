import { type SyntheticEvent, useState } from 'react';

import { LARGE_STEP_FACTOR } from '../../domain/tableEdit.ts';
import type { TableData } from '../../protocol/messages.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { Button } from '../controls/Button.tsx';

interface TableToolbarProps {
  table: TableData;
  step: number;
  actions: {
    adjust: (delta: number) => void;
    smooth: () => void;
    interpolate: () => void;
    set: (value: number) => void;
  };
}

/** Buttons for every editor action; each one also has a keyboard shortcut (shown as tooltip). */
export function TableToolbar({ table, step, actions }: TableToolbarProps) {
  const store = useStore();
  const { canUndo, canRedo } = useSnapshot();
  const [setValue, setSetValue] = useState('');
  const readOnly = !table.editable;
  const large = step * LARGE_STEP_FACTOR;

  const onSet = (event: SyntheticEvent): void => {
    event.preventDefault();
    const value = Number(setValue.replace(',', '.'));
    if (setValue.trim() !== '' && Number.isFinite(value)) {
      actions.set(value);
    }
  };

  return (
    <div className="flex flex-wrap items-center gap-1.5">
      <Button
        title={`−${String(large)} (PageDown)`}
        disabled={readOnly}
        onClick={() => {
          actions.adjust(-large);
        }}
      >
        −−
      </Button>
      <Button
        title={`−${String(step)} (-)`}
        disabled={readOnly}
        onClick={() => {
          actions.adjust(-step);
        }}
      >
        −
      </Button>
      <Button
        title={`+${String(step)} (+)`}
        disabled={readOnly}
        onClick={() => {
          actions.adjust(step);
        }}
      >
        +
      </Button>
      <Button
        title={`+${String(large)} (PageUp)`}
        disabled={readOnly}
        onClick={() => {
          actions.adjust(large);
        }}
      >
        ++
      </Button>
      <span className="mx-1 h-5 w-px bg-line" aria-hidden="true" />
      <Button title="Smooth selection, 3×3 mean (S)" disabled={readOnly} onClick={actions.smooth}>
        Smooth
      </Button>
      <Button
        title="Interpolate between the selection's corners (I)"
        disabled={readOnly}
        onClick={actions.interpolate}
      >
        Interpolate
      </Button>
      <form onSubmit={onSet} className="flex items-center gap-1">
        <input
          value={setValue}
          onChange={(event) => {
            setSetValue(event.target.value);
          }}
          disabled={readOnly}
          inputMode="decimal"
          placeholder="value"
          aria-label="Value to set"
          className="h-8 w-16 rounded-md border border-line bg-canvas px-2 font-mono text-sm text-ink placeholder:text-muted/60 focus:border-accent focus:outline-none disabled:opacity-40"
        />
        <Button type="submit" disabled={readOnly} title="Set all selected cells to this value">
          Set
        </Button>
      </form>
      <span className="mx-1 h-5 w-px bg-line" aria-hidden="true" />
      <Button
        title="Undo (Ctrl+Z)"
        disabled={!canUndo}
        onClick={() => {
          store.undo();
        }}
      >
        Undo
      </Button>
      <Button
        title="Redo (Ctrl+Y)"
        disabled={!canRedo}
        onClick={() => {
          store.redo();
        }}
      >
        Redo
      </Button>
      {readOnly && (
        <span className="ml-auto rounded border border-line px-2 py-0.5 text-xs text-muted">
          read-only (engine property)
        </span>
      )}
    </div>
  );
}
