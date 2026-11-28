import { useEffect, useMemo, useState } from 'react';

import { createSource, initialSourceKind, rememberSourceKind } from './sim/createSource.ts';
import type { SourceKind } from './sim/SimSource.ts';
import { loadAudioPreference } from './state/audioPreference.ts';
import { SimStore } from './state/simStore.ts';
import { StoreContext, useSnapshot } from './state/StoreContext.ts';
import { ControlBar } from './ui/controls/ControlBar.tsx';
import { TuneMenu } from './ui/controls/TuneMenu.tsx';
import { DynoPanel } from './ui/dyno/DynoPanel.tsx';
import { GaugeCluster } from './ui/gauges/GaugeCluster.tsx';
import { ConnectionBanner } from './ui/layout/ConnectionBanner.tsx';
import { Toast } from './ui/layout/Toast.tsx';
import { Map3dPanel } from './ui/map3d/Map3dPanel.tsx';
import { LaunchPanel } from './ui/launch/LaunchPanel.tsx';
import { OverrunPanel } from './ui/overrun/OverrunPanel.tsx';
import { TablePanel } from './ui/table/TablePanel.tsx';

/**
 * Composition root: the only place that decides which SimSource the UI uses. Switching the
 * source builds a fresh store, so nothing of the old connection (tables, runs) leaks over.
 */
export function App() {
  const [sourceKind, setSourceKind] = useState<SourceKind>(() => initialSourceKind());
  const store = useMemo(
    () => new SimStore(createSource(sourceKind), loadAudioPreference()),
    [sourceKind],
  );

  useEffect(() => {
    store.start();
    return () => {
      store.stop();
    };
  }, [store]);

  const changeSource = (kind: SourceKind): void => {
    rememberSourceKind(kind);
    setSourceKind(kind);
  };

  return (
    <StoreContext value={store}>
      <Cockpit onSourceChange={changeSource} />
    </StoreContext>
  );
}

/**
 * Layout from plan.md §7: gauges on top, table editor left, dyno and 3D map right, controls at
 * the bottom.
 */
function Cockpit({ onSourceChange }: { onSourceChange: (kind: SourceKind) => void }) {
  const { hello } = useSnapshot();
  return (
    <div className="mx-auto flex min-h-screen max-w-[110rem] flex-col gap-4 p-4">
      <header className="flex flex-wrap items-center justify-between gap-x-6 gap-y-2 px-1">
        <div className="flex flex-wrap items-baseline gap-x-4">
          <h1 className="text-xl font-bold tracking-tight">
            tuning<span className="text-redline">-</span>sim
          </h1>
          <p className="text-sm text-muted">
            {hello === null
              ? 'no engine'
              : `${hello.engine.name} · ${hello.engine.displacement_l.toFixed(3)} l · simcore ${hello.simcore}`}
          </p>
        </div>
        <TuneMenu />
      </header>
      <ConnectionBanner
        onUseFake={() => {
          onSourceChange('fake');
        }}
      />
      <GaugeCluster />
      <main className="grid flex-1 grid-cols-1 gap-4 xl:grid-cols-2">
        <div className="flex min-w-0 flex-col gap-4">
          <TablePanel />
          <OverrunPanel />
          <LaunchPanel />
        </div>
        <div className="flex min-w-0 flex-col gap-4">
          <DynoPanel />
          <Map3dPanel />
        </div>
      </main>
      <ControlBar onSourceChange={onSourceChange} />
      <Toast />
    </div>
  );
}
