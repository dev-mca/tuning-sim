/**
 * Reference data of the i6_30_na engine, read at build time from shared/ (the same files simcore
 * and lab use). The fake source uses it so its numbers look like the real engine's.
 */
import engineDefinition from '../../../../shared/engines/i6_30_na.json';
import goldenSweepCsv from '../../../../shared/golden/i6_30_na_sweep.csv?raw';
import ignitionTable from '../../../../shared/tables/i6_30_na/ignition.json';
import ignitionMbtTable from '../../../../shared/tables/i6_30_na/ignitionMbt.json';
import lambdaTargetTable from '../../../../shared/tables/i6_30_na/lambdaTarget.json';
import veTable from '../../../../shared/tables/i6_30_na/ve.json';
import type { EngineInfo, TableData } from '../../protocol/messages.ts';

/** One row of the golden full-load sweep (shared/golden/i6_30_na_sweep.csv). */
export interface GoldenPoint {
  rpm: number;
  torque_Nm: number;
  power_kW: number;
  map_kPa: number;
  lambda: number;
  ignition_deg: number;
  airflow_kg_s: number;
  egt_C: number;
}

const CUBIC_METRES_PER_LITRE = 1e-3;

/**
 * Parses the golden sweep CSV. Lines starting with '#' are comments; the header names the
 * columns, so the parser looks them up by name instead of relying on their order.
 */
export function parseGoldenSweep(csv: string): GoldenPoint[] {
  const lines = csv
    .split('\n')
    .map((line) => line.trim())
    .filter((line) => line !== '' && !line.startsWith('#'));
  const [header, ...rows] = lines;
  if (header === undefined) {
    throw new Error('golden sweep CSV has no header');
  }
  const columns = header.split(',');
  const column = (name: string): number => {
    const index = columns.indexOf(name);
    if (index < 0) {
      throw new Error(`golden sweep CSV has no column '${name}'`);
    }
    return index;
  };
  const indices = {
    rpm: column('speedSetpoint_rpm'),
    torque: column('torque_Nm'),
    power: column('power_kW'),
    map: column('map_kPa'),
    lambda: column('lambda'),
    ignition: column('ignition_degBTDC'),
    airflow: column('airflow_kg_s'),
    egt: column('egt_degC'),
  };
  return rows.map((row) => {
    const fields = row.split(',').map(Number);
    const field = (index: number): number => {
      const value = fields[index];
      if (value === undefined || Number.isNaN(value)) {
        throw new Error(`invalid golden sweep row: '${row}'`);
      }
      return value;
    };
    return {
      rpm: field(indices.rpm),
      torque_Nm: field(indices.torque),
      power_kW: field(indices.power),
      map_kPa: field(indices.map),
      lambda: field(indices.lambda),
      ignition_deg: field(indices.ignition),
      airflow_kg_s: field(indices.airflow),
      egt_C: field(indices.egt),
    };
  });
}

/** Displacement from bore and stroke, V = z * pi/4 * d^2 * s (research.md §1). */
function displacement_l(): number {
  const { cylinders, bore_m, stroke_m } = engineDefinition;
  return (cylinders * (Math.PI / 4) * bore_m * bore_m * stroke_m) / CUBIC_METRES_PER_LITRE;
}

export const REFERENCE_ENGINE: EngineInfo = {
  id: engineDefinition.id,
  name: engineDefinition.name,
  cylinders: engineDefinition.cylinders,
  displacement_l: Math.round(displacement_l() * 1000) / 1000,
  idle_rpm: engineDefinition.idle_rpm,
  revLimit_rpm: engineDefinition.revLimit_rpm,
};

export const REVLIMIT_HYSTERESIS_RPM = engineDefinition.revLimitHysteresis_rpm;

/** Combustion parameters for the efficiency factors (research.md §2.6, §2.7). */
export const COMBUSTION = {
  lambdaOptimum: engineDefinition.combustion.lambdaOptimum,
  lambdaCurvature: engineDefinition.combustion.lambdaCurvature,
  ignitionCurvature_per_deg2: engineDefinition.combustion.ignitionCurvature_per_deg2,
} as const;

/** The ECU tune as delivered, plus the read-only MBT table of the engine model. */
export function referenceTables(): TableData[] {
  return [
    { ...veTable, editable: true },
    { ...lambdaTargetTable, editable: true },
    { ...ignitionTable, editable: true },
    { ...ignitionMbtTable, editable: false },
  ].map((table) => structuredClone(table));
}

export const GOLDEN_SWEEP: readonly GoldenPoint[] = parseGoldenSweep(goldenSweepCsv);
