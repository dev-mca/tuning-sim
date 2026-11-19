/**
 * Display conversions. The protocol already uses rpm/kPa/kW/degC (docs/interface.md);
 * these are the extra views a tuner expects on the gauges.
 */

/** Stoichiometric air/fuel ratio of gasoline (research.md §0, AFR_st). */
export const STOICHIOMETRIC_AFR_GASOLINE = 14.7;

/** Standard atmosphere; boost is shown relative to it. */
export const AMBIENT_PRESSURE_KPA = 101.325;

const KPA_PER_BAR = 100;
const KW_PER_PS = 0.73549875;

export function lambdaToAfr(lambda: number): number {
  return lambda * STOICHIOMETRIC_AFR_GASOLINE;
}

/** Gauge pressure in bar: negative = vacuum, positive = boost. */
export function mapToBoostBar(map_kPa: number): number {
  return (map_kPa - AMBIENT_PRESSURE_KPA) / KPA_PER_BAR;
}

/** Metric horsepower, shown next to kW because tuners think in PS. */
export function kwToPs(power_kW: number): number {
  return power_kW / KW_PER_PS;
}

/** Significant digits kept for display; removes noise such as 1100.0000000000002 rpm. */
const DISPLAY_PRECISION = 10;

/**
 * Formats an axis or measured value without floating-point noise. simcore converts rad/s back
 * to rpm, which turns 1100 into 1100.0000000000002; this shows "1100".
 */
export function formatClean(value: number, maxDecimals = 3): string {
  const clean = Number(value.toPrecision(DISPLAY_PRECISION));
  const factor = 10 ** maxDecimals;
  return String(Math.round(clean * factor) / factor);
}
