import { createContext, useContext, useSyncExternalStore } from 'react';

import type { SimSnapshot, SimStore } from './simStore.ts';

/**
 * Makes the one SimStore available to all components without passing it through every prop
 * (dependency injection the React way; App creates the store and provides it).
 */
export const StoreContext = createContext<SimStore | null>(null);

export function useStore(): SimStore {
  const store = useContext(StoreContext);
  if (store === null) {
    throw new Error('useStore must be used inside <StoreContext value={store}>');
  }
  return store;
}

/**
 * Subscribes a component to the store snapshot. React re-renders it only when the snapshot
 * object changes, which happens on discrete events, not on every 60 Hz state message.
 */
export function useSnapshot(): SimSnapshot {
  const store = useStore();
  return useSyncExternalStore(store.subscribe, store.getSnapshot);
}
