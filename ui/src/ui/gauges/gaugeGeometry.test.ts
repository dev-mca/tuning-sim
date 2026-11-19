import { describe, expect, it } from 'vitest';

import { arcPath, type DialRange, polarToCartesian, ticks, valueToAngle } from './gaugeGeometry.ts';

const dial: DialRange = { min: 0, max: 8000, startAngle_deg: -135, sweep_deg: 270 };

describe('valueToAngle', () => {
  it('maps min, middle and max onto the dial', () => {
    expect(valueToAngle(0, dial)).toBe(-135);
    expect(valueToAngle(4000, dial)).toBe(0);
    expect(valueToAngle(8000, dial)).toBe(135);
  });

  it('clamps values outside the scale', () => {
    expect(valueToAngle(-500, dial)).toBe(-135);
    expect(valueToAngle(9000, dial)).toBe(135);
  });
});

describe('polarToCartesian', () => {
  it('puts 0 deg at 12 o clock and 90 deg at 3 o clock (SVG y grows downwards)', () => {
    const top = polarToCartesian({ x: 50, y: 50 }, 10, 0);
    expect(top.x).toBeCloseTo(50, 9);
    expect(top.y).toBeCloseTo(40, 9);
    const right = polarToCartesian({ x: 50, y: 50 }, 10, 90);
    expect(right.x).toBeCloseTo(60, 9);
    expect(right.y).toBeCloseTo(50, 9);
  });
});

describe('arcPath', () => {
  it('uses the large-arc flag for arcs over 180 deg', () => {
    expect(arcPath({ x: 0, y: 0 }, 1, -135, 135)).toContain(' 0 1 1 ');
    expect(arcPath({ x: 0, y: 0 }, 1, 0, 90)).toContain(' 0 0 1 ');
  });
});

describe('ticks', () => {
  it('creates major and minor ticks', () => {
    const result = ticks(0, 2000, 1000, 2);
    expect(result).toEqual([
      { value: 0, major: true },
      { value: 500, major: false },
      { value: 1000, major: true },
      { value: 1500, major: false },
      { value: 2000, major: true },
    ]);
  });

  it('keeps decimal steps exact', () => {
    expect(ticks(0.7, 1.3, 0.1, 1).map((tick) => tick.value)).toEqual([
      0.7, 0.8, 0.9, 1, 1.1, 1.2, 1.3,
    ]);
  });

  it('returns nothing for an invalid range', () => {
    expect(ticks(10, 0, 1, 1)).toEqual([]);
    expect(ticks(0, 10, 0, 1)).toEqual([]);
  });
});
