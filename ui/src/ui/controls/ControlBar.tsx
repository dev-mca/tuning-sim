import { useEffect, useRef, useState } from 'react';

import type { ConnectionStatus, SourceKind } from '../../sim/SimSource.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { isTypingTarget } from '../keyboard.ts';
import { useAnimationFrame } from '../useAnimationFrame.ts';
import { AudioControl } from './AudioControl.tsx';

const PEDAL_STEPS = 100;
/** Key that floors the pedal while held, to blip the throttle. */
const BLIP_KEY = ' ';

const STATUS_STYLE: Record<ConnectionStatus, { dot: string; text: string }> = {
  connected: { dot: 'bg-good', text: 'connected' },
  connecting: { dot: 'bg-warn animate-pulse', text: 'connecting…' },
  disconnected: { dot: 'bg-muted', text: 'disconnected' },
};

/** Bottom bar: pedal, throttle blip, connection state and the simulated clock. */
export function ControlBar({ onSourceChange }: { onSourceChange: (kind: SourceKind) => void }) {
  const store = useStore();
  const { status, sourceKind, hello, sweep } = useSnapshot();
  const [pedal, setPedal] = useState(0);
  const [blipping, setBlipping] = useState(false);
  const clockRef = useRef<HTMLSpanElement>(null);
  const effectivePedal = blipping ? 1 : pedal;

  useEffect(() => {
    store.setPedal(effectivePedal);
  }, [store, effectivePedal]);

  useEffect(() => {
    const down = (event: KeyboardEvent): void => {
      if (event.key === BLIP_KEY && !isTypingTarget(event.target)) {
        // Every repeat of a held key must be stopped, or the browser scrolls the page.
        event.preventDefault();
        if (!event.repeat) {
          setBlipping(true);
        }
      }
    };
    const up = (event: KeyboardEvent): void => {
      if (event.key === BLIP_KEY) {
        setBlipping(false);
      }
    };
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    return () => {
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
    };
  }, []);

  useAnimationFrame(() => {
    const state = store.latestState();
    if (clockRef.current !== null && state !== null) {
      clockRef.current.textContent = `t = ${state.t_s.toFixed(1)} s`;
    }
  });

  const statusStyle = STATUS_STYLE[status];
  return (
    <footer className="flex flex-wrap items-center gap-x-6 gap-y-3 rounded-xl border border-line bg-panel px-4 py-3">
      <label className="flex min-w-[16rem] flex-1 items-center gap-3 text-xs font-semibold tracking-[0.2em] text-muted uppercase">
        Pedal
        <input
          type="range"
          min={0}
          max={PEDAL_STEPS}
          value={Math.round(effectivePedal * PEDAL_STEPS)}
          disabled={sweep.active}
          onChange={(event) => {
            setPedal(Number(event.target.value) / PEDAL_STEPS);
          }}
          className="h-2 flex-1 cursor-pointer accent-accent disabled:cursor-not-allowed disabled:opacity-40"
        />
        <span className="w-12 text-right font-mono text-sm text-ink tabular-nums">
          {(effectivePedal * 100).toFixed(0)} %
        </span>
      </label>
      <p className="text-xs text-muted">
        Hold{' '}
        <kbd className="rounded border border-line bg-raised px-1.5 py-0.5 font-mono text-ink">
          Space
        </kbd>{' '}
        for full throttle,{' '}
        <kbd className="rounded border border-line bg-raised px-1.5 py-0.5 font-mono text-ink">
          M
        </kbd>{' '}
        mutes,{' '}
        <kbd className="rounded border border-line bg-raised px-1.5 py-0.5 font-mono text-ink">
          L
        </kbd>{' '}
        holds the launch button
      </p>
      <div className="ml-auto flex items-center gap-3 font-mono text-xs text-muted">
        <span ref={clockRef} className="tabular-nums" />
        <AudioControl />
        <SourceSwitch value={sourceKind} onChange={onSourceChange} />
        <span className="inline-flex items-center gap-2 rounded-full border border-line px-3 py-1">
          <span className={`size-2 rounded-full ${statusStyle.dot}`} aria-hidden="true" />
          {hello?.engine.id ?? 'no engine'} · {statusStyle.text}
        </span>
      </div>
    </footer>
  );
}

const SOURCE_OPTIONS: readonly { kind: SourceKind; label: string; title: string }[] = [
  { kind: 'live', label: 'Live', title: 'Real simulation: simcore over WebSocket' },
  { kind: 'fake', label: 'Fake', title: 'Generated in the browser, no simcore needed' },
];

/** Segmented switch between the real simulation and the in-browser fake. */
function SourceSwitch({
  value,
  onChange,
}: {
  value: SourceKind;
  onChange: (kind: SourceKind) => void;
}) {
  return (
    <div
      role="radiogroup"
      aria-label="Data source"
      className="flex rounded-md border border-line p-0.5"
    >
      {SOURCE_OPTIONS.map((option) => {
        const active = option.kind === value;
        return (
          <button
            key={option.kind}
            type="button"
            role="radio"
            aria-checked={active}
            title={option.title}
            onClick={() => {
              if (!active) {
                onChange(option.kind);
              }
            }}
            className={`rounded px-2.5 py-0.5 font-semibold transition ${
              active
                ? option.kind === 'fake'
                  ? 'bg-warn text-canvas'
                  : 'bg-accent text-canvas'
                : 'text-muted hover:text-ink'
            }`}
          >
            {option.label}
          </button>
        );
      })}
    </div>
  );
}
