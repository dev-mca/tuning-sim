import { describe, expect, it } from 'vitest';

import { loadAudioPreference, parseAudioSettings } from './audioPreference.ts';
import { DEFAULT_AUDIO } from './simStore.ts';

describe('audio preference', () => {
  it('parses stored settings and clamps the volume', () => {
    expect(parseAudioSettings('{"volume":0.4,"mute":true}')).toEqual({ volume: 0.4, mute: true });
    expect(parseAudioSettings('{"volume":3,"mute":false}')).toEqual({ volume: 1, mute: false });
  });

  it('falls back to the default for broken values', () => {
    expect(parseAudioSettings('nope')).toEqual(DEFAULT_AUDIO);
    expect(parseAudioSettings('{"volume":"loud","mute":false}')).toEqual(DEFAULT_AUDIO);
    expect(parseAudioSettings('{"volume":0.5}')).toEqual(DEFAULT_AUDIO);
  });

  it('uses the default when storage is unavailable (no window in tests)', () => {
    expect(loadAudioPreference()).toEqual(DEFAULT_AUDIO);
  });
});
