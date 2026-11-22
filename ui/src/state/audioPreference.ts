import type { AudioSettings } from '../protocol/messages.ts';
import { DEFAULT_AUDIO } from './simStore.ts';

const STORAGE_KEY = 'tuning-sim.audio';

/**
 * The volume the user chose last time in this browser. Storage can be blocked (private mode),
 * so every access is guarded and broken values fall back to the default.
 */
export function loadAudioPreference(): AudioSettings {
  try {
    const stored = window.localStorage.getItem(STORAGE_KEY);
    return stored === null ? DEFAULT_AUDIO : parseAudioSettings(stored);
  } catch {
    return DEFAULT_AUDIO;
  }
}

export function saveAudioPreference(audio: AudioSettings): void {
  try {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(audio));
  } catch {
    // Not remembering the volume is acceptable.
  }
}

export function parseAudioSettings(text: string): AudioSettings {
  try {
    const data: unknown = JSON.parse(text);
    if (typeof data === 'object' && data !== null && 'volume' in data && 'mute' in data) {
      const { volume, mute } = data;
      if (typeof volume === 'number' && Number.isFinite(volume) && typeof mute === 'boolean') {
        return { volume: Math.min(Math.max(volume, 0), 1), mute };
      }
    }
  } catch {
    // fall through to the default
  }
  return DEFAULT_AUDIO;
}
