import { useMemo, useRef } from 'react';

import { smoothTowards } from '../../domain/smoothing.ts';
import type { EngineState } from '../../protocol/messages.ts';
import { useStore } from '../../state/StoreContext.ts';
import { useAnimationFrame } from '../useAnimationFrame.ts';
import { arcPath, type DialRange, polarToCartesian, ticks, valueToAngle } from './gaugeGeometry.ts';

export type ZoneTone = 'warn' | 'danger';

export interface GaugeZone {
  from: number;
  to: number;
  tone: ZoneTone;
}

export interface RadialGaugeProps {
  label: string;
  unit: string;
  min: number;
  max: number;
  majorStep: number;
  minorPerMajor: number;
  /** Reads the gauge's value from the engine state; null = no valid value (e.g. lambda in cut). */
  read: (state: EngineState) => number | null;
  /** Main digital readout. */
  format: (value: number) => string;
  /** Optional second readout below, e.g. AFR next to lambda. */
  formatSecondary?: (value: number) => string;
  /** Label of a major tick, e.g. 3000 rpm -> "3". */
  formatTick?: (value: number) => string;
  zones?: readonly GaugeZone[];
  /** Shown instead of the number while `read` returns null. */
  nullText?: string;
  /** Needle smoothing; larger = calmer needle (see smoothing.ts). */
  timeConstant_s?: number;
  size?: 'large' | 'small';
}

/** All geometry in a 200 x 200 SVG viewBox; CSS scales it to the panel. */
const VIEW = 200;
const CENTER = { x: VIEW / 2, y: VIEW / 2 };
const DIAL_START_DEG = -135;
const DIAL_SWEEP_DEG = 270;
const ARC_RADIUS = 84;
const ZONE_RADIUS = 91;
const MAJOR_TICK = 11;
const MINOR_TICK = 5;
const LABEL_RADIUS = 60;
const NEEDLE_LENGTH = 78;
const NEEDLE_TAIL = 14;
const DEFAULT_TIME_CONSTANT_S = 0.06;
const NO_DATA_TEXT = '—';

const ZONE_COLOR: Record<ZoneTone, string> = {
  warn: 'var(--color-warn)',
  danger: 'var(--color-redline)',
};

/**
 * SVG gauge whose needle is animated outside React: every animation frame it reads the latest
 * state from the store and writes the SVG attributes directly through refs. React only renders
 * the static dial once. This keeps 60 Hz data at a smooth screen-rate needle without
 * re-rendering components (Java analogy: drawing in paintComponent instead of rebuilding the UI).
 */
