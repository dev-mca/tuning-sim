import { useMemo, useState } from 'react';

import { kwToPs } from '../../domain/units.ts';
import { type DynoRun, peakOf } from '../../state/dynoRuns.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { Button } from '../controls/Button.tsx';
import { Panel } from '../layout/Panel.tsx';
import { runColor } from '../theme/palette.ts';
import { alignRuns, type DynoField, valueAtRpm } from './alignRuns.ts';
import { type ChartSeries, DynoChart } from './DynoChart.tsx';

const SYNC_KEY = 'dyno';
const TORQUE: DynoField = (result) => result.torque_Nm;
const POWER: DynoField = (result) => result.power_kW;
const TORQUE_CHART_HEIGHT_PX = 190;
const POWER_CHART_HEIGHT_PX = 210;

const SWEEP_RATES_RPM_S = [300, 600, 1200] as const;
const DEFAULT_SWEEP = { rpmFrom: 1000, rpmTo: 7000, rate_rpm_s: 600 };

export function DynoPanel() {
  const { runs, sweep, hello } = useSnapshot();
  const store = useStore();
  const [hoverRpm, setHoverRpm] = useState<number | null>(null);
  const [sweepRequest, setSweepRequest] = useState(DEFAULT_SWEEP);
  const revLimit = hello?.engine.revLimit_rpm ?? DEFAULT_SWEEP.rpmTo;

  const visible = useMemo(() => runs.filter((run) => run.visible), [runs]);
  const series: ChartSeries[] = visible.map((run) => ({
    label: run.label,
    color: runColor(run.colorSlot),
  }));
  const torque = useMemo(
    () =>
      alignRuns(
        visible.map((run) => run.result),
        TORQUE,
      ),
    [visible],
  );
  const power = useMemo(
    () =>
      alignRuns(
        visible.map((run) => run.result),
        POWER,
      ),
    [visible],
  );

  return (
    <Panel
      title="Dyno"
      subtitle="Full-load sweep · torque and power over engine speed"
      actions={
        <SweepControls
          request={sweepRequest}
          revLimit={revLimit}
          active={sweep.active}
          onChange={setSweepRequest}
          onStart={() => {
            store.startSweep({ ...sweepRequest, pedal: 1 });
          }}
          onStop={() => {
            store.stopSweep();
          }}
        />
      }
    >
      <SweepProgress active={sweep.active} progress={sweep.progress} rpm={sweep.rpm} />
      {visible.length === 0 ? (
        <div className="flex min-h-[25rem] flex-col items-center justify-center gap-2 rounded-lg border border-dashed border-line text-center text-sm text-muted">
          <p>{runs.length === 0 ? 'No dyno runs yet.' : 'All runs are hidden.'}</p>
          <p>Start a sweep, change a table, sweep again: the runs are overlaid here.</p>
        </div>
      ) : (
        <div className="flex flex-col">
          <DynoChart
            yLabel="Torque (Nm)"
            data={torque}
            series={series}
            syncKey={SYNC_KEY}
            height={TORQUE_CHART_HEIGHT_PX}
            showXAxis={false}
            onHoverRpm={setHoverRpm}
          />
          <DynoChart
            yLabel="Power (kW)"
            data={power}
            series={series}
            syncKey={SYNC_KEY}
            height={POWER_CHART_HEIGHT_PX}
            showXAxis={true}
            onHoverRpm={setHoverRpm}
          />
        </div>
      )}
      <RunTable runs={runs} hoverRpm={hoverRpm} />
    </Panel>
  );
}

interface SweepControlsProps {
  request: typeof DEFAULT_SWEEP;
  revLimit: number;
  active: boolean;
  onChange: (request: typeof DEFAULT_SWEEP) => void;
  onStart: () => void;
  onStop: () => void;
}

function SweepControls({
  request,
  revLimit,
  active,
  onChange,
  onStart,
  onStop,
}: SweepControlsProps) {
  const field =
    'h-8 w-[4.5rem] rounded-md border border-line bg-canvas px-2 font-mono text-sm text-ink focus:border-accent focus:outline-none disabled:opacity-40';
  const valid =
    request.rpmFrom >= 500 && request.rpmTo <= revLimit && request.rpmFrom < request.rpmTo;
  return (
    <div className="flex flex-wrap items-center gap-1.5 text-xs text-muted">
      <label className="flex items-center gap-1">
        from
        <input
          type="number"
          step={100}
          value={request.rpmFrom}
          disabled={active}
          className={field}
          onChange={(event) => {
            onChange({ ...request, rpmFrom: Number(event.target.value) });
          }}
        />
      </label>
      <label className="flex items-center gap-1">
        to
        <input
          type="number"
          step={100}
          max={revLimit}
          value={request.rpmTo}
          disabled={active}
          className={field}
          onChange={(event) => {
            onChange({ ...request, rpmTo: Number(event.target.value) });
          }}
        />
      </label>
      <select
        aria-label="Sweep rate"
        value={request.rate_rpm_s}
        disabled={active}
        className={`${field} w-auto`}
        onChange={(event) => {
          onChange({ ...request, rate_rpm_s: Number(event.target.value) });
        }}
      >
        {SWEEP_RATES_RPM_S.map((rate) => (
          <option key={rate} value={rate}>
            {rate} rpm/s
          </option>
        ))}
      </select>
      {active ? (
        <Button variant="danger" onClick={onStop}>
          Stop
        </Button>
      ) : (
        <Button
          variant="primary"
          disabled={!valid}
          onClick={onStart}
          title={valid ? 'Start full-load sweep' : `range must be 500 … ${String(revLimit)} 1/min`}
        >
          Start sweep
        </Button>
      )}
    </div>
  );
}

