import { describe, expect, it } from 'vitest';

import { createTuneFile } from '../domain/tuneFile.ts';
import type {
  ClientMessage,
  HelloPayload,
  ServerMessage,
  TableData,
} from '../protocol/messages.ts';
import type { ConnectionStatus, SimSource } from '../sim/SimSource.ts';
import { SimStore } from './simStore.ts';

/**
 * Test double for a live connection: the test decides what the "server" sends and inspects
 * what the store sent. Unlike FakeSimSource it answers nothing by itself.
 */
class ScriptedSource implements SimSource {
  readonly kind = 'live';
  readonly address = 'ws://test';
  readonly sent: ClientMessage[] = [];
  private messageListener: ((message: ServerMessage) => void) | null = null;
  private statusListener: ((status: ConnectionStatus) => void) | null = null;

  connect(): void {
    this.statusListener?.('connecting');
  }

  disconnect(): void {
    this.statusListener?.('disconnected');
  }

  send(message: ClientMessage): void {
    this.sent.push(message);
  }

  onMessage(listener: (message: ServerMessage) => void): () => void {
    this.messageListener = listener;
    return () => (this.messageListener = null);
  }

  onStatus(listener: (status: ConnectionStatus) => void): () => void {
    this.statusListener = listener;
    return () => (this.statusListener = null);
  }

  status(status: ConnectionStatus): void {
    this.statusListener?.(status);
  }

  server(message: Omit<ServerMessage, 'v'>): void {
    this.messageListener?.({ v: 1, ...message } as ServerMessage);
  }

  sentOfType<T extends ClientMessage['type']>(type: T): Extract<ClientMessage, { type: T }>[] {
    return this.sent.filter(
      (message): message is Extract<ClientMessage, { type: T }> => message.type === type,
    );
  }
}

const HELLO: HelloPayload = {
  protocolVersion: 1,
  simcore: '0.1.0',
  stateRate_Hz: 60,
  engine: {
    id: 'i6_30_na',
    name: 'test engine',
    cylinders: 6,
    displacement_l: 2.993,
    idle_rpm: 750,
    revLimit_rpm: 7000,
  },
  tables: [{ id: 'ignition', name: 'Ignition', unit: 'deg BTDC', editable: true }],
};

function ignition(values: number[][]): TableData {
  return {
    id: 'ignition',
    name: 'Ignition',
    unit: 'deg BTDC',
    editable: true,
    xAxis: { name: 'rpm', unit: '1/min', bins: [1000, 2000] },
    yAxis: { name: 'map', unit: 'kPa', bins: [30, 100] },
    min: -10,
    max: 50,
    values,
  };
}

const ORIGINAL = ignition([
  [20, 30],
  [10, 25],
]);

function connectedStore(): { store: SimStore; source: ScriptedSource } {
  const source = new ScriptedSource();
  const store = new SimStore(source);
  store.start();
  source.status('connected');
  source.server({ type: 'hello', payload: HELLO });
  source.server({ type: 'table', payload: ORIGINAL });
  return { store, source };
}

function ignitionValues(store: SimStore): number[][] | undefined {
  return store.getSnapshot().tables.get('ignition')?.values;
}

