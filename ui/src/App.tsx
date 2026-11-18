const UI_VERSION = '0.1.0';

const PLANNED_PANELS = [
  { title: 'Gauges', detail: 'rpm · boost · lambda · IAT · ignition' },
  { title: 'ECU tables', detail: 'VE · lambda target · ignition (heatmap)' },
  { title: 'Dyno', detail: 'torque & power curves, run overlay' },
] as const;

export function App() {
  return (
    <main className="mx-auto flex min-h-screen max-w-4xl flex-col justify-center gap-10 px-6 py-16">
      <header className="flex flex-col gap-3">
        <p className="font-mono text-sm tracking-widest text-accent uppercase">
          engine tuning simulator
        </p>
        <h1 className="text-5xl font-bold tracking-tight sm:text-6xl">
          tuning<span className="text-redline">-</span>sim
        </h1>
        <p className="max-w-2xl text-lg text-muted">
          ECU maps, boost, dyno and physically synthesised engine sound. The cockpit UI is coming
          together step by step.
        </p>
      </header>

      <section aria-label="Planned panels" className="grid gap-4 sm:grid-cols-3">
        {PLANNED_PANELS.map((panel) => (
          <article key={panel.title} className="rounded-xl border border-line bg-panel p-5">
            <h2 className="font-semibold">{panel.title}</h2>
            <p className="mt-1 text-sm text-muted">{panel.detail}</p>
          </article>
        ))}
      </section>

      <footer className="flex items-center gap-3 font-mono text-xs text-muted">
        <span className="inline-flex items-center gap-2 rounded-full border border-line px-3 py-1">
          <span className="size-2 rounded-full bg-muted" aria-hidden="true" />
          simcore: not connected
        </span>
        <span>ui v{UI_VERSION}</span>
      </footer>
    </main>
  );
}
