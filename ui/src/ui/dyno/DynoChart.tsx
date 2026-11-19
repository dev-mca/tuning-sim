import 'uplot/dist/uPlot.min.css';

import { useEffect, useRef } from 'react';
import uPlot from 'uplot';

import { SURFACE } from '../theme/palette.ts';
import type { AlignedSeries } from './alignRuns.ts';

export interface ChartSeries {
  label: string;
  color: string;
}

interface DynoChartProps {
  /** Axis title, e.g. "Torque (Nm)". */
  yLabel: string;
  data: AlignedSeries;
  series: readonly ChartSeries[];
  /** Charts with the same key share the crosshair (torque and power at the same rpm). */
  syncKey: string;
  height: number;
  showXAxis: boolean;
  onHoverRpm: (rpm: number | null) => void;
}

const AXIS_FONT = '11px "JetBrains Mono Variable", ui-monospace, monospace';
const LABEL_FONT = '600 11px "Inter Variable", ui-sans-serif, system-ui, sans-serif';
const LINE_WIDTH_PX = 2;
const CURSOR_POINT_PX = 8;

/**
 * Thin React wrapper around uPlot (a fast canvas chart library). uPlot is imperative, so the
 * chart lives in a ref: created when the series change, fed new data with setData and resized
 * with setSize. One y-axis per chart on purpose: torque and power have different units, a
 * shared chart with two y-scales would invite reading crossings that mean nothing.
 */
export function DynoChart({
  yLabel,
  data,
  series,
  syncKey,
  height,
  showXAxis,
  onHoverRpm,
}: DynoChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<uPlot | null>(null);
  const hoverRef = useRef(onHoverRpm);

  useEffect(() => {
    hoverRef.current = onHoverRpm;
  });

  const seriesKey = series.map((s) => `${s.label}:${s.color}`).join('|');

  useEffect(() => {
    const container = containerRef.current;
    if (container === null) {
      return;
    }
    const options: uPlot.Options = {
      width: Math.max(container.clientWidth, 100),
      height,
      legend: { show: false },
      cursor: {
        sync: { key: syncKey },
        points: { size: CURSOR_POINT_PX },
        drag: { x: false, y: false },
      },
      scales: { x: { time: false } },
      axes: [
        {
          show: showXAxis,
          stroke: SURFACE.muted,
          font: AXIS_FONT,
          labelFont: LABEL_FONT,
          label: showXAxis ? 'Engine speed (1/min)' : undefined,
          grid: { stroke: SURFACE.grid, width: 1 },
          ticks: { stroke: SURFACE.line, width: 1 },
        },
        {
          stroke: SURFACE.muted,
          font: AXIS_FONT,
          labelFont: LABEL_FONT,
          label: yLabel,
          size: 52,
          grid: { stroke: SURFACE.grid, width: 1 },
          ticks: { stroke: SURFACE.line, width: 1 },
        },
      ],
      series: [
        {},
        ...series.map((s) => ({
          label: s.label,
          stroke: s.color,
          width: LINE_WIDTH_PX,
          points: { show: false },
          spanGaps: false,
        })),
      ],
      hooks: {
        setCursor: [
          (chart: uPlot) => {
            const index = chart.cursor.idx;
            const rpm =
              index === null || index === undefined ? null : (chart.data[0][index] ?? null);
            hoverRef.current(rpm);
          },
        ],
      },
    };
    const chart = new uPlot(options, toUplotData(data), container);
    chartRef.current = chart;
    const observer = new ResizeObserver(() => {
      chart.setSize({ width: Math.max(container.clientWidth, 100), height });
    });
    observer.observe(container);
    return () => {
      observer.disconnect();
      chart.destroy();
      chartRef.current = null;
    };
    // The chart is rebuilt only when the set of series changes; data updates go through setData.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [seriesKey, syncKey, height, showXAxis, yLabel]);

  useEffect(() => {
    chartRef.current?.setData(toUplotData(data));
  }, [data]);

  return <div ref={containerRef} className="w-full" />;
}

function toUplotData(data: AlignedSeries): uPlot.AlignedData {
  return [data.rpm, ...data.values];
}