function SweepProgress({
  active,
  progress,
  rpm,
}: {
  active: boolean;
  progress: number;
  rpm: number;
}) {
  return (
    <div className="flex items-center gap-3" aria-live="polite">
      <div className="h-1.5 flex-1 overflow-hidden rounded-full bg-raised">
        <div
          className="h-full rounded-full bg-accent transition-[width] duration-100"
          style={{ width: `${(active ? progress * 100 : 0).toFixed(1)}%` }}
        />
      </div>
      <span className="w-36 text-right font-mono text-xs text-muted tabular-nums">
        {active ? `sweeping ${rpm.toFixed(0)} 1/min` : 'idle'}
      </span>
    </div>
  );
}

function RunTable({ runs, hoverRpm }: { runs: readonly DynoRun[]; hoverRpm: number | null }) {
  const store = useStore();
  if (runs.length === 0) {
    return null;
  }
  const baseline = runs.find((run) => run.visible);
  const baselineTorque =
    baseline === undefined ? null : peakOf(baseline.result.torque_Nm, baseline.result.rpm);
  return (
    <table className="w-full text-left text-xs">
      <thead className="text-muted">
        <tr className="[&>th]:pb-1 [&>th]:font-semibold">
          <th scope="col">Run</th>
          <th scope="col" className="text-right">
            Peak torque
          </th>
          <th scope="col" className="text-right">
            Peak power
          </th>
          <th scope="col" className="text-right">
            {hoverRpm === null ? 'At cursor' : `@ ${hoverRpm.toFixed(0)} 1/min`}
          </th>
          <th scope="col">
            <span className="sr-only">Actions</span>
          </th>
        </tr>
      </thead>
      <tbody className="font-mono tabular-nums">
        {runs.map((run) => {
          const torque = peakOf(run.result.torque_Nm, run.result.rpm);
          const power = peakOf(run.result.power_kW, run.result.rpm);
          const delta =
            torque !== null && baselineTorque !== null && run !== baseline
              ? torque.value - baselineTorque.value
              : null;
          const hoverTorque = hoverRpm === null ? null : valueAtRpm(run.result, TORQUE, hoverRpm);
          const hoverPower = hoverRpm === null ? null : valueAtRpm(run.result, POWER, hoverRpm);
          return (
            <tr
              key={run.id}
              className={`border-t border-line [&>td]:py-1.5 ${run.visible ? '' : 'opacity-40'}`}
            >
              <td className="font-sans">
                <label className="flex cursor-pointer items-center gap-2">
                  <input
                    type="checkbox"
                    checked={run.visible}
                    onChange={() => {
                      store.toggleRunVisible(run.id);
                    }}
                    className="sr-only"
                  />
                  <span
                    aria-hidden="true"
                    className="h-0.5 w-5 rounded-full"
                    style={{ backgroundColor: runColor(run.colorSlot) }}
                  />
                  <span className="font-semibold text-ink">{run.label}</span>
                </label>
                <span
                  className="mt-0.5 block max-w-[16rem] truncate text-[0.7rem] text-muted"
                  title={run.note}
                >
                  {run.note}
                </span>
              </td>
              <td className="text-right text-ink">
                {torque === null ? '—' : `${torque.value.toFixed(1)} Nm`}
                {delta !== null && (
                  <span className={delta >= 0 ? 'text-good' : 'text-redline'}>
                    {' '}
                    {delta >= 0 ? '+' : ''}
                    {delta.toFixed(1)}
                  </span>
                )}
                <span className="block text-muted">
                  {torque === null ? '' : `@ ${torque.rpm.toFixed(0)}`}
                </span>
              </td>
              <td className="text-right text-ink">
                {power === null ? '—' : `${power.value.toFixed(1)} kW`}
                <span className="block text-muted">
                  {power === null
                    ? ''
                    : `${kwToPs(power.value).toFixed(0)} PS @ ${power.rpm.toFixed(0)}`}
                </span>
              </td>
              <td className="text-right text-ink">
                {hoverTorque === null ? '—' : `${hoverTorque.toFixed(1)} Nm`}
                <span className="block text-muted">
                  {hoverPower === null ? '' : `${hoverPower.toFixed(1)} kW`}
                </span>
              </td>
              <td className="text-right">
                <button
                  type="button"
                  onClick={() => {
                    store.deleteRun(run.id);
                  }}
                  className="rounded px-1.5 text-muted hover:text-redline"
                  aria-label={`Delete ${run.label}`}
                  title="Delete run"
                >
                  ✕
                </button>
              </td>
            </tr>
          );
        })}
      </tbody>
    </table>
  );
}
