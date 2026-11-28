import type { TableId } from '../../protocol/messages.ts';

/**
 * Which .msq constants feed which tuning-sim table, and how their units are converted
 * (ADR 0012). Unit conversion happens only here, at the edge: everything after this module
 * works in the units of our tables (%, deg BTDC, lambda, rpm, kPa).
 */

/**
 * Stoichiometric air-fuel ratio of gasoline, used to turn AFR into lambda (λ = AFR / 14.7).
 * Our reference engine runs on gasoline; other fuels are not supported by the import.
 */
export const GASOLINE_STOICH_AFR = 14.7;

/** Converts a value from one accepted file unit into our unit. */
export interface UnitConversion {
  /** Accepted spellings of the file's `units` attribute, lower case. */
  fileUnits: readonly string[];
  convert: (value: number) => number;
}

/** A physical quantity with the file units we accept for it. */
export interface Quantity {
  /** Our unit, for messages. */
  unit: string;
  conversions: readonly UnitConversion[];
}

const identity = (value: number): number => value;

export const RPM: Quantity = {
  unit: 'rpm',
  conversions: [{ fileUnits: ['rpm', '1/min'], convert: identity }],
};

/** Only MAP-based load is supported; alpha-N (TPS %) tables describe a different axis. */
export const MAP_KPA: Quantity = {
  unit: 'kPa',
  conversions: [{ fileUnits: ['kpa'], convert: identity }],
};

export const VE_PERCENT: Quantity = {
  unit: '%',
  conversions: [{ fileUnits: ['%'], convert: identity }],
};

export const ADVANCE_DEG: Quantity = {
  unit: 'deg BTDC',
  conversions: [{ fileUnits: ['deg', '°', 'deg btdc', 'degree'], convert: identity }],
};

export const LAMBDA: Quantity = {
  unit: 'lambda',
  conversions: [
    { fileUnits: ['lambda', 'λ'], convert: identity },
    { fileUnits: ['afr'], convert: (afr) => afr / GASOLINE_STOICH_AFR },
  ],
};

/** The three constants that make up one table: values plus its two axes. */
export interface MsqTableSource {
  values: string;
  xBins: string;
  yBins: string;
}

export interface MsqTableMapping {
  tableId: TableId;
  quantity: Quantity;
  /** Constant names per firmware family; the first one present in the file is used. */
  sources: readonly MsqTableSource[];
}

/**
 * Supported tables. Names follow the common TunerStudio firmware conventions (Speeduino-style
 * first, rusEFI-style second); only names are used, no firmware code or .ini content.
 */
export const MSQ_TABLE_MAPPINGS: readonly MsqTableMapping[] = [
  {
    tableId: 've',
    quantity: VE_PERCENT,
    sources: [
      { values: 'veTable', xBins: 'rpmBins', yBins: 'fuelLoadBins' },
      { values: 'veTable', xBins: 'veRpmBins', yBins: 'veLoadBins' },
    ],
  },
  {
    tableId: 'ignition',
    quantity: ADVANCE_DEG,
    sources: [
      { values: 'advTable1', xBins: 'rpmBins2', yBins: 'mapBins1' },
      { values: 'ignitionTable', xBins: 'ignitionRpmBins', yBins: 'ignitionLoadBins' },
    ],
  },
  {
    tableId: 'lambdaTarget',
    quantity: LAMBDA,
    sources: [
      { values: 'afrTable', xBins: 'rpmBinsAFR', yBins: 'loadBinsAFR' },
      { values: 'lambdaTable', xBins: 'lambdaRpmBins', yBins: 'lambdaLoadBins' },
    ],
  },
];

/** The x axis of every supported table is engine speed, the y axis is MAP. */
export const X_AXIS_QUANTITY = RPM;
export const Y_AXIS_QUANTITY = MAP_KPA;

/**
 * Finds the conversion for a file unit (case and surrounding spaces ignored), or undefined
 * if the unit is not accepted for this quantity.
 */
export function findConversion(quantity: Quantity, fileUnit: string): UnitConversion | undefined {
  const normalized = fileUnit.trim().toLowerCase();
  return quantity.conversions.find((conversion) => conversion.fileUnits.includes(normalized));
}
