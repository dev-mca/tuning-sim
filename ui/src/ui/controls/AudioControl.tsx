import { useEffect } from 'react';

import type { AudioSettings } from '../../protocol/messages.ts';
import { saveAudioPreference } from '../../state/audioPreference.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';

const VOLUME_STEPS = 100;
/** Key that toggles mute anywhere outside text fields and the table editor. */
const MUTE_KEY = 'm';

function isTypingTarget(target: EventTarget | null): boolean {
  return (
    target instanceof HTMLInputElement ||
    target instanceof HTMLSelectElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLCanvasElement
  );
}

/**
 * Volume and mute of the engine sound. simcore plays the sound itself (not the browser), so
 * this only sends setAudio; hello tells whether a sound device is available at all.
 */
export function AudioControl() {
  const store = useStore();
  const { audio, hello, sourceKind } = useSnapshot();
  const info = hello?.audio;
  const available = info?.available ?? false;
  const title = info === undefined ? 'no sound information yet' : `sound: ${info.device}`;

  const change = (next: AudioSettings): void => {
    store.setAudio(next);
    saveAudioPreference(next);
  };

  useEffect(() => {
    const onKey = (event: KeyboardEvent): void => {
      if (event.key.toLowerCase() === MUTE_KEY && !event.repeat && !isTypingTarget(event.target)) {
        const current = store.getSnapshot().audio;
        const next = { ...current, mute: !current.mute };
        store.setAudio(next);
        saveAudioPreference(next);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => {
      window.removeEventListener('keydown', onKey);
    };
  }, [store]);

  const silent = audio.mute || !available;
  return (
    <div className="flex items-center gap-2" title={title}>
      <button
        type="button"
        onClick={() => {
          change({ ...audio, mute: !audio.mute });
        }}
        aria-pressed={audio.mute}
        aria-label={audio.mute ? 'Unmute engine sound (M)' : 'Mute engine sound (M)'}
        className={`rounded-md border px-2 py-0.5 font-mono text-xs font-semibold transition ${
          silent ? 'border-line text-muted' : 'border-accent/70 text-accent'
        }`}
      >
        {audio.mute ? 'MUTED' : 'SOUND'}
      </button>
      <input
        type="range"
        min={0}
        max={VOLUME_STEPS}
        value={Math.round(audio.volume * VOLUME_STEPS)}
        aria-label="Engine sound volume"
        onChange={(event) => {
          change({ volume: Number(event.target.value) / VOLUME_STEPS, mute: false });
        }}
        className="h-1.5 w-24 cursor-pointer accent-accent"
      />
      {sourceKind === 'live' && hello !== null && !available && (
        <span className="text-warn">no sound device</span>
      )}
    </div>
  );
}
