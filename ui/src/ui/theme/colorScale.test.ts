import { describe, expect, it } from 'vitest';

import { normalise, parseHex, relativeLuminance, sequentialColor } from './colorScale.ts';
import { HEATMAP_RAMP } from './palette.ts';

describe('sequentialColor', () => {
  it('returns the ramp ends for 0 and 1', () => {
    expect(sequentialColor(0).fill).toBe(HEATMAP_RAMP[0]);
    expect(sequentialColor(1).fill).toBe(HEATMAP_RAMP.at(-1));
  });

  it('clamps values outside [0, 1] and survives NaN', () => {
    expect(sequentialColor(-3).fill).toBe(HEATMAP_RAMP[0]);
    expect(sequentialColor(7).fill).toBe(HEATMAP_RAMP.at(-1));
    expect(sequentialColor(Number.NaN).fill).toBe(HEATMAP_RAMP[0]);
  });

  it('gets lighter monotonically, so higher always looks like more', () => {
    let previous = -1;
    for (let t = 0; t <= 1.0001; t += 0.05) {
      const luminance = relativeLuminance(parseHex(sequentialColor(t).fill));
      expect(luminance).toBeGreaterThanOrEqual(previous);
      previous = luminance;
    }
  });

  it('switches to dark text on light cells', () => {
    expect(sequentialColor(0).text).not.toBe(sequentialColor(1).text);
  });
});

describe('normalise', () => {
  it('maps the range onto [0, 1] and a flat range to 0.5', () => {
    expect(normalise(15, 10, 20)).toBe(0.5);
    expect(normalise(10, 10, 20)).toBe(0);
    expect(normalise(3, 3, 3)).toBe(0.5);
  });
});

describe('parseHex', () => {
  it('rejects anything but #rrggbb', () => {
    expect(() => parseHex('blue')).toThrow();
  });
});
