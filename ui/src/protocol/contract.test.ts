/**
 * Contract test (plan.md §11, option A): the examples in shared/protocol/v1/ are the shared
 * contract of docs/interface.md. Every server example must parse, and a fully typed sample of
 * every message type must have the same shape (field names, nesting, JSON types) as its
 * example. simcore checks the same files in core/tests/protocol_contract_test.cpp.
 */
import { describe, expect, it } from 'vitest';

import type {
  AudioSettings,
  CellValue,
  ClientMessage,
  ClientMessageType,
  DynoResultPayload,
  EngineState,
  HelloPayload,
  LaunchSettings,
  OverrunSettings,
  ServerMessage,
  StartRollerRunPayload,
  StartSweepPayload,
  TableData,
} from './messages.ts';
import { parseServerMessage } from './parse.ts';

type Json = null | boolean | number | string | Json[] | { [key: string]: Json };

const serverFiles = import.meta.glob<Json>('../../../shared/protocol/v1/server/*.json', {
  eager: true,
  import: 'default',
});
const clientFiles = import.meta.glob<Json>('../../../shared/protocol/v1/client/*.json', {
  eager: true,
  import: 'default',
});

/** Message type -> example, from the file names. */
function byType(files: Record<string, Json>): Map<string, Json> {
  return new Map(
    Object.entries(files).map(([path, json]) => [
      path.replace(/^.*\//, '').replace('.json', ''),
      json,
    ]),
  );
}

function kind(value: Json): string {
  if (value === null) {
    return 'null';
  }
  return Array.isArray(value) ? 'array' : typeof value;
}

/** null if `actual` has the shape of `expected`; null in the example matches a number or null. */
function shapeMismatch(expected: Json, actual: Json, path = '$'): string | null {
  if (expected === null) {
    return actual === null || typeof actual === 'number'
      ? null
      : `${path}: expected number or null`;
  }
  if (kind(expected) !== kind(actual)) {
    return `${path}: expected ${kind(expected)}, got ${kind(actual)}`;
  }
  if (Array.isArray(expected) && Array.isArray(actual)) {
    const [first] = expected;
    const [other] = actual;
    return first === undefined || other === undefined
      ? null
      : shapeMismatch(first, other, `${path}[0]`);
  }
  if (kind(expected) === 'object') {
    const e = expected as Record<string, Json>;
    const a = actual as Record<string, Json>;
    for (const key of Object.keys(e)) {
      const value = a[key];
      if (value === undefined) {
        return `${path}.${key}: missing`;
      }
      const mismatch = shapeMismatch(e[key] ?? null, value, `${path}.${key}`);
      if (mismatch !== null) {
        return mismatch;
      }
    }
    const extra = Object.keys(a).find((key) => !(key in e));
    return extra === undefined ? null : `${path}.${extra}: not in the example`;
  }
  return null;
}

// --- fully typed samples: Required<> makes optional fields mandatory here ---

const hello: Required<HelloPayload> = {
  protocolVersion: 1,
  simcore: '0.1.0',
  stateRate_Hz: 60,
  engine: {
    id: 'i6_30_na',
    name: 'inline six',
    cylinders: 6,
    displacement_l: 2.993,
    idle_rpm: 750,
    revLimit_rpm: 7000,
  },
  tables: [{ id: 've', name: 'Volumetric Efficiency', unit: '%', editable: true }],
  audio: { available: true, device: 'Speakers' },
};

const state: Required<EngineState> = {
  t_s: 1,
  rpm: 3000,
  pedal: 0.4,
  map_kPa: 78,
  lambda: 1,
  lambdaTarget: 1,
  ignition_deg: 31,
  torque_Nm: 120,
  power_kW: 38,
  airflow_kg_s: 0.05,
  fuelflow_kg_s: 0.0035,
  egt_C: 742,
  dynoMode: 'free',
  sweepProgress: 0,
  cutActive: false,
  overrunFueling: false,
  catalyst_C: 610,
  launchSwitch: false,
  launchActive: false,
  sparkCut: false,
  iat_C: null,
  boost_kPa: null,
  turboSpeed_rpm: null,
  wastegateDuty_pct: null,
  knockRetard_deg: 0,
  gear: 0,
  clutch: 1,
  vehicleSpeed_kmh: 0,
  wheelTorque_Nm: 0,
  clutchLocked: false,
  shifting: false,
};

const table: TableData = {
  id: 've',
  name: 'Volumetric Efficiency',
  unit: '%',
  editable: true,
  xAxis: { name: 'rpm', unit: '1/min', bins: [800, 1200] },
  yAxis: { name: 'map', unit: 'kPa', bins: [20, 100] },
  min: 0,
  max: 150,
  values: [
    [40, 45],
    [90, 98],
  ],
};

const dynoResult: DynoResultPayload = {
  kind: 'engine',
  gear: null,
  rpm: [1000],
  torque_Nm: [249],
  power_kW: [26],
  lambda: [0.88],
  map_kPa: [101],
  ignition_deg: [11],
  egt_C: [784],
  airflow_kg_s: [0.024],
};

const overrun: OverrunSettings = { fuelCut: true, retard_deg: 0, enrichment_pct: 0 };
const launch: LaunchSettings = {
  enabled: false,
  launch_rpm: 4000,
  retard_deg: 0,
  flatShift: false,
};

type Sample<M extends { type: string }> = { [T in M['type']]: Extract<M, { type: T }> };

const SERVER_SAMPLES: Sample<ServerMessage> = {
  hello: { v: 1, type: 'hello', payload: hello },
  state: { v: 1, type: 'state', payload: state },
  table: { v: 1, type: 'table', id: '7', payload: table },
  ack: { v: 1, type: 'ack', id: '7', payload: {} },
  error: { v: 1, type: 'error', id: '7', payload: { message: 'unknown table' } },
  dynoProgress: { v: 1, type: 'dynoProgress', payload: { progress: 0.4, rpm: 3500 } },
  dynoResult: { v: 1, type: 'dynoResult', payload: dynoResult },
  overrunSettings: { v: 1, type: 'overrunSettings', payload: overrun },
  launchSettings: { v: 1, type: 'launchSettings', payload: launch },
};

const sweep: Required<StartSweepPayload> = {
  rpmFrom: 1000,
  rpmTo: 7000,
  rate_rpm_s: 300,
  pedal: 1,
};
const rollerRun: Required<StartRollerRunPayload> = { gear: 4, rpmFrom: 1500, rpmTo: 6800 };
const cell: CellValue = { x: 3, y: 10, value: 28.5 };
const audio: AudioSettings = { volume: 0.7, mute: false };

const CLIENT_SAMPLES: Sample<ClientMessage> = {
  setPedal: { v: 1, type: 'setPedal', id: '1', payload: { value: 0.5 } },
  startSweep: { v: 1, type: 'startSweep', id: '2', payload: sweep },
  stopSweep: { v: 1, type: 'stopSweep', id: '3', payload: {} },
  getTable: { v: 1, type: 'getTable', id: '4', payload: { id: 've' } },
  setTableCells: {
    v: 1,
    type: 'setTableCells',
    id: '5',
    payload: { id: 'ignition', cells: [cell] },
  },
  setAudio: { v: 1, type: 'setAudio', id: '6', payload: audio },
  getOverrunSettings: { v: 1, type: 'getOverrunSettings', id: '7', payload: {} },
  setOverrunSettings: {
    v: 1,
    type: 'setOverrunSettings',
    id: '8',
    payload: { fuelCut: false, retard_deg: 35, enrichment_pct: 10 },
  },
  getLaunchSettings: { v: 1, type: 'getLaunchSettings', id: '9', payload: {} },
  setLaunchSettings: {
    v: 1,
    type: 'setLaunchSettings',
    id: '10',
    payload: { enabled: true, launch_rpm: 4000, retard_deg: 10, flatShift: true },
  },
  setLaunchSwitch: { v: 1, type: 'setLaunchSwitch', id: '11', payload: { held: true } },
  shift: { v: 1, type: 'shift', id: '12', payload: { gear: 2 } },
  setClutch: { v: 1, type: 'setClutch', id: '13', payload: { pressed: true } },
  startRollerRun: { v: 1, type: 'startRollerRun', id: '14', payload: rollerRun },
};

function checkAgainstExamples(examples: Map<string, Json>, samples: Record<string, unknown>): void {
  expect([...examples.keys()].sort()).toEqual(Object.keys(samples).sort());
  for (const [type, example] of examples) {
    expect({ type, mismatch: shapeMismatch(example, samples[type] as Json) }).toEqual({
      type,
      mismatch: null,
    });
  }
}

describe('protocol contract (shared/protocol/v1)', () => {
  it('finds renamed, missing and retyped fields', () => {
    const example: Json = { a: 1, b: { c: true }, d: [{ e: 'x' }], f: null };
    expect(shapeMismatch(example, { a: 2, b: { c: false }, d: [{ e: 'y' }], f: 3 })).toBeNull();
    expect(shapeMismatch(example, { a: 2, b: { c: false }, d: [], g: 1 })).not.toBeNull();
    expect(shapeMismatch(example, { a: '2', b: { c: false }, d: [], f: null })).not.toBeNull();
    expect(shapeMismatch(example, { a: 2, b: { c: 1 }, d: [], f: null })).not.toBeNull();
  });

  it('parses every server example', () => {
    for (const [type, example] of byType(serverFiles)) {
      expect({ type, kind: parseServerMessage(JSON.stringify(example)).kind }).toEqual({
        type,
        kind: 'message',
      });
    }
  });

  it('has the shape of every server example in its types', () => {
    checkAgainstExamples(byType(serverFiles), SERVER_SAMPLES);
  });

  it('has the shape of every client example in its types', () => {
    const types: ClientMessageType[] = Object.keys(CLIENT_SAMPLES) as ClientMessageType[];
    expect(types.length).toBe(byType(clientFiles).size);
    checkAgainstExamples(byType(clientFiles), CLIENT_SAMPLES);
  });
});
