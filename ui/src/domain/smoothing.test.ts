import { describe, expect, it } from 'vitest';

import { clampFrameDt, MAX_FRAME_DT_S, smoothTowards } from './smoothing.ts';

describe('smoothTowards', () => {
  it('covers 63 % of the way after one time constant', () => {
    expect(smoothTowards(0, 100, 0.2, 0.2)).toBeCloseTo(100 * (1 - Math.exp(-1)), 9);
  });

  it('does not depend on the frame rate', () => {
    let at60Hz = 0;
    for (let i = 0; i < 60; i++) at60Hz = smoothTowards(at60Hz, 1, 1 / 60, 0.1);
    let at144Hz = 0;
    for (let i = 0; i < 144; i++) at144Hz = smoothTowards(at144Hz, 1, 1 / 144, 0.1);
    expect(at60Hz).toBeCloseTo(at144Hz, 9);
  });

  it('never overshoots the target', () => {
    expect(smoothTowards(0, 1, 10, 0.1)).toBeLessThanOrEqual(1);
  });

  it('jumps to the target without smoothing and stays put without time', () => {
    expect(smoothTowards(0, 5, 0.016, 0)).toBe(5);
    expect(smoothTowards(3, 5, 0, 0.1)).toBe(3);
  });
});

describe('clampFrameDt', () => {
  it('limits long frames and negative times', () => {
    expect(clampFrameDt(2)).toBe(MAX_FRAME_DT_S);
    expect(clampFrameDt(-1)).toBe(0);
    expect(clampFrameDt(0.016)).toBe(0.016);
  });
});
