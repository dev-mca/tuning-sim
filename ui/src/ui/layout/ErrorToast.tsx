import { useSnapshot, useStore } from '../../state/StoreContext.ts';

/** Shows the last error the simulation sent (e.g. an invalid sweep range). */
export function ErrorToast() {
  const { lastError } = useSnapshot();
  const store = useStore();
  if (lastError === null) {
    return null;
  }
  return (
    <div
      role="alert"
      className="fixed right-4 bottom-20 z-10 flex max-w-sm items-start gap-3 rounded-lg border border-redline/60 bg-panel px-4 py-3 text-sm shadow-lg"
    >
      <span className="text-redline">simulation error:</span>
      <span className="flex-1 text-ink">{lastError}</span>
      <button
        type="button"
        onClick={() => {
          store.dismissError();
        }}
        className="text-muted hover:text-ink"
        aria-label="Dismiss error"
      >
        ✕
      </button>
    </div>
  );
}
