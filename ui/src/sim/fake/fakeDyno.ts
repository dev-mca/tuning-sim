/**
 * Fake dyno result. NOT a second simulator (CLAUDE.md: lab/ and simcore own the physics):
 * it takes the golden full-load curve of the real model and only scales it with the two
 * efficiency factors of research.md §2.6/§2.7, so that editing a table visibly changes the
 * next run. The real effect of an edit comes from simcore in iteration 6.
 */
import { findBin, interpolate } from '../../domain/table2d.ts';
import type { DynoResultPayload, TableData, TableId } from '../../protocol/messages.ts';
import { COMBUSTION, GOLDEN_SWEEP, type GoldenPoint } from './referenceData.ts';

/** simcore reports one dyno point per 100 rpm (docs/interface.md, dynoResult). */
export const DYNO_RESULT_STEP_RPM = 100;

/** Rough EGT rise per degree of retard: later combustion leaves more heat in the exhaust (§2.7). */
const EGT_RISE_PER_DEG_RETARD_C = 2.5;

const WATTS_PER_KW = 1000;
const SECONDS_PER_MINUTE = 60;

export type TableSet = ReadonlyMap<TableId, TableData>;

/** eta_ign = max(0, 1 - k (theta - theta_MBT)^2), research.md §2.7. */
export function ignitionEfficiency(ignition_deg: number, mbt_deg: number): number {
  const deviation = ignition_deg - mbt_deg;
  return Math.max(0, 1 - COMBUSTION.ignitionCurvature_per_deg2 * deviation * deviation);
}

/** Parabola with maximum 1 at lambdaOptimum, research.md §2.6. */
export function lambdaEfficiency(lambda: number): number {
  const deviation = lambda - COMBUSTION.lambdaOptimum;
  return Math.max(0, 1 - COMBUSTION.lambdaCurvature * deviation * deviation);
}

/** P = T * omega, with omega = 2 pi n / 60 (research.md §2.9). */
export function powerFromTorque_kW(torque_Nm: number, rpm: number): number {
  return (torque_Nm * 2 * Math.PI * rpm) / SECONDS_PER_MINUTE / WATTS_PER_KW;
}

/** Linear interpolation in the golden full-load curve, clamped at both ends. */
export function goldenAt(rpm: number, golden: readonly GoldenPoint[] = GOLDEN_SWEEP): GoldenPoint {
  const position = findBin(
    golden.map((point) => point.rpm),
    rpm,
  );
  const lower = golden[position.index];
  const upper = golden[position.index + 1] ?? lower;
  if (lower === undefined || upper === undefined) {
    throw new Error('golden sweep is empty');
  }
  const f = position.fraction;
  const mix = (a: number, b: number): number => a + (b - a) * f;
  return {
    rpm,
    torque_Nm: mix(lower.torque_Nm, upper.torque_Nm),
    power_kW: mix(lower.power_kW, upper.power_kW),
    map_kPa: mix(lower.map_kPa, upper.map_kPa),
    lambda: mix(lower.lambda, upper.lambda),
    ignition_deg: mix(lower.ignition_deg, upper.ignition_deg),
    airflow_kg_s: mix(lower.airflow_kg_s, upper.airflow_kg_s),
    egt_C: mix(lower.egt_C, upper.egt_C),
  };
}

/**
 * The measured lambda when the ECU's VE table differs from the engine's true VE: the ECU
 * believes in `veEcu` and doses fuel for it, but the engine breathes `veTrue` (research.md §2.4).
 */
export function measuredLambda(lambdaTarget: number, veTrue: number, veEcu: number): number {
  return veEcu === 0 ? Number.POSITIVE_INFINITY : (lambdaTarget * veTrue) / veEcu;
}

/** Operating point of the tune at (rpm, MAP), compared with the reference tune. */
export interface TuneEffect {
  ignition_deg: number;
  lambda: number;
  lambdaTarget: number;
  /** Torque factor relative to the reference tune (1 = unchanged). */
  torqueFactor: number;
  /** Degrees later than the reference tune (positive = retarded). */
  retard_deg: number;
}

export function tuneEffect(
  tune: TableSet,
  reference: TableSet,
  rpm: number,
  map_kPa: number,
): TuneEffect {
  const at = (set: TableSet, id: TableId): number =>
    interpolate(requireTable(set, id), rpm, map_kPa);
  const mbt = at(reference, 'ignitionMbt');
  const ignition = at(tune, 'ignition');
  const referenceIgnition = at(reference, 'ignition');
  const lambdaTarget = at(tune, 'lambdaTarget');
  const lambda = measuredLambda(lambdaTarget, at(reference, 've'), at(tune, 've'));
  const referenceLambda = at(reference, 'lambdaTarget');
  const torqueFactor =
    safeRatio(ignitionEfficiency(ignition, mbt), ignitionEfficiency(referenceIgnition, mbt)) *
    safeRatio(lambdaEfficiency(lambda), lambdaEfficiency(referenceLambda));
  return {
    ignition_deg: ignition,
    lambda,
    lambdaTarget,
    torqueFactor,
    retard_deg: referenceIgnition - ignition,
  };
}

/** Full-load sweep result in the format of simcore's dynoResult message. */
export function fakeDynoResult(
  tune: TableSet,
  reference: TableSet,
  rpmFrom: number,
  rpmTo: number,
): DynoResultPayload {
  const result: DynoResultPayload = {
    kind: 'engine',
    gear: null,
    rpm: [],
    torque_Nm: [],
    power_kW: [],
    lambda: [],
    map_kPa: [],
    ignition_deg: [],
    egt_C: [],
    airflow_kg_s: [],
  };
  const first = Math.ceil(rpmFrom / DYNO_RESULT_STEP_RPM) * DYNO_RESULT_STEP_RPM;
  for (let rpm = first; rpm <= rpmTo; rpm += DYNO_RESULT_STEP_RPM) {
    const golden = goldenAt(rpm);
    const effect = tuneEffect(tune, reference, rpm, golden.map_kPa);
    const torque = golden.torque_Nm * effect.torqueFactor;
    result.rpm.push(rpm);
    result.torque_Nm.push(torque);
    result.power_kW.push(powerFromTorque_kW(torque, rpm));
    result.lambda.push(Number.isFinite(effect.lambda) ? effect.lambda : null);
    result.map_kPa.push(golden.map_kPa);
    result.ignition_deg.push(effect.ignition_deg);
    result.egt_C.push(golden.egt_C + EGT_RISE_PER_DEG_RETARD_C * effect.retard_deg);
    result.airflow_kg_s.push(golden.airflow_kg_s);
  }
  return result;
}

export function requireTable(set: TableSet, id: TableId): TableData {
  const table = set.get(id);
  if (table === undefined) {
    throw new Error(`table '${id}' is missing`);
  }
  return table;
}

function safeRatio(numerator: number, denominator: number): number {
  return denominator === 0 ? 0 : numerator / denominator;
}
