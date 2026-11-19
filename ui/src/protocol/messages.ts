/**
 * TypeScript types for the WebSocket protocol v1 between simcore and the UI.
 * The single source of truth is docs/interface.md; these types mirror it field by field,
 * so a JSON message from simcore can be used directly once it has been parsed.
 * Units follow the protocol (rpm, kPa, kW, degC), not SI: conversion happened in simcore's codec.
 */

export const PROTOCOL_VERSION = 1;

/** Table ids known in protocol v1. Kept as string so newer simcore tables do not break the UI. */
export type TableId = string;

export interface TableAxis {
  name: string;
  unit: string;
  /** Strictly increasing breakpoints. */
  bins: number[];
}

/** An ECU table as in shared/tables (plan.md §5.1); values[y][x], row 0 = lowest MAP. */
export interface TableData {
  id: TableId;
  name: string;
  unit: string;
  editable: boolean;
  xAxis: TableAxis;
  yAxis: TableAxis;
  min: number;
  max: number;
  values: number[][];
}

export interface TableInfo {
  id: TableId;
  name: string;
  unit: string;
  editable: boolean;
}

export interface EngineInfo {
  id: string;
  name: string;
  cylinders: number;
  displacement_l: number;
  idle_rpm: number;
  revLimit_rpm: number;
}

export interface HelloPayload {
  protocolVersion: number;
  simcore: string;
  stateRate_Hz: number;
  engine: EngineInfo;
  tables: TableInfo[];
}

export type DynoMode = 'free' | 'sweep';

/** Engine state snapshot, sent at about 60 Hz. */
export interface EngineState {
  t_s: number;
  rpm: number;
  pedal: number;
  map_kPa: number;
  /** null while lambda is undefined, e.g. during fuel cut. */
  lambda: number | null;
  lambdaTarget: number;
  ignition_deg: number;
  torque_Nm: number;
  power_kW: number;
  airflow_kg_s: number;
  fuelflow_kg_s: number;
  egt_C: number;
  dynoMode: DynoMode;
  sweepProgress: number;
  cutActive: boolean;
  /** Intake air temperature. Not sent by simcore yet (protocol v1 allows new fields). */
  iat_C?: number;
}

export interface DynoProgressPayload {
  progress: number;
  rpm: number;
}

/** Result of a sweep: parallel arrays, one entry per 100 rpm. */
export interface DynoResultPayload {
  rpm: number[];
  torque_Nm: number[];
  power_kW: number[];
  lambda: (number | null)[];
  map_kPa: number[];
  ignition_deg: number[];
  egt_C: number[];
  airflow_kg_s: number[];
}

export interface CellValue {
  /** Column index (rpm bin). */
  x: number;
  /** Row index (MAP bin). */
  y: number;
  value: number;
}

interface Envelope<T extends string, P> {
  v: typeof PROTOCOL_VERSION;
  type: T;
  id?: string;
  payload: P;
}

export type HelloMessage = Envelope<'hello', HelloPayload>;
export type StateMessage = Envelope<'state', EngineState>;
export type TableMessage = Envelope<'table', TableData>;
export type AckMessage = Envelope<'ack', Record<string, never>>;
export type ErrorMessage = Envelope<'error', { message: string }>;
export type DynoProgressMessage = Envelope<'dynoProgress', DynoProgressPayload>;
export type DynoResultMessage = Envelope<'dynoResult', DynoResultPayload>;

/** Everything simcore can send to the UI. */
export type ServerMessage =
  | HelloMessage
  | StateMessage
  | TableMessage
  | AckMessage
  | ErrorMessage
  | DynoProgressMessage
  | DynoResultMessage;

export interface StartSweepPayload {
  rpmFrom: number;
  rpmTo: number;
  rate_rpm_s?: number;
  pedal?: number;
}

/** Everything the UI can send to simcore. */
export type ClientMessage =
  | Envelope<'setPedal', { value: number }>
  | Envelope<'startSweep', StartSweepPayload>
  | Envelope<'stopSweep', Record<string, never>>
  | Envelope<'getTable', { id: TableId }>
  | Envelope<'setTableCells', { id: TableId; cells: CellValue[] }>;

export type ClientMessageType = ClientMessage['type'];
