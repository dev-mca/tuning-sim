import { useEffect, useRef } from 'react';

import type { LaunchSettings } from '../../protocol/messages.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { Button } from '../controls/Button.tsx';
import { isTypingTarget } from '../keyboard.ts';
import { Panel } from '../layout/Panel.tsx';
import { useAnimationFrame } from '../useAnimationFrame.ts';

/** Same limits as simcore's validateLaunchSettings (core/src/domain/ecu/ecu.hpp). */
const MIN_LAUNCH_RPM = 2000;
const MAX_LAUNCH_RPM = 6500;
const LAUNCH_RPM_STEP = 100;
const MAX_RETARD_DEG = 30;
/** Key that holds the launch button (like pressing the clutch while standing). */
const LAUNCH_KEY = 'l';

const STOCK: LaunchSettings = { enabled: false, launch_rpm: 4000, retard_deg: 0 };
const TWO_STEP: LaunchSettings = { enabled: true, launch_rpm: 4000, retard_deg: 10 };

/**
 * Launch control / 2-step (research.md §3.4): while the launch button is held and the throttle
 * is open, the ECU holds the launch speed by cutting the spark and keeps injecting, so the
 * unburnt mixture bangs in the hot exhaust. Release the button to "launch".
 */
export function LaunchPanel() {
  const store = useStore();
  const { launch, launchSwitch } = useSnapshot();
  const activeRef = useRef<HTMLSpanElement>(null);
  const sparkCutRef = useRef<HTMLSpanElement>(null);
  const rpmRef = useRef<HTMLSpanElement>(null);

  useEffect(() => {
    const down = (event: KeyboardEvent): void => {
      if (event.key.toLowerCase() === LAUNCH_KEY && !isTypingTarget(event.target)) {
        event.preventDefault();
        store.setLaunchSwitch(true); // repeated key events are filtered by the store
      }
    };
    const up = (event: KeyboardEvent): void => {
      if (event.key.toLowerCase() === LAUNCH_KEY) {
        store.setLaunchSwitch(false);
      }
    };
    const release = (): void => {
      store.setLaunchSwitch(false); // e.g. the window loses focus while the key is held
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
    if (activeRef.current !== null) {
      activeRef.current.dataset.active = state?.launchActive === true ? 'true' : 'false';
    }
    if (sparkCutRef.current !== null) {
      sparkCutRef.current.dataset.active = state?.sparkCut === true ? 'true' : 'false';
    }
    if (rpmRef.current !== null) {
      rpmRef.current.textContent = state === null ? '—' : `${state.rpm.toFixed(0)} /min`;
    }
  });

  const change = (next: Partial<LaunchSettings>): void => {
    if (launch !== null) {
      store.setLaunch({ ...launch, ...next });
    }
  };
  const disabled = launch === null;
  const enabled = launch?.enabled === true;
  const hold = (held: boolean) => () => {
    store.setLaunchSwitch(held);
  };

  return (
    <Panel
      title="Launch control · 2-step"
      subtitle="Hold the launch button and floor it: the ECU holds the launch speed"
      actions={
        <div className="flex gap-1.5">
          <Button
            disabled={disabled}
            onClick={() => {
              store.setLaunch(STOCK);
            }}
            title="No launch control"
          >
            Stock
          </Button>
          <Button
            variant="primary"
            disabled={disabled}
            onClick={() => {
              store.setLaunch(TWO_STEP);
            }}
            title="2-step at 4000 1/min with 10 deg retard"
          >
            2-step
          </Button>
        </div>
      }
    >
      <div className="flex flex-col gap-3">
        <label className="flex cursor-pointer items-center gap-2 text-sm">
          <input
            type="checkbox"
            checked={enabled}
            disabled={disabled}
            onChange={(event) => {
              change({ enabled: event.target.checked });
            }}
            className="size-4 accent-accent"
          />
          Launch control
        </label>
        <Slider
          label="Launch speed"
          unit="/min"
          value={launch?.launch_rpm ?? STOCK.launch_rpm}
          min={MIN_LAUNCH_RPM}
          max={MAX_LAUNCH_RPM}
          step={LAUNCH_RPM_STEP}
          disabled={!enabled}
          onChange={(launch_rpm) => {
            change({ launch_rpm });
          }}
        />
        <Slider
          label="Retard while held"
          unit="°"
          value={launch?.retard_deg ?? 0}
          min={0}
          max={MAX_RETARD_DEG}
          step={1}
          disabled={!enabled}
          onChange={(retard_deg) => {
            change({ retard_deg });
          }}
        />
      </div>
      <div className="flex flex-wrap items-center gap-x-4 gap-y-2 font-mono text-xs text-muted">
        <button
          type="button"
          aria-pressed={launchSwitch}
          disabled={disabled}
          onPointerDown={hold(true)}
          onPointerUp={hold(false)}
          onPointerLeave={hold(false)}
          onPointerCancel={hold(false)}
          title="Hold (or hold L) like pressing the clutch while standing; release to launch"
          className="rounded-md border border-line px-3 py-1.5 font-semibold tracking-[0.2em] text-ink uppercase transition select-none aria-pressed:border-redline aria-pressed:bg-redline aria-pressed:text-canvas disabled:opacity-40"
        >
          Launch
        </button>
        <span
          ref={activeRef}
          className="rounded border border-line px-1.5 py-0.5 data-[active=true]:border-accent data-[active=true]:bg-accent data-[active=true]:text-canvas"
          title="Lit while the 2-step works: enabled, button held, throttle open"
        >
          2-STEP
        </span>
        <span
          ref={sparkCutRef}
          className="rounded border border-line px-1.5 py-0.5 data-[active=true]:border-redline data-[active=true]:bg-redline data-[active=true]:text-canvas"
          title="Lit while the ECU cuts the spark but keeps injecting: bangs need a hot catalyst"
        >
          SPARK CUT
        </span>
        <span>
          engine <span ref={rpmRef} className="text-sm text-ink tabular-nums" />
        </span>
        <span className="font-sans">
          {enabled
            ? 'The catalyst heats up while you hold it: the longer, the louder the bangs.'
            : 'Stock: no launch control, the engine revs to the normal limiter.'}
        </span>
      </div>
    </Panel>
  );
}

interface SliderProps {
  label: string;
  unit: string;
  value: number;
  min: number;
  max: number;
  step: number;
  disabled: boolean;
  onChange: (value: number) => void;
}

function Slider({ label, unit, value, min, max, step, disabled, onChange }: SliderProps) {
  return (
    <label className={`flex items-center gap-3 text-sm ${disabled ? 'opacity-40' : ''}`}>
      <span className="w-44 shrink-0 text-muted">{label}</span>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        disabled={disabled}
        onChange={(event) => {
          onChange(Number(event.target.value));
        }}
        className="h-1.5 flex-1 cursor-pointer accent-accent disabled:cursor-not-allowed"
      />
      <span className="w-20 text-right font-mono text-ink tabular-nums">
        {value.toFixed(0)} {unit}
      </span>
    </label>
  );
}
