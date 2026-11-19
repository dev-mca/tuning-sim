import { lambdaToAfr, mapToBoostBar } from '../../domain/units.ts';
import { useSnapshot } from '../../state/StoreContext.ts';
import { LiveReadouts } from './LiveReadouts.tsx';
import { type GaugeZone, RadialGauge } from './RadialGauge.tsx';

/** Dial range of the tachometer: a little beyond the limiter so the red zone is visible. */
const TACHO_MAX_RPM = 8000;
const DEFAULT_REV_LIMIT_RPM = 7000;
/** Width of the red zone below the limiter and the warning zone before it. */
const REDLINE_BAND_RPM = 500;
const WARN_BAND_RPM = 1000;
const RPM_PER_TICK_LABEL = 1000;

const LAMBDA_ZONES: readonly GaugeZone[] = [
  { from: 0.7, to: 0.75, tone: 'danger' },
  { from: 1.05, to: 1.15, tone: 'warn' },
  { from: 1.15, to: 1.3, tone: 'danger' },
];
const IAT_ZONES: readonly GaugeZone[] = [
  { from: 50, to: 65, tone: 'warn' },
  { from: 65, to: 80, tone: 'danger' },
];
const EGT_ZONES: readonly GaugeZone[] = [
  { from: 850, to: 950, tone: 'warn' },
  { from: 950, to: 1000, tone: 'danger' },
];

function signed(value: number, digits: number): string {
  return `${value >= 0 ? '+' : ''}${value.toFixed(digits)}`;
}

export function GaugeCluster() {
  const { hello, engineMode } = useSnapshot();
  const revLimit = hello?.engine.revLimit_rpm ?? DEFAULT_REV_LIMIT_RPM;
  const rpmZones: GaugeZone[] = [
    {
      from: revLimit - REDLINE_BAND_RPM - WARN_BAND_RPM,
      to: revLimit - REDLINE_BAND_RPM,
      tone: 'warn',
    },
    { from: revLimit - REDLINE_BAND_RPM, to: TACHO_MAX_RPM, tone: 'danger' },
  ];

  return (
    <section
      aria-label="Gauges"
      className="grid grid-cols-2 items-center gap-x-2 gap-y-4 rounded-xl border border-line bg-panel px-4 py-4 sm:grid-cols-5 xl:grid-cols-[repeat(2,minmax(0,1fr))_minmax(0,1.7fr)_repeat(3,minmax(0,1fr))_minmax(0,1.1fr)]"
    >
      <RadialGauge
        label="MAP"
        unit="kPa"
        min={0}
        max={120}
        majorStep={20}
        minorPerMajor={4}
        read={(state) => state.map_kPa}
        format={(value) => value.toFixed(0)}
        formatSecondary={(value) => `${signed(mapToBoostBar(value), 2)} bar`}
      />
      <RadialGauge
        label="Lambda"
        unit="λ"
        min={0.7}
        max={1.3}
        majorStep={0.1}
        minorPerMajor={5}
        zones={LAMBDA_ZONES}
        read={(state) => state.lambda}
        format={(value) => value.toFixed(2)}
        formatSecondary={(value) => `AFR ${lambdaToAfr(value).toFixed(1)}`}
        formatTick={(value) => value.toFixed(1)}
        nullText="CUT"
        timeConstant_s={0.12}
      />
      <div className="relative col-span-2 flex justify-center sm:col-span-5 sm:row-start-1 xl:col-span-1 xl:col-start-3">
        <RadialGauge
          size="large"
          label="Engine speed"
          unit="1/min"
          min={0}
          max={TACHO_MAX_RPM}
          majorStep={RPM_PER_TICK_LABEL}
          minorPerMajor={4}
          zones={rpmZones}
          read={(state) => state.rpm}
          format={(value) => value.toFixed(0)}
          formatTick={(value) => String(value / RPM_PER_TICK_LABEL)}
          timeConstant_s={0.05}
        />
        <div className="pointer-events-none absolute top-1 right-2 flex flex-col items-end gap-1 font-mono text-[0.7rem] font-semibold tracking-widest">
          <span
            className={`rounded px-1.5 py-0.5 ${engineMode.cutActive ? 'animate-pulse bg-redline text-canvas' : 'border border-line text-muted'}`}
          >
            LIMIT
          </span>
          <span
            className={`rounded px-1.5 py-0.5 ${engineMode.dynoMode === 'sweep' ? 'bg-accent text-canvas' : 'border border-line text-muted'}`}
          >
            DYNO
          </span>
        </div>
      </div>
      <RadialGauge
        label="Ignition"
        unit="° BTDC"
        min={-10}
        max={50}
        majorStep={10}
        minorPerMajor={5}
        read={(state) => state.ignition_deg}
        format={(value) => value.toFixed(1)}
      />
      <RadialGauge
        label="IAT"
        unit="°C"
        min={0}
        max={80}
        majorStep={20}
        minorPerMajor={4}
        zones={IAT_ZONES}
        read={(state) => state.iat_C ?? null}
        format={(value) => value.toFixed(0)}
        timeConstant_s={0.3}
      />
      <RadialGauge
        label="EGT"
        unit="°C"
        min={0}
        max={1000}
        majorStep={200}
        minorPerMajor={4}
        zones={EGT_ZONES}
        read={(state) => state.egt_C}
        format={(value) => value.toFixed(0)}
        formatTick={(value) => String(value / 100)}
        timeConstant_s={0.3}
      />
      <LiveReadouts />
    </section>
  );
}
