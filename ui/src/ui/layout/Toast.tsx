import { useEffect } from 'react';

import { useSnapshot, useStore } from '../../state/StoreContext.ts';

/** Info messages disappear by themselves; errors stay until dismissed. */
const INFO_VISIBLE_MS = 4000;

/** Shows the store's current toast: an error from the simulation or a short success message. */
export function Toast() {
  const { toast } = useSnapshot();
  const store = useStore();

  useEffect(() => {
    if (toast?.tone !== 'info') {
      return;
    }
    const timer = setTimeout(() => {
      store.dismissToast();
    }, INFO_VISIBLE_MS);
    return () => {
      clearTimeout(timer);
    };
  }, [toast, store]);

  if (toast === null) {
    return null;
  }
  const error = toast.tone === 'error';
  return (
    <div
      role={error ? 'alert' : 'status'}
      className={`fixed right-4 bottom-20 z-10 flex max-w-md items-start gap-3 rounded-lg border bg-panel px-4 py-3 text-sm shadow-lg ${error ? 'border-redline/60' : 'border-good/60'}`}
    >
      <span className={error ? 'text-redline' : 'text-good'}>{error ? 'error:' : 'ok:'}</span>
      <span className="flex-1 text-ink">{toast.text}</span>
      <button
        type="button"
        onClick={() => {
          store.dismissToast();
        }}
        className="text-muted hover:text-ink"
        aria-label="Dismiss message"
      >
        ✕
      </button>
    </div>
  );
}
