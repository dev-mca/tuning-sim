import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import type {
  ClientMessage,
  EngineState,
  ServerMessage,
  TableData,
} from '../../protocol/messages.ts';
import type { ConnectionStatus } from '../SimSource.ts';
import { FAKE_STATE_RATE_HZ, FakeSimSource } from './FakeSimSource.ts';

const TICK_MS = 1000 / FAKE_STATE_RATE_HZ;

/** Collects everything the fake sends, like a test double for the UI store. */
function connectedSource(): {
  source: FakeSimSource;
  messages: ServerMessage[];
  statuses: ConnectionStatus[];
} {
  const source = new FakeSimSource({ seed: 42 });
  const messages: ServerMessage[] = [];
  const statuses: ConnectionStatus[] = [];
  source.onMessage((message) => messages.push(message));
  source.onStatus((status) => statuses.push(status));
  source.connect();
  return { source, messages, statuses };
}

function send(source: FakeSimSource, message: Omit<ClientMessage, 'v'>): void {
  source.send({ v: 1, ...message } as ClientMessage);
}

function ofType<T extends ServerMessage['type']>(
  messages: ServerMessage[],
  type: T,
): Extract<ServerMessage, { type: T }>[] {
  return messages.filter(
    (message): message is Extract<ServerMessage, { type: T }> => message.type === type,
  );
}

function lastState(messages: ServerMessage[]): EngineState {
  const state = ofType(messages, 'state').at(-1);
  if (state === undefined) throw new Error('no state received');
  return state.payload;
}

describe('FakeSimSource', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('connects asynchronously and greets with hello like simcore', () => {
    const { messages, statuses } = connectedSource();
    expect(statuses).toEqual(['connecting']);
    expect(messages).toHaveLength(0);

    vi.advanceTimersByTime(0);
    expect(statuses).toEqual(['connecting', 'connected']);
    const hello = ofType(messages, 'hello')[0];
    expect(hello?.payload.engine.id).toBe('i6_30_na');
    expect(hello?.payload.tables.map((table) => table.id)).toEqual([
      've',
      'lambdaTarget',
      'ignition',
      'ignitionMbt',
    ]);
  });

  it('sends state at about 60 Hz with idle values at rest', () => {
    const { messages } = connectedSource();
    vi.advanceTimersByTime(1000);
    expect(ofType(messages, 'state').length).toBeGreaterThanOrEqual(FAKE_STATE_RATE_HZ - 1);
    const state = lastState(messages);
    expect(state.rpm).toBeGreaterThan(700);
    expect(state.rpm).toBeLessThan(820);
    expect(state.map_kPa).toBeGreaterThan(25);
    expect(state.map_kPa).toBeLessThan(40);
    expect(state.dynoMode).toBe('free');
  });

  it('revs up on pedal, hits the limiter and cuts fuel', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'setPedal', id: 'p', payload: { value: 1 } });
    vi.advanceTimersByTime(4000);
    const states = ofType(messages, 'state').map((message) => message.payload);
    expect(Math.max(...states.map((state) => state.rpm))).toBeGreaterThan(6900);
    expect(states.some((state) => state.cutActive && state.lambda === null)).toBe(true);
    expect(ofType(messages, 'ack').some((ack) => ack.id === 'p')).toBe(true);
  });

  it('answers getTable with the requested table and its request id', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'getTable', id: '7', payload: { id: 'ignition' } });
    vi.advanceTimersByTime(0);
    const table = ofType(messages, 'table')[0];
    expect(table?.id).toBe('7');
    expect(table?.payload.values).toHaveLength(16);
  });

  it('rejects unknown and read-only tables with an error', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'getTable', id: 'a', payload: { id: 'turbo' } });
    send(source, {
      type: 'setTableCells',
      id: 'b',
      payload: { id: 'ignitionMbt', cells: [{ x: 0, y: 0, value: 10 }] },
    });
    vi.advanceTimersByTime(0);
    expect(ofType(messages, 'error').map((error) => error.id)).toEqual(['a', 'b']);
  });

  it('applies setTableCells all or nothing', () => {
    const { source, messages } = connectedSource();
    send(source, {
      type: 'setTableCells',
      id: 'bad',
      payload: {
        id: 've',
        cells: [
          { x: 0, y: 0, value: 60 },
          { x: 99, y: 0, value: 60 },
        ],
      },
    });
    send(source, {
      type: 'setTableCells',
      id: 'good',
      payload: { id: 've', cells: [{ x: 0, y: 0, value: 61 }] },
    });
    send(source, { type: 'getTable', id: 'read', payload: { id: 've' } });
    vi.advanceTimersByTime(0);
    expect(ofType(messages, 'error')[0]?.id).toBe('bad');
    const table: TableData | undefined = ofType(messages, 'table')[0]?.payload;
    expect(table?.values[0]?.[0]).toBe(61);
  });

  it('runs a sweep with progress messages and a final dynoResult', () => {
    const { source, messages } = connectedSource();
    send(source, {
      type: 'startSweep',
      id: 's',
      payload: { rpmFrom: 2000, rpmTo: 3000, rate_rpm_s: 1000 },
    });
    vi.advanceTimersByTime(500);
    expect(lastState(messages).dynoMode).toBe('sweep');
    vi.advanceTimersByTime(600 + TICK_MS);
    expect(ofType(messages, 'dynoProgress').length).toBeGreaterThan(5);
    const result = ofType(messages, 'dynoResult')[0];
    expect(result?.payload.rpm).toEqual([
      2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000,
    ]);
    expect(lastState(messages).dynoMode).toBe('free');
  });

  it('rejects an invalid sweep range', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'startSweep', id: 'x', payload: { rpmFrom: 5000, rpmTo: 4000 } });
    vi.advanceTimersByTime(0);
    expect(ofType(messages, 'error')[0]?.id).toBe('x');
  });

  it('delivers copies, so the UI cannot change the fake tables by accident', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'getTable', id: '1', payload: { id: 've' } });
    vi.advanceTimersByTime(0);
    const first = ofType(messages, 'table')[0]?.payload;
    if (first?.values[0] === undefined) throw new Error('table missing');
    first.values[0][0] = -1;
    send(source, { type: 'getTable', id: '2', payload: { id: 've' } });
    vi.advanceTimersByTime(0);
    expect(ofType(messages, 'table')[1]?.payload.values[0]?.[0]).not.toBe(-1);
  });

  it('stops sending after disconnect', () => {
    const { source, messages, statuses } = connectedSource();
    vi.advanceTimersByTime(100);
    source.disconnect();
    const count = messages.length;
    vi.advanceTimersByTime(1000);
    expect(messages.length).toBe(count);
    expect(statuses.at(-1)).toBe('disconnected');
  });
});

