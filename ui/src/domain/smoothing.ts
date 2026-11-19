/**
 * First-order low-pass ("exponential smoothing") for gauge needles.
 * State arrives at 60 Hz but the screen may refresh at 144 Hz; without smoothing the needle
 * would jump. The factor 1 - exp(-dt/tau) makes the result independent of the frame rate:
 * after tau seconds the needle has covered 63 % of the way to the target.
 */
export function smoothTowards(
  current: number,
  target: number,
  dt_s: number,
  timeConstant_s: number,
): number {
  if (dt_s <= 0) {
    return current;
  }
  if (timeConstant_s <= 0) {
    return target;
  }
  const alpha = 1 - Math.exp(-dt_s / timeConstant_s);
  return current + (target - current) * alpha;
}

/**
 * Frames can be very long (tab in background, debugger pause). Limiting dt avoids
 * a visible jump of half a second when the tab becomes active again.
 */
export const MAX_FRAME_DT_S = 0.1;

export function clampFrameDt(dt_s: number): number {
  return Math.min(Math.max(dt_s, 0), MAX_FRAME_DT_S);
}
