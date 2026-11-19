import { useState } from 'react';

import type { TableId } from '../../protocol/messages.ts';
import { useSnapshot } from '../../state/StoreContext.ts';
import { Panel } from '../layout/Panel.tsx';
import { TableEditor } from './TableEditor.tsx';

const DEFAULT_TABLE: TableId = 'ignition';

/** Tabs over all tables the simulation offers (from hello), with the editor for the active one. */
export function TablePanel() {
  const { hello, tables, originalTables } = useSnapshot();
  const [activeId, setActiveId] = useState<TableId>(DEFAULT_TABLE);
  const infos = hello?.tables ?? [];
  const active = tables.get(activeId) ?? tables.values().next().value;

  return (
    <Panel
      title="ECU tables"
      subtitle={active === undefined ? 'loading…' : `${active.name} · ${active.unit} · rpm × MAP`}
      actions={
        <div role="tablist" aria-label="Tables" className="flex flex-wrap gap-1">
          {infos.map((info) => (
            <button
              key={info.id}
              type="button"
              role="tab"
              aria-selected={info.id === active?.id}
              onClick={() => {
                setActiveId(info.id);
              }}
              className={`rounded-md px-2.5 py-1 text-xs font-semibold tracking-wide transition ${
                info.id === active?.id
                  ? 'bg-accent text-canvas'
                  : 'text-muted hover:bg-raised hover:text-ink'
              }`}
            >
              {info.id}
            </button>
          ))}
        </div>
      }
    >
      {active === undefined ? (
        <p className="py-20 text-center text-sm text-muted">Waiting for tables…</p>
      ) : (
        <TableEditor key={active.id} table={active} original={originalTables.get(active.id)} />
      )}
    </Panel>
  );
}