describe('FakeSimSource overrun settings', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('reports, validates, stores and broadcasts overrun settings', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'getOverrunSettings', id: 'g', payload: {} });
    send(source, {
      type: 'setOverrunSettings',
      id: 'bad',
      payload: { fuelCut: false, retard_deg: 80, enrichment_pct: 10 },
    });
    send(source, {
      type: 'setOverrunSettings',
      id: 'ok',
      payload: { fuelCut: false, retard_deg: 35, enrichment_pct: 10 },
    });
    vi.advanceTimersByTime(0);
    const settings = ofType(messages, 'overrunSettings');
    expect(settings[0]?.id).toBe('g');
    expect(settings[0]?.payload.fuelCut).toBe(true);
    expect(ofType(messages, 'error').map((error) => error.id)).toEqual(['bad']);
    expect(settings.at(-1)?.payload).toEqual({
      fuelCut: false,
      retard_deg: 35,
      enrichment_pct: 10,
    });
  });
});

describe('FakeSimSource launch control', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('reports, validates, stores and broadcasts launch settings', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'getLaunchSettings', id: 'g', payload: {} });
    send(source, {
      type: 'setLaunchSettings',
      id: 'bad',
      payload: { enabled: true, launch_rpm: 9000, retard_deg: 10, flatShift: false },
    });
    send(source, {
      type: 'setLaunchSettings',
      id: 'ok',
      payload: { enabled: true, launch_rpm: 4000, retard_deg: 10, flatShift: false },
    });
    vi.advanceTimersByTime(0);
    const settings = ofType(messages, 'launchSettings');
    expect(settings[0]?.id).toBe('g');
    expect(settings[0]?.payload.enabled).toBe(false);
    expect(ofType(messages, 'error').map((error) => error.id)).toEqual(['bad']);
    expect(settings.at(-1)?.payload).toEqual({
      enabled: true,
      launch_rpm: 4000,
      retard_deg: 10,
      flatShift: false,
    });
  });

  it('holds the launch speed at full pedal while the button is held', () => {
    const { source, messages } = connectedSource();
    send(source, {
      type: 'setLaunchSettings',
      payload: { enabled: true, launch_rpm: 4000, retard_deg: 10, flatShift: false },
    });
    send(source, { type: 'setLaunchSwitch', payload: { held: true } });
    send(source, { type: 'setPedal', payload: { value: 1 } });
    vi.advanceTimersByTime(4000);
    const held = ofType(messages, 'state').at(-1)?.payload;
    expect(held?.launchActive).toBe(true);
    expect(held?.rpm).toBeGreaterThan(3700);
    expect(held?.rpm).toBeLessThan(4150);

    send(source, { type: 'setLaunchSwitch', payload: { held: false } });
    vi.advanceTimersByTime(3000);
    const released = ofType(messages, 'state').at(-1)?.payload;
    expect(released?.launchActive).toBe(false);
    expect(released?.rpm).toBeGreaterThan(6000);
  });
});

describe('FakeSimSource drivetrain', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('shifts, reports gear and clutch and rejects invalid gears', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'shift', id: 'bad', payload: { gear: 9 } });
    send(source, { type: 'setClutch', payload: { pressed: true } });
    send(source, { type: 'shift', id: 'ok', payload: { gear: 2 } });
    vi.advanceTimersByTime(200);
    expect(ofType(messages, 'error').map((error) => error.id)).toEqual(['bad']);
    const state = ofType(messages, 'state').at(-1)?.payload;
    expect(state?.gear).toBe(2);
    expect(state?.clutch).toBe(0);
    expect(state?.vehicleSpeed_kmh).toBe(0);
  });

  it('answers a roller run with a roller result below the engine power', () => {
    const { source, messages } = connectedSource();
    send(source, { type: 'startRollerRun', payload: { gear: 4, rpmFrom: 3000, rpmTo: 4000 } });
    vi.advanceTimersByTime(500);
    expect(ofType(messages, 'state').at(-1)?.payload.dynoMode).toBe('roller');
    vi.advanceTimersByTime(5000);
    const result = ofType(messages, 'dynoResult').at(-1)?.payload;
    expect(result?.kind).toBe('roller');
    expect(result?.gear).toBe(4);
    expect(result?.rpm.length).toBe(11);
  });
});
