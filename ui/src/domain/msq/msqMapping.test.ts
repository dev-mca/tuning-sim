import { describe, expect, it } from 'vitest';

import {
  ADVANCE_DEG,
  findConversion,
  GASOLINE_STOICH_AFR,
  LAMBDA,
  MAP_KPA,
  MSQ_TABLE_MAPPINGS,
  RPM,
} from './msqMapping.ts';

describe('findConversion', () => {
  it('converts AFR to lambda with the gasoline stoich ratio', () => {
    const conversion = findConversion(LAMBDA, 'AFR');
    expect(conversion?.convert(GASOLINE_STOICH_AFR)).toBe(1);
    expect(conversion?.convert(12.5)).toBeCloseTo(0.85, 3); // rich full-load target
  });

  it('keeps lambda, rpm and degrees as they are', () => {
    expect(findConversion(LAMBDA, 'Lambda')?.convert(0.9)).toBe(0.9);
    expect(findConversion(RPM, 'RPM')?.convert(6500)).toBe(6500);
    expect(findConversion(ADVANCE_DEG, ' deg ')?.convert(32)).toBe(32);
  });

  it('accepts only MAP in kPa as load', () => {
    expect(findConversion(MAP_KPA, 'kPa')).toBeDefined();
    expect(findConversion(MAP_KPA, 'TPS')).toBeUndefined();
    expect(findConversion(MAP_KPA, '%')).toBeUndefined();
    expect(findConversion(MAP_KPA, '')).toBeUndefined();
  });
});

describe('MSQ_TABLE_MAPPINGS', () => {
  it('maps the three editable ECU tables', () => {
    expect(MSQ_TABLE_MAPPINGS.map((mapping) => mapping.tableId)).toEqual([
      've',
      'ignition',
      'lambdaTarget',
    ]);
  });

  it('lists the Speeduino-style names first', () => {
    expect(MSQ_TABLE_MAPPINGS.map((mapping) => mapping.sources[0]?.values)).toEqual([
      'veTable',
      'advTable1',
      'afrTable',
    ]);
  });
});
