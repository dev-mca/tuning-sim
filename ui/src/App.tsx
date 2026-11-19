import { useEffect, useState } from 'react';

import { FakeSimSource } from './sim/fake/FakeSimSource.ts';
import { SimStore } from './state/simStore.ts';
import { StoreContext, useSnapshot } from './state/StoreContext.ts';
import { ControlBar } from './ui/controls/ControlBar.tsx';
import { DynoPanel } from './ui/dyno/DynoPanel.tsx';
import { GaugeCluster } from './ui/gauges/GaugeCluster.tsx';
import { ErrorToast } from './ui/layout/ErrorToast.tsx';
import { TablePanel } from './ui/table/TablePanel.tsx';

/**
 * Composition root: the only place that decides which SimSource the UI uses.
 * Iteration 6 adds the WebSocket client here and a switch between fake and live.
 */
export function App() {
  const [store] = useState(() => new SimStore(new FakeSimSource()));

  useEffect(() => {
    store.start();
    return () => {
      store.stop();
    };
  }, [store]);

  return (
    <StoreContext value={store}>
      <Cockpit />
    </StoreContext>
  );
}

/** Layout from plan.md §7: gauges on top, table editor left, dyno right, controls at the bottom. */
function Cockpit() {
  const { hello } = useSnapshot();
  return (
    <div className="mx-auto flex min-h-screen max-w-[110rem] flex-col gap-4 p-4">
      <header className="flex flex-wrap items-baseline justify-between gap-2 px-1">
        <h1 className="text-xl font-bold tracking-tight">
          tuning<span className="text-redline">-</span>sim
        </h1>
        <p className="text-sm text-muted">
          {hello === null
            ? 'connecting…'
            : `${hello.engine.name} · ${hello.engine.displacement_l.toFixed(3)} l · simcore ${hello.simcore}`}
        </p>
      </header>
      <GaugeCluster />
      <main className="grid flex-1 grid-cols-1 gap-4 xl:grid-cols-2">
        <TablePanel />
        <DynoPanel />
      </main>
      <ControlBar />
      <ErrorToast />
    </div>
  );
}
