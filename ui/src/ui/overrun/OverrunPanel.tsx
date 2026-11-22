import { useRef } from 'react';

import type { OverrunSettings } from '../../protocol/messages.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { Button } from '../controls/Button.tsx';
import { Panel } from '../layout/Panel.tsx';
import { useAnimationFrame } from '../useAnimationFrame.ts';

/** Same thresholds as simcore's kPopLightOff_K / kPopFullyHot_K (core/src/app/engine_acoustics.hpp). */
const POP_LIGHT_OFF_C = 480;
const POP_FULLY_HOT_C = 630;
const MAX_RETARD_DEG = 45;
const MAX_ENRICHMENT_PCT = 30;

const STOCK: OverrunSettings = { fuelCut: true, retard_deg: 0, enrichment_pct: 0 };
const POPS_AND_BANGS: OverrunSettings = { fuelCut: false, retard_deg: 35, enrichment_pct: 10 };

type CatalystState = 'cold' | 'warm' | 'hot';

function catalystState(catalyst_C: number): CatalystState {
  if (catalyst_C >= POP_FULLY_HOT_C) {
    return 'hot';
  }
  return catalyst_C >= POP_LIGHT_OFF_C ? 'warm' : 'cold';
}

const CATALYST_LABEL: Record<CatalystState, string> = {
  cold: 'too cold for pops',
  warm: 'warm: some pops',
  hot: 'hot: ready to bang',
};

/**
 * ECU overrun settings: fuel cut on/off and the "pops & bangs" map (retard + extra fuel in
 * overrun). Pops only happen with fuel in overrun, very late ignition and a hot catalyst
 * (research.md §2.10a), so the live catalyst temperature is shown right here.
 */
export function OverrunPanel() {
  const store = useStore();
  const { overrun } = useSnapshot();
  const catalystRef = useRef<HTMLSpanElement>(null);
  const catalystStateRef = useRef<HTMLSpanElement>(null);
  const fuellingRef = useRef<HTMLSpanElement>(null);

  useAnimationFrame(() => {
    const state = store.latestState();
    const catalyst = state?.catalyst_C;
    if (catalystRef.current !== null) {
      catalystRef.current.textContent = catalyst === undefined ? '—' : `${catalyst.toFixed(0)} °C`;
    }
    if (catalystStateRef.current !== null) {
      const status = catalyst === undefined ? 'cold' : catalystState(catalyst);
      catalystStateRef.current.textContent = catalyst === undefined ? '' : CATALYST_LABEL[status];
      catalystStateRef.current.dataset.state = status;
    }
    if (fuellingRef.current !== null) {
      fuellingRef.current.dataset.active = state?.overrunFueling === true ? 'true' : 'false';
    }
  });

  const change = (next: Partial<OverrunSettings>): void => {
    if (overrun !== null) {
      store.setOverrun({ ...overrun, ...next });
    }
  };
  const disabled = overrun === null;
  const mapActive = overrun !== null && !overrun.fuelCut;

  return (
    <Panel
      title="Overrun · pops & bangs"
      subtitle="What the ECU does when you lift off at speed"
      actions={
        <div className="flex gap-1.5">
          <Button
            disabled={disabled}
            onClick={() => {
              store.setOverrun(STOCK);
            }}
            title="Fuel cut on, no retard, no extra fuel"
          >
            Stock
          </Button>
          <Button
            variant="primary"
            disabled={disabled}
            onClick={() => {
              store.setOverrun(POPS_AND_BANGS);
            }}
            title="Fuel cut off, 35 deg retard, +10 % fuel in overrun"
          >
            Pops & bangs
          </Button>
        </div>
      }
    >
      <div className="flex flex-col gap-3">
        <label className="flex cursor-pointer items-center gap-2 text-sm">
          <input
            type="checkbox"
            checked={overrun?.fuelCut ?? true}
            disabled={disabled}
            onChange={(event) => {
              change({ fuelCut: event.target.checked });
            }}
            className="size-4 accent-accent"
          />
          Overrun fuel cut
        </label>
        <Slider
          label="Retard in overrun"
          unit="°"
          value={overrun?.retard_deg ?? 0}
          max={MAX_RETARD_DEG}
          disabled={!mapActive}
          onChange={(retard_deg) => {
            change({ retard_deg });
          }}
        />
        <Slider
          label="Extra fuel in overrun"
          unit="%"
          value={overrun?.enrichment_pct ?? 0}
          max={MAX_ENRICHMENT_PCT}
          disabled={!mapActive}
          onChange={(enrichment_pct) => {
            change({ enrichment_pct });
          }}
        />
      </div>
      <div className="flex flex-wrap items-center gap-x-6 gap-y-2 font-mono text-xs text-muted">
        <span>
          catalyst <span ref={catalystRef} className="text-sm text-ink tabular-nums" />{' '}
          <span
            ref={catalystStateRef}
            className="data-[state=cold]:text-muted data-[state=hot]:text-redline data-[state=warm]:text-warn"
          />
        </span>
        <span
          ref={fuellingRef}
          className="rounded border border-line px-1.5 py-0.5 data-[active=true]:border-accent data-[active=true]:bg-accent data-[active=true]:text-canvas"
          title="Lit while the ECU injects fuel in overrun (fuel cut off)"
        >
          FUEL IN OVERRUN
        </span>
        <span className="font-sans">
          {overrun?.fuelCut === false
            ? 'Pops need about 25° or more retard and a catalyst above 480 °C.'
            : 'Stock: no fuel in overrun, the engine coasts silently.'}
        </span>
      </div>
    </Panel>
  );
}

interface SliderProps {
  label: string;
  unit: string;
  value: number;
  max: number;
  disabled: boolean;
  onChange: (value: number) => void;
}

function Slider({ label, unit, value, max, disabled, onChange }: SliderProps) {
  return (
    <label className={`flex items-center gap-3 text-sm ${disabled ? 'opacity-40' : ''}`}>
      <span className="w-44 shrink-0 text-muted">{label}</span>
      <input
        type="range"
        min={0}
        max={max}
        step={1}
        value={value}
        disabled={disabled}
        onChange={(event) => {
          onChange(Number(event.target.value));
        }}
        className="h-1.5 flex-1 cursor-pointer accent-accent disabled:cursor-not-allowed"
      />
      <span className="w-12 text-right font-mono text-ink tabular-nums">
        {value.toFixed(0)} {unit}
      </span>
    </label>
  );
}
