import { describe, expect, it } from 'vitest';

import { createSource, initialSourceKind } from './createSource.ts';

describe('initialSourceKind', () => {
  it('takes the source from the URL', () => {
    expect(initialSourceKind('?source=fake')).toBe('fake');
    expect(initialSourceKind('?source=live')).toBe('live');
  });

  it('defaults to live for missing or unknown values (no storage in tests)', () => {
    expect(initialSourceKind('')).toBe('live');
    expect(initialSourceKind('?source=other')).toBe('live');
  });
});

describe('createSource', () => {
  it('creates the implementation for each kind', () => {
    expect(createSource('fake').kind).toBe('fake');
    expect(createSource('live').address).toBe('ws://127.0.0.1:47800');
  });
});