describe('SimStore with a live source', () => {
  it('fetches all tables and repeats the pedal after every hello', () => {
    const { store, source } = connectedStore();
    store.setPedal(0.3);
    source.status('disconnected');
    source.sent.length = 0;

    source.status('connected');
    source.server({ type: 'hello', payload: HELLO });
    expect(source.sentOfType('getTable').map((m) => m.payload.id)).toEqual(['ignition']);
    expect(source.sentOfType('setPedal').at(-1)?.payload.value).toBe(0.3);
  });

  it('sends the sound settings after hello and on change', () => {
    const source = new ScriptedSource();
    const store = new SimStore(source, { volume: 0.4, mute: true });
    store.start();
    source.status('connected');
    source.server({ type: 'hello', payload: HELLO });
    expect(source.sentOfType('setAudio').at(-1)?.payload).toEqual({ volume: 0.4, mute: true });

    store.setAudio({ volume: 1.5, mute: false });
    expect(store.getSnapshot().audio).toEqual({ volume: 1, mute: false });
    expect(source.sentOfType('setAudio').at(-1)?.payload).toEqual({ volume: 1, mute: false });
  });

  it('forgets the engine state and the sweep when the connection drops', () => {
    const { store, source } = connectedStore();
    source.server({
      type: 'dynoProgress',
      payload: { progress: 0.5, rpm: 4000 },
    });
    expect(store.getSnapshot().sweep.active).toBe(true);
    source.status('disconnected');
    expect(store.getSnapshot().sweep.active).toBe(false);
    expect(store.latestState()).toBeNull();
  });

  it('re-fetches a table when simcore rejects an edit', () => {
    const { store, source } = connectedStore();
    store.editTable('ignition', [{ x: 0, y: 0, value: 22 }]);
    const request = source.sentOfType('setTableCells')[0];
    source.sent.length = 0;

    source.server({ type: 'error', id: request?.id, payload: { message: 'invalid cell' } });
    expect(store.getSnapshot().toast).toEqual({ tone: 'error', text: 'invalid cell' });
    expect(source.sentOfType('getTable').map((m) => m.payload.id)).toEqual(['ignition']);

    source.server({ type: 'table', payload: ORIGINAL });
    expect(ignitionValues(store)).toEqual(ORIGINAL.values);
  });

  it('keeps the original tables across a reconnect to the same engine', () => {
    const { store, source } = connectedStore();
    store.editTable('ignition', [{ x: 0, y: 0, value: 22 }]);
    source.status('connected');
    source.server({ type: 'hello', payload: HELLO });
    source.server({
      type: 'table',
      payload: ignition([
        [22, 30],
        [10, 25],
      ]),
    });
    expect(store.getSnapshot().originalTables.get('ignition')?.values).toEqual(ORIGINAL.values);
  });

  it('exports, imports and reverts a tune, each import being one undo step', () => {
    const { store, source } = connectedStore();
    store.editTable('ignition', [{ x: 1, y: 1, value: 20 }]);
    const tune = store.exportTune(new Date(2026, 10, 19));
    expect(tune?.tables.ignition?.values).toEqual([
      [20, 30],
      [10, 20],
    ]);

    store.revertToOriginal();
    expect(ignitionValues(store)).toEqual(ORIGINAL.values);

    const changed = store.importTune(JSON.stringify(tune));
    expect(changed).toBe(1);
    expect(ignitionValues(store)?.[1]?.[1]).toBe(20);
    expect(source.sentOfType('setTableCells').at(-1)?.payload.cells).toEqual([
      { x: 1, y: 1, value: 20 },
    ]);

    store.undo();
    expect(ignitionValues(store)).toEqual(ORIGINAL.values);
  });

  it('rejects a tune for another engine without changing anything', () => {
    const { store } = connectedStore();
    const tune = createTuneFile(
      'v8',
      [
        ignition([
          [0, 0],
          [0, 0],
        ]),
      ],
      new Date(),
    );
    expect(() => store.importTune(JSON.stringify(tune))).toThrow(/engine 'v8'/);
    expect(ignitionValues(store)).toEqual(ORIGINAL.values);
  });

  it('notes the tune of the sweep on the dyno run', () => {
    const { store, source } = connectedStore();
    store.editTable('ignition', [
      { x: 0, y: 0, value: 17 },
      { x: 1, y: 0, value: 27 },
    ]);
    store.startSweep({ rpmFrom: 1000, rpmTo: 2000 });
    store.editTable('ignition', [{ x: 0, y: 1, value: 0 }]); // after the start: not in the note
    source.server({
      type: 'dynoResult',
      payload: {
        rpm: [1000],
        torque_Nm: [250],
        power_kW: [26],
        lambda: [0.88],
        map_kPa: [100],
        ignition_deg: [17],
        egt_C: [780],
        airflow_kg_s: [0.02],
      },
    });
    expect(store.getSnapshot().runs[0]?.note).toBe('ignition: 2 cells, mean −3.0 deg BTDC');
  });
});

describe('SimStore overrun settings', () => {
  it('asks for them after hello, keeps the first as original and reverts to it', () => {
    const { store, source } = connectedStore();
    expect(source.sentOfType('getOverrunSettings')).toHaveLength(1);
    const stock = { fuelCut: true, retard_deg: 0, enrichment_pct: 0 };
    source.server({ type: 'overrunSettings', payload: stock });
    expect(store.getSnapshot().overrun).toEqual(stock);

    const pops = { fuelCut: false, retard_deg: 35, enrichment_pct: 10 };
    store.setOverrun(pops);
    expect(store.getSnapshot().overrun).toEqual(pops); // optimistic
    expect(source.sentOfType('setOverrunSettings').at(-1)?.payload).toEqual(pops);
    source.server({ type: 'overrunSettings', payload: pops });
    expect(store.getSnapshot().originalOverrun).toEqual(stock);

    store.revertToOriginal();
    expect(source.sentOfType('setOverrunSettings').at(-1)?.payload).toEqual(stock);
  });

  it('asks again when simcore rejects a change', () => {
    const { store, source } = connectedStore();
    store.setOverrun({ fuelCut: false, retard_deg: 99, enrichment_pct: 0 });
    const request = source.sentOfType('setOverrunSettings').at(-1);
    source.sent.length = 0;
    source.server({
      type: 'error',
      id: request?.id,
      payload: { message: 'overrun retard must be 0..60 deg' },
    });
    expect(source.sentOfType('getOverrunSettings')).toHaveLength(1);
  });

  it('saves the overrun settings in the tune file and loads them back', () => {
    const { store, source } = connectedStore();
    const pops = { fuelCut: false, retard_deg: 35, enrichment_pct: 10 };
    source.server({ type: 'overrunSettings', payload: pops });
    const tune = store.exportTune(new Date(2026, 10, 22));
    expect(tune?.overrun).toEqual(pops);
    source.server({
      type: 'overrunSettings',
      payload: { fuelCut: true, retard_deg: 0, enrichment_pct: 0 },
    });
    store.importTune(JSON.stringify(tune));
    expect(source.sentOfType('setOverrunSettings').at(-1)?.payload).toEqual(pops);
  });
});
