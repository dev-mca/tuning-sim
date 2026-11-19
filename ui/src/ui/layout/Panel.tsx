import type { ReactNode } from 'react';

interface PanelProps {
  title: string;
  subtitle?: string;
  actions?: ReactNode;
  children: ReactNode;
  className?: string;
}

/** Card with a header row; every area of the cockpit is one panel. */
export function Panel({ title, subtitle, actions, children, className = '' }: PanelProps) {
  return (
    <section
      aria-label={title}
      className={`flex min-w-0 flex-col gap-3 rounded-xl border border-line bg-panel p-4 ${className}`}
    >
      <header className="flex flex-wrap items-center justify-between gap-x-4 gap-y-2">
        <div className="min-w-0">
          <h2 className="text-xs font-semibold tracking-[0.2em] text-muted uppercase">{title}</h2>
          {subtitle !== undefined && <p className="truncate text-sm text-ink">{subtitle}</p>}
        </div>
        {actions}
      </header>
      {children}
    </section>
  );
}
