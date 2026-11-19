import { describe, expect, it } from 'vitest';

import { formatClean, kwToPs, lambdaToAfr, mapToBoostBar } from './units.ts';

describe('display units', () => {
  it('converts lambda to gasoline AFR', () => {
    expect(lambdaToAfr(1)).toBe(14.7);
  });

  it('shows vacuum as negative and boost as positive bar', () => {
    expect(mapToBoostBar(101.325)).toBe(0);
    expect(mapToBoostBar(201.325)).toBeCloseTo(1, 9);
    expect(mapToBoostBar(30)).toBeLessThan(0);
  });

  it('converts kW to metric horsepower', () => {
    expect(kwToPs(73.549875)).toBeCloseTo(100, 9);
  });

  it('removes floating-point noise from axis values', () => {
    expect(formatClean(1100.0000000000002)).toBe('1100');
    expect(formatClean(0.30000000000000004)).toBe('0.3');
    expect(formatClean(4602.877606907298, 0)).toBe('4603');
  });
});
