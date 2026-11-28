import { useEffect, useRef } from 'react';

import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { Button } from '../controls/Button.tsx';
import { isTypingTarget } from '../keyboard.ts';
import { Panel } from '../layout/Panel.tsx';
import { useAnimationFrame } from '../useAnimationFrame.ts';
import { clutchStatus, gearLabel } from './drivetrainDisplay.ts';

/** Keys: shift up, shift down, clutch pedal (held). */
const SHIFT_UP_KEY = 'e';
const SHIFT_DOWN_KEY = 'q';
const CLUTCH_KEY = 'c';

const CLUTCH_TEXT = {
  open: 'neutral',
  pressed: 'clutch pressed',
  slipping: 'clutch slipping',
  locked: 'clutch locked',
} as const;

/**
 * Gearbox and clutch of the car on the roller dyno (research.md §8): sequential shifting with
 * an automatic clutch, plus the clutch pedal for launching. In neutral or with the pedal pressed
 * the engine revs freely as before.
 */
export function DrivetrainPanel() {
  const store = useStore();
  const { clutchPressed, sweep } = useSnapshot();
  const gearRef = useRef<HTMLSpanElement>(null);
  const speedRef = useRef<HTMLSpanElement>(null);
  const wheelTorqueRef = useRef<HTMLSpanElement>(null);
  const clutchRef = useRef<HTMLSpanElement>(null);
  const shiftingRef = useRef<HTMLSpanElement>(null);

  useEffect(() => {
    const down = (event: KeyboardEvent): void => {
      if (isTypingTarget(event.target)) {
        return;
      }
      const key = event.key.toLowerCase();
      if (key === CLUTCH_KEY) {
        event.preventDefault();
        store.setClutch(true); // repeated key events are filtered by the store
      } else if (!event.repeat && key === SHIFT_UP_KEY) {
        store.shiftBy(1);
      } else if (!event.repeat && key === SHIFT_DOWN_KEY) {
        store.shiftBy(-1);
      }
    };
    const up = (event: KeyboardEvent): void => {
      if (event.key.toLowerCase() === CLUTCH_KEY) {
        store.setClutch(false);
      }
    };
    const release = (): void => {
      store.setClutch(false); // the window lost focus while the key was held
    };
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    window.addEventListener('blur', release);
    return () => {
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
      window.removeEventListener('blur', release);
    };
  }, [store]);

  useAnimationFrame(() => {
    const state = store.latestState();
    if (state === null) {
      return;
    }
    if (gearRef.current !== null) {
      gearRef.current.textContent = gearLabel(state.gear);
    }
    if (speedRef.current !== null) {
      speedRef.current.textContent = (state.vehicleSpeed_kmh ?? 0).toFixed(0);
    }
    if (wheelTorqueRef.current !== null) {
      wheelTorqueRef.current.textContent = `${(state.wheelTorque_Nm ?? 0).toFixed(0)} Nm`;
    }
    if (clutchRef.current !== null) {
      const status = clutchStatus(state);
      clutchRef.current.textContent = CLUTCH_TEXT[status];
      clutchRef.current.dataset.status = status;
    }
    if (shiftingRef.current !== null) {
      shiftingRef.current.dataset.active = state.shifting === true ? 'true' : 'false';
    }
  });

  const hold = (pressed: boolean) => () => {
    store.setClutch(pressed);
  };

  return (
    <Panel
      title="Gearbox · roller dyno"
      subtitle="The car stands on road-load rollers: shift, launch and drive"
    >
      <div className="flex flex-wrap items-center gap-6">
        <div className="flex items-baseline gap-2">
          <span className="text-xs font-semibold tracking-[0.2em] text-muted uppercase">Gear</span>
          <span ref={gearRef} className="w-10 font-mono text-5xl font-bold text-ink tabular-nums">
            –
          </span>
        </div>
        <div className="flex items-baseline gap-1">
          <span ref={speedRef} className="font-mono text-3xl text-ink tabular-nums">
            0
          </span>
          <span className="text-sm text-muted">km/h</span>
        </div>
        <div className="flex flex-col gap-1 font-mono text-xs text-muted">
          <span>
            wheels <span ref={wheelTorqueRef} className="text-ink tabular-nums" />
          </span>
          <span
            ref={clutchRef}
            className="data-[status=locked]:text-good data-[status=pressed]:text-warn data-[status=slipping]:text-redline"
          />
        </div>
        <span
          ref={shiftingRef}
          className="rounded border border-line px-1.5 py-0.5 font-mono text-xs text-muted data-[active=true]:border-accent data-[active=true]:bg-accent data-[active=true]:text-canvas"
          title="Lit during the torque interruption of a shift"
        >
          SHIFT
        </span>
      </div>
      <div className="flex flex-wrap items-center gap-2">
        <Button
          disabled={sweep.active}
          onClick={() => {
            store.shiftBy(-1);
          }}
          title="Shift down (Q)"
        >
          ▼ Down
        </Button>
        <Button
          disabled={sweep.active}
          onClick={() => {
            store.shiftBy(1);
          }}
          title="Shift up (E)"
        >
          ▲ Up
        </Button>
        <button
          type="button"
          aria-pressed={clutchPressed}
          onPointerDown={hold(true)}
          onPointerUp={hold(false)}
          onPointerLeave={hold(false)}
          onPointerCancel={hold(false)}
          title="Hold (or hold C) to press the clutch pedal"
          className="rounded-md border border-line px-3 py-1.5 text-sm font-semibold tracking-[0.2em] text-ink uppercase transition select-none aria-pressed:border-warn aria-pressed:bg-warn aria-pressed:text-canvas"
        >
          Clutch
        </button>
        <p className="text-xs text-muted">
          <kbd className="rounded border border-line bg-raised px-1.5 py-0.5 font-mono text-ink">
            E
          </kbd>
          /
          <kbd className="rounded border border-line bg-raised px-1.5 py-0.5 font-mono text-ink">
            Q
          </kbd>{' '}
          shift,{' '}
          <kbd className="rounded border border-line bg-raised px-1.5 py-0.5 font-mono text-ink">
            C
          </kbd>{' '}
          clutch. Launch: 1st gear, hold C, floor it (2-step), release C.
        </p>
      </div>
    </Panel>
  );
}
