import { useEffect, useRef } from 'react';

import { clampFrameDt } from '../domain/smoothing.ts';

/**
 * Calls `onFrame(dt_s)` once per display refresh (requestAnimationFrame) while the component
 * is mounted. The latest callback is kept in a ref, so the loop does not restart on re-render.
 * Used for everything that moves at screen rate: needles, live cursor.
 */
export function useAnimationFrame(onFrame: (dt_s: number) => void): void {
  const callback = useRef(onFrame);

  useEffect(() => {
    callback.current = onFrame;
  });

  useEffect(() => {
    let handle = 0;
    let previous: number | null = null;
    const loop = (now_ms: number): void => {
      const dt_s = previous === null ? 0 : clampFrameDt((now_ms - previous) / 1000);
      previous = now_ms;
      callback.current(dt_s);
      handle = requestAnimationFrame(loop);
    };
    handle = requestAnimationFrame(loop);
    return () => {
      cancelAnimationFrame(handle);
    };
  }, []);
}
