// @vitest-environment jsdom
// Node has no DOMParser; jsdom provides the same API the browser uses (ADR 0012).
import { describe, expect, it } from 'vitest';

import speeduinoStyle from './fixtures/speeduino-style-4x4.msq?raw';
import { constant, msqXml } from './msqTestXml.ts';
import { MsqError, parseMsq } from './parseMsq.ts';

describe('parseMsq', () => {
  it('reads table and axis constants with their dimensions and units', () => {
    const document = parseMsq(speeduinoStyle);
    const ve = document.constants.get('veTable');
    expect(ve).toMatchObject({ units: '%', rows: 4, cols: 4 });
    expect(ve?.values?.slice(0, 5)).toEqual([40, 50, 60, 55, 55]);
    expect(document.constants.get('rpmBins')?.values).toEqual([1000, 3000, 5000, 7000]);
    expect(document.signature).toBe('synthetic-speeduino-style');
  });

  it('treats constants without rows/cols as scalars', () => {
    const stoich = parseMsq(speeduinoStyle).constants.get('stoich');
    expect(stoich).toMatchObject({ rows: 1, cols: 1, units: 'AFR', values: [14.7] });
  });

  it('keeps string and option constants but without numbers', () => {
    const document = parseMsq(speeduinoStyle);
    expect(document.constants.get('sparkMode')?.values).toBeNull();
    expect(document.constants.get('nCylinders')?.values).toBeNull(); // "6" is a quoted string
  });

  it('works without the msq namespace and keeps the first of duplicate constants', () => {
    const xml =
      '<msq><page><constant name="a">1</constant></page>' +
      '<page><constant name="a">2</constant></page></msq>';
    expect(parseMsq(xml).constants.get('a')?.values).toEqual([1]);
    expect(parseMsq(xml).signature).toBe('');
  });

  it('rejects text that is not XML', () => {
    expect(() => parseMsq('{"format": "tuning-sim-tune"}')).toThrow(MsqError);
    expect(() => parseMsq('<msq><page></msq>')).toThrow(/not well-formed XML/);
  });

  it('rejects XML whose root is not <msq>', () => {
    expect(() => parseMsq('<tune><constant name="a">1</constant></tune>')).toThrow(
      /not a TunerStudio .msq/,
    );
  });

  it('rejects constants without a name or with invalid dimensions', () => {
    expect(() => parseMsq(msqXml([{ name: '', content: '1' }]))).toThrow(/has no name/);
    expect(() => parseMsq(msqXml([{ name: 'a', rows: 0, content: '1' }]))).toThrow(
      /'a' has an invalid rows value '0'/,
    );
  });

  it('handles values separated by any whitespace', () => {
    const xml = msqXml([constant('t', '%', [[1, 2]])]).replace('1 2', '\n\t1\t\n  2  \n');
    expect(parseMsq(xml).constants.get('t')?.values).toEqual([1, 2]);
  });
});
