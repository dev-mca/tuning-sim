import { useSnapshot } from '../../state/StoreContext.ts';

/** Command that starts simcore from the repository root (CLAUDE.md, build commands). */
const START_COMMAND = './build/debug/core/simcore';

/**
 * Explains what to do while the live connection is down, instead of showing frozen gauges
 * without comment. The client keeps retrying in the background.
 */
export function ConnectionBanner({ onUseFake }: { onUseFake: () => void }) {
  const { sourceKind, status, sourceAddress } = useSnapshot();
  if (sourceKind !== 'live' || status === 'connected') {
    return null;
  }
  return (
    <div
      role="status"
      className="flex flex-wrap items-center gap-x-4 gap-y-2 rounded-xl border border-warn/50 bg-warn/10 px-4 py-3 text-sm"
    >
      <span className="size-2 animate-pulse rounded-full bg-warn" aria-hidden="true" />
      <p className="flex-1">
        simcore is not reachable at <code className="font-mono text-ink">{sourceAddress}</code>.
        Start it from the repository root with{' '}
        <code className="rounded bg-canvas px-1.5 py-0.5 font-mono text-ink">{START_COMMAND}</code>;
        the UI reconnects by itself.
      </p>
      <button
        type="button"
        onClick={onUseFake}
        className="rounded-md border border-line bg-raised px-3 py-1 text-sm hover:border-muted"
      >
        Use fake data instead
      </button>
    </div>
  );
}
