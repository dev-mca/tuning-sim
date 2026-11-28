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

/** Whether simcore plays sound, and on which device (or why not). */
export interface AudioInfo {
  available: boolean;
  device: string;
}

export interface HelloPayload {
  protocolVersion: number;
  simcore: string;
  stateRate_Hz: number;
  engine: EngineInfo;
  tables: TableInfo[];
  /** Since iteration 7; older simcore versions do not send it. */
  audio?: AudioInfo;
}

/** free: no dyno run (engine free or driving on the rollers); sweep: engine dyno; roller: roller run. */
export type DynoMode = 'free' | 'sweep' | 'roller';

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
  /** Intake air temperature; null while simcore models the intake at ambient (until iteration 8). */
  iat_C?: number | null;
  /** Catalyst temperature (since iteration 7b). */
  catalyst_C?: number;
  /** True while the ECU fuels in overrun with the fuel cut switched off: pops possible. */
  overrunFueling?: boolean;
  /** Launch button held (since iteration 7c; ignored by simcore during a sweep). */
  launchSwitch?: boolean;
  /** 2-step works: enabled, button held, throttle open. */
  launchActive?: boolean;
  /** The 2-step cuts the spark right now (fuel still injected: bangs possible). */
  sparkCut?: boolean;
  // Agreed fields for later iterations (plan.md §11, since 10a); null = not available.
  /** Boost above ambient (iteration 8); null without a charger. */
  boost_kPa?: number | null;
  /** Turbocharger shaft speed (iteration 8). */
  turboSpeed_rpm?: number | null;
  /** Wastegate duty cycle (iteration 8). */
  wastegateDuty_pct?: number | null;
  /** Ignition taken back by the knock control (iteration 8); 0 without knock. */
  knockRetard_deg?: number;
  /** Engaged gear (iteration 10): 0 = neutral, 1.. = forward gears; null without a gearbox. */
  gear?: number | null;
  /** Clutch engagement (iteration 10): 0 = open (pedal pressed) .. 1 = closed. */
  clutch?: number | null;
  /** Vehicle speed (iteration 10), e.g. on the roller dyno. */
  vehicleSpeed_kmh?: number | null;
  /** Torque at the driven wheels (iteration 10). */
  wheelTorque_Nm?: number | null;
  /** Engine and car turn together (since iteration 10). */
  clutchLocked?: boolean;
  /** Torque interruption of a shift (since iteration 10). */
  shifting?: boolean;
  /** Brake pedal 0..1 (since iteration 10b). */
  brake?: number;
}

/** What the ECU does when the driver lifts off at speed (research.md §3.3). */
export interface OverrunSettings {
  /** true = stock fuel cut; false = keep fuelling (needed for pops). */
  fuelCut: boolean;
  /** Ignition taken back from the map while fuelling in overrun. */
  retard_deg: number;
  /** Extra fuel while fuelling in overrun. */
  enrichment_pct: number;
}

/** Launch control / 2-step while standing (research.md §3.4). */
export interface LaunchSettings {
  /** Stock ECUs have no launch control. */
  enabled: boolean;
  /** Speed held by cutting the spark while the launch button is held. */
  launch_rpm: number;
  /** Ignition taken back from the map while the 2-step works. */
  retard_deg: number;
  /** Cut the spark during an upshift at open throttle instead of flaring (iteration 10). */
  flatShift: boolean;
}

export interface DynoProgressPayload {
  progress: number;
  rpm: number;
}

/** Result of a sweep: parallel arrays, one entry per 100 rpm. */
export interface DynoResultPayload {
  /** engine: engine dyno sweep; roller: roller run in `gear` (power at the wheels). */
  kind: 'engine' | 'roller';
  gear: number | null;
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
export type OverrunSettingsMessage = Envelope<'overrunSettings', OverrunSettings>;
export type LaunchSettingsMessage = Envelope<'launchSettings', LaunchSettings>;

/** Everything simcore can send to the UI. */
export type ServerMessage =
  | HelloMessage
  | StateMessage
  | TableMessage
  | AckMessage
  | ErrorMessage
  | DynoProgressMessage
  | DynoResultMessage
  | OverrunSettingsMessage
  | LaunchSettingsMessage;

export interface StartRollerRunPayload {
  gear: number;
  rpmFrom?: number;
  rpmTo?: number;
}

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
  | Envelope<'setTableCells', { id: TableId; cells: CellValue[] }>
  | Envelope<'setAudio', AudioSettings>
  | Envelope<'getOverrunSettings', Record<string, never>>
  | Envelope<'setOverrunSettings', OverrunSettings>
  | Envelope<'getLaunchSettings', Record<string, never>>
  | Envelope<'setLaunchSettings', LaunchSettings>
  | Envelope<'setLaunchSwitch', { held: boolean }>
  | Envelope<'shift', { gear: number }>
  | Envelope<'setClutch', { pressed: boolean }>
  | Envelope<'setBrake', { value: number }>
  | Envelope<'startRollerRun', StartRollerRunPayload>;

/** Volume 0..1; mute keeps the volume for later. */
export interface AudioSettings {
  volume: number;
  mute: boolean;
}

export type ClientMessageType = ClientMessage['type'];
