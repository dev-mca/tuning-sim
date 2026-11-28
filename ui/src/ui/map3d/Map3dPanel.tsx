import { lazy, Suspense, useState } from 'react';

import type { TableId } from '../../protocol/messages.ts';
import { useSnapshot } from '../../state/StoreContext.ts';
import { Panel } from '../layout/Panel.tsx';
import { SceneErrorBoundary } from './SceneErrorBoundary.tsx';

/**
 * three.js and react-three-fiber are loaded only when this panel mounts, as their own chunk,
 * so they do not slow down the first paint of the cockpit.
 */
const SurfaceScene = lazy(() =>
  import('./SurfaceScene.tsx').then((module) => ({ default: module.SurfaceScene })),
);

/** Same start table as the 2D editor. */
const DEFAULT_TABLE: TableId = 'ignition';

/** Read-only 3D surface of one ECU table over rpm × MAP, with the live operating point. */
export function Map3dPanel() {
  const { hello, tables } = useSnapshot();
  const [activeId, setActiveId] = useState<TableId>(DEFAULT_TABLE);
  const infos = hello?.tables ?? [];
  const active = tables.get(activeId) ?? tables.values().next().value;

  return (
    <Panel
      title="3D map"
      subtitle={
        active === undefined
          ? 'loading…'
          : `${active.name} · drag to orbit, wheel to zoom, right-drag to pan`
      }
      actions={
        <div role="tablist" aria-label="3D table" className="flex flex-wrap gap-1">
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
      <div className="h-[26rem] w-full overflow-hidden rounded-md">
        {active === undefined ? (
          <p className="py-20 text-center text-sm text-muted">Waiting for tables…</p>
        ) : (
          <SceneErrorBoundary>
            <Suspense
              fallback={<p className="py-20 text-center text-sm text-muted">Loading 3D…</p>}
            >
              <SurfaceScene table={active} />
            </Suspense>
          </SceneErrorBoundary>
        )}
      </div>
    </Panel>
  );
}