export function RadialGauge(props: RadialGaugeProps) {
  const {
    label,
    unit,
    min,
    max,
    majorStep,
    minorPerMajor,
    zones = [],
    size = 'small',
    nullText = NO_DATA_TEXT,
    timeConstant_s = DEFAULT_TIME_CONSTANT_S,
  } = props;
  const store = useStore();
  const needleRef = useRef<SVGGElement>(null);
  const valueArcRef = useRef<SVGPathElement>(null);
  const readoutRef = useRef<SVGTextElement>(null);
  const secondaryRef = useRef<SVGTextElement>(null);
  const smoothed = useRef<number>(min);

  const range: DialRange = useMemo(
    () => ({ min, max, startAngle_deg: DIAL_START_DEG, sweep_deg: DIAL_SWEEP_DEG }),
    [min, max],
  );

  const dial = useMemo(
    () => (
      <Dial
        range={range}
        majorStep={majorStep}
        minorPerMajor={minorPerMajor}
        zones={zones}
        formatTick={props.formatTick ?? ((value) => String(value))}
      />
    ),
    // formatTick and zones are created inline by callers; their content only changes with the range.
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [range, majorStep, minorPerMajor],
  );

  useAnimationFrame((dt_s) => {
    const state = store.latestState();
    const target = state === null ? null : props.read(state);
    const goal = target ?? min;
    smoothed.current = smoothTowards(smoothed.current, goal, dt_s, timeConstant_s);
    const angle = valueToAngle(smoothed.current, range);
    needleRef.current?.setAttribute(
      'transform',
      `rotate(${angle.toFixed(2)} ${String(CENTER.x)} ${String(CENTER.y)})`,
    );
    valueArcRef.current?.setAttribute(
      'd',
      arcPath(CENTER, ARC_RADIUS, DIAL_START_DEG, Math.max(angle, DIAL_START_DEG + 0.01)),
    );
    const zone = zones.find((z) => target !== null && target >= z.from && target <= z.to);
    if (readoutRef.current !== null) {
      // No state at all (not connected) is different from "no valid value" (e.g. lambda in cut).
      readoutRef.current.textContent =
        state === null ? NO_DATA_TEXT : target === null ? nullText : props.format(smoothed.current);
      readoutRef.current.dataset.zone = zone?.tone ?? 'none';
    }
    if (secondaryRef.current !== null && props.formatSecondary !== undefined) {
      secondaryRef.current.textContent =
        target === null ? '' : props.formatSecondary(smoothed.current);
    }
  });

  const large = size === 'large';
  return (
    <figure className="flex flex-col items-center" aria-label={`${label} gauge`}>
      <svg
        viewBox={`0 0 ${String(VIEW)} ${String(VIEW)}`}
        className={large ? 'w-full max-w-[19rem]' : 'w-full max-w-[10.5rem]'}
        role="img"
      >
        {dial}
        <path
          ref={valueArcRef}
          fill="none"
          stroke="var(--color-accent)"
          strokeWidth={3}
          strokeLinecap="round"
          opacity={0.85}
        />
        <g ref={needleRef}>
          <line
            x1={CENTER.x}
            y1={CENTER.y + NEEDLE_TAIL}
            x2={CENTER.x}
            y2={CENTER.y - NEEDLE_LENGTH}
            stroke="var(--color-accent)"
            strokeWidth={large ? 2.5 : 3}
            strokeLinecap="round"
          />
        </g>
        <circle
          cx={CENTER.x}
          cy={CENTER.y}
          r={6}
          fill="var(--color-raised)"
          stroke="var(--color-line)"
        />
        <text
          ref={readoutRef}
          x={CENTER.x}
          y={CENTER.y + 44}
          textAnchor="middle"
          className="gauge-readout fill-ink font-mono"
          fontSize={large ? 24 : 23}
          fontWeight={600}
        />
        <text
          ref={secondaryRef}
          x={CENTER.x}
          y={CENTER.y + 64}
          textAnchor="middle"
          className="fill-muted font-mono"
          fontSize={13}
        />
        <text
          x={CENTER.x}
          y={CENTER.y + 84}
          textAnchor="middle"
          className="fill-muted"
          fontSize={12}
          letterSpacing={1.5}
        >
          {unit}
        </text>
      </svg>
      <figcaption className="-mt-3 text-xs font-semibold tracking-[0.2em] text-muted uppercase">
        {label}
      </figcaption>
    </figure>
  );
}

interface DialProps {
  range: DialRange;
  majorStep: number;
  minorPerMajor: number;
  zones: readonly GaugeZone[];
  formatTick: (value: number) => string;
}

/** The static part of the gauge: track, coloured zones, ticks and labels. */
function Dial({ range, majorStep, minorPerMajor, zones, formatTick }: DialProps) {
  const end_deg = DIAL_START_DEG + DIAL_SWEEP_DEG;
  return (
    <g>
      <path
        d={arcPath(CENTER, ARC_RADIUS, DIAL_START_DEG, end_deg)}
        fill="none"
        stroke="var(--color-line)"
        strokeWidth={3}
        strokeLinecap="round"
      />
      {zones.map((zone) => (
        <path
          key={`${String(zone.from)}-${String(zone.to)}`}
          d={arcPath(
            CENTER,
            ZONE_RADIUS,
            valueToAngle(zone.from, range),
            valueToAngle(zone.to, range),
          )}
          fill="none"
          stroke={ZONE_COLOR[zone.tone]}
          strokeWidth={5}
        />
      ))}
      {ticks(range.min, range.max, majorStep, minorPerMajor).map((tick) => {
        const angle = valueToAngle(tick.value, range);
        const outer = polarToCartesian(CENTER, ARC_RADIUS - 4, angle);
        const inner = polarToCartesian(
          CENTER,
          ARC_RADIUS - 4 - (tick.major ? MAJOR_TICK : MINOR_TICK),
          angle,
        );
        const labelPosition = polarToCartesian(CENTER, LABEL_RADIUS, angle);
        return (
          <g key={tick.value}>
            <line
              x1={inner.x}
              y1={inner.y}
              x2={outer.x}
              y2={outer.y}
              stroke={tick.major ? 'var(--color-ink)' : 'var(--color-muted)'}
              strokeWidth={tick.major ? 2 : 1}
              opacity={tick.major ? 0.9 : 0.6}
            />
            {tick.major && (
              <text
                x={labelPosition.x}
                y={labelPosition.y}
                textAnchor="middle"
                dominantBaseline="central"
                className="fill-muted font-mono"
                fontSize={11.5}
              >
                {formatTick(tick.value)}
              </text>
            )}
          </g>
        );
      })}
    </g>
  );
}
