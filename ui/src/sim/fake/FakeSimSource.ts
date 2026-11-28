import { applyCells } from '../../domain/tableEdit.ts';
import {
  type CellValue,
  type ClientMessage,
  type LaunchSettings,
  type OverrunSettings,
  PROTOCOL_VERSION,
  type ServerMessage,
  type StartSweepPayload,
  type TableData,
  type TableId,
} from '../../protocol/messages.ts';
import type { ConnectionStatus, SimSource } from '../SimSource.ts';
import { fakeDynoResult, type TableSet } from './fakeDyno.ts';
import { FakeEngine } from './fakeEngine.ts';
import { REFERENCE_ENGINE, referenceTables } from './referenceData.ts';

/** simcore sends state at about 60 Hz (docs/interface.md). */
export const FAKE_STATE_RATE_HZ = 60;
/** dynoProgress is sent at 10 Hz, i.e. every 6th state tick. */
const TICKS_PER_PROGRESS = 6;
/** Lowest start speed for a sweep; below idle the dyno would stall the engine. */
const MIN_SWEEP_RPM = 500;
const FAKE_VERSION = 'fake-0.1.0';
/** Same limits as simcore (core/src/domain/ecu/ecu.hpp). */
const MAX_OVERRUN_RETARD_DEG = 60;
const MAX_OVERRUN_ENRICHMENT_PCT = 50;
const MIN_LAUNCH_RPM = 2000;
const MAX_LAUNCH_RPM = 6500;
const MAX_LAUNCH_RETARD_DEG = 30;
const DEFAULT_SEED = 1;

export interface FakeSimSourceOptions {
  seed?: number;
}

/**
 * Stand-in for simcore that speaks protocol v1 (docs/interface.md) without a network.
 * Messages are delivered asynchronously and as deep copies, like over a real WebSocket,
 * so the UI cannot accidentally depend on shared objects or synchronous answers.
 */
export class FakeSimSource implements SimSource {
  readonly kind = 'fake';
  readonly address = 'in-browser fake';

  private readonly messageListeners = new Set<(message: ServerMessage) => void>();
  private readonly statusListeners = new Set<(status: ConnectionStatus) => void>();
  private readonly reference: TableSet;
  private tune: Map<TableId, TableData>;
  private readonly engine: FakeEngine;
  private timer: ReturnType<typeof setInterval> | undefined;
  private tick = 0;
  private sweepRange = { rpmFrom: 0, rpmTo: 0 };
  /** Stored and echoed only: the fake has no afterburn physics and no sound. */
  private overrun: OverrunSettings = { fuelCut: true, retard_deg: 0, enrichment_pct: 0 };
  private launch: LaunchSettings = { enabled: false, launch_rpm: 4000, retard_deg: 0 };

  constructor(options: FakeSimSourceOptions = {}) {
    this.reference = toTableSet(referenceTables());
    this.tune = toTableSet(referenceTables());
    this.engine = new FakeEngine(REFERENCE_ENGINE, this.reference, options.seed ?? DEFAULT_SEED);
    this.engine.setTune(this.tune);
  }

  connect(): void {
    if (this.timer !== undefined) {
      return;
    }
    this.emitStatus('connecting');
    this.timer = setInterval(() => {
      this.onTick();
    }, 1000 / FAKE_STATE_RATE_HZ);
    this.deliverLater(() => {
      this.emitStatus('connected');
      this.emit(this.helloMessage());
    });
  }

  disconnect(): void {
    if (this.timer === undefined) {
      return;
    }
    clearInterval(this.timer);
    this.timer = undefined;
    this.emitStatus('disconnected');
  }

  send(message: ClientMessage): void {
    const request = structuredClone(message);
    this.deliverLater(() => {
      this.handle(request);
    });
  }

  onMessage(listener: (message: ServerMessage) => void): () => void {
    this.messageListeners.add(listener);
    return () => this.messageListeners.delete(listener);
  }

  onStatus(listener: (status: ConnectionStatus) => void): () => void {
    this.statusListeners.add(listener);
    return () => this.statusListeners.delete(listener);
  }

  private handle(message: ClientMessage): void {
    switch (message.type) {
      case 'setPedal':
        this.engine.setPedal(message.payload.value);
        this.ack(message.id);
        return;
      case 'startSweep':
        this.startSweep(message.payload, message.id);
        return;
      case 'stopSweep':
        this.engine.stopSweep();
        this.ack(message.id);
        return;
      case 'getTable':
        this.sendTable(message.payload.id, message.id);
        return;
      case 'setTableCells':
        this.setTableCells(message.payload.id, message.payload.cells, message.id);
        return;
      case 'setAudio':
        this.ack(message.id); // the fake has no sound; accepting keeps the UI simple
        return;
      case 'getOverrunSettings':
        this.emit({
          v: PROTOCOL_VERSION,
          type: 'overrunSettings',
          id: message.id,
          payload: this.overrun,
        });
        return;
      case 'setOverrunSettings':
        this.setOverrun(message.payload, message.id);
        return;
      case 'getLaunchSettings':
        this.emit({
          v: PROTOCOL_VERSION,
          type: 'launchSettings',
          id: message.id,
          payload: this.launch,
        });
        return;
      case 'setLaunchSettings':
        this.setLaunch(message.payload, message.id);
        return;
      case 'setLaunchSwitch':
        this.engine.setLaunchSwitch(message.payload.held);
        this.ack(message.id);
        return;
    }
  }

  private startSweep(request: StartSweepPayload, id: string | undefined): void {
    const { rpmFrom, rpmTo } = request;
    if (rpmFrom < MIN_SWEEP_RPM || rpmTo > REFERENCE_ENGINE.revLimit_rpm || rpmFrom >= rpmTo) {
      this.error(
        id,
        `sweep range must satisfy ${String(MIN_SWEEP_RPM)} <= rpmFrom < rpmTo <= ${String(REFERENCE_ENGINE.revLimit_rpm)}`,
      );
      return;
    }
    if ((request.rate_rpm_s ?? 1) <= 0) {
      this.error(id, 'rate_rpm_s must be positive');
      return;
    }
    this.engine.startSweep(request);
    this.sweepRange = { rpmFrom, rpmTo };
    this.ack(id);
  }

  private setOverrun(settings: OverrunSettings, id: string | undefined): void {
    const { retard_deg, enrichment_pct } = settings;
    if (!(retard_deg >= 0 && retard_deg <= MAX_OVERRUN_RETARD_DEG)) {
      this.error(id, 'overrun retard must be 0..60 deg');
      return;
    }
    if (!(enrichment_pct >= 0 && enrichment_pct <= MAX_OVERRUN_ENRICHMENT_PCT)) {
      this.error(id, 'overrun enrichment must be 0..50 %');
      return;
    }
    this.overrun = { ...settings };
    this.ack(id);
    this.emit({ v: PROTOCOL_VERSION, type: 'overrunSettings', payload: this.overrun });
  }

  private setLaunch(settings: LaunchSettings, id: string | undefined): void {
    const { launch_rpm, retard_deg } = settings;
    if (!(launch_rpm >= MIN_LAUNCH_RPM && launch_rpm <= MAX_LAUNCH_RPM)) {
      this.error(id, 'launch speed must be 2000..6500 1/min');
      return;
    }
    if (!(retard_deg >= 0 && retard_deg <= MAX_LAUNCH_RETARD_DEG)) {
      this.error(id, 'launch retard must be 0..30 deg');
      return;
    }
    this.launch = { ...settings };
    this.engine.setLaunch(this.launch);
    this.ack(id);
    this.emit({ v: PROTOCOL_VERSION, type: 'launchSettings', payload: this.launch });
  }

  private sendTable(tableId: TableId, id: string | undefined): void {
    const table = this.tune.get(tableId);
    if (table === undefined) {
      this.error(id, `unknown table '${tableId}'`);
      return;
    }
    this.emit({ v: PROTOCOL_VERSION, type: 'table', id, payload: table });
  }

  /** All or nothing, like simcore: one invalid cell rejects the whole request. */
  private setTableCells(tableId: TableId, cells: CellValue[], id: string | undefined): void {
    const table = this.tune.get(tableId);
    if (table === undefined) {
      this.error(id, `unknown table '${tableId}'`);
      return;
    }
    if (!table.editable) {
      this.error(id, `table '${tableId}' is read-only`);
      return;
    }
    const invalid = cells.find((cell) => !isValidCell(table, cell));
    if (invalid !== undefined) {
      this.error(id, `invalid cell (${String(invalid.x)}, ${String(invalid.y)})`);
      return;
    }
    this.tune = new Map(this.tune).set(tableId, applyCells(table, cells));
    this.engine.setTune(this.tune);
    this.ack(id);
  }

  private onTick(): void {
    const sweepFinished = this.engine.step(1 / FAKE_STATE_RATE_HZ);
    const state = this.engine.state();
    this.emit({ v: PROTOCOL_VERSION, type: 'state', payload: state });
    this.tick++;
    if (this.engine.sweepActive && this.tick % TICKS_PER_PROGRESS === 0) {
      this.emit({
        v: PROTOCOL_VERSION,
        type: 'dynoProgress',
        payload: { progress: state.sweepProgress, rpm: state.rpm },
      });
    }
    if (sweepFinished) {
      const { rpmFrom, rpmTo } = this.sweepRange;
      this.emit({
        v: PROTOCOL_VERSION,
        type: 'dynoResult',
        payload: fakeDynoResult(this.tune, this.reference, rpmFrom, rpmTo),
      });
    }
  }

  private helloMessage(): ServerMessage {
    return {
      v: PROTOCOL_VERSION,
      type: 'hello',
      payload: {
        protocolVersion: PROTOCOL_VERSION,
        simcore: FAKE_VERSION,
        stateRate_Hz: FAKE_STATE_RATE_HZ,
        engine: REFERENCE_ENGINE,
        tables: [...this.tune.values()].map(({ id, name, unit, editable }) => ({
          id,
          name,
          unit,
          editable,
        })),
        audio: { available: false, device: 'the fake source has no sound' },
      },
    };
  }

  private ack(id: string | undefined): void {
    this.emit({ v: PROTOCOL_VERSION, type: 'ack', id, payload: {} });
  }

  private error(id: string | undefined, message: string): void {
    this.emit({ v: PROTOCOL_VERSION, type: 'error', id, payload: { message } });
  }

  private emit(message: ServerMessage): void {
    const copy = structuredClone(message);
    for (const listener of this.messageListeners) {
      listener(copy);
    }
  }

  private emitStatus(status: ConnectionStatus): void {
    for (const listener of this.statusListeners) {
      listener(status);
    }
  }

  /** Like network latency: answers never arrive inside the call that caused them. */
  private deliverLater(action: () => void): void {
    setTimeout(action, 0);
  }
}

function toTableSet(tables: TableData[]): Map<TableId, TableData> {
  return new Map(tables.map((table) => [table.id, table]));
}

function isValidCell(table: TableData, cell: CellValue): boolean {
  return (
    Number.isInteger(cell.x) &&
    Number.isInteger(cell.y) &&
    cell.x >= 0 &&
    cell.x < table.xAxis.bins.length &&
    cell.y >= 0 &&
    cell.y < table.yAxis.bins.length &&
    Number.isFinite(cell.value) &&
    cell.value >= table.min &&
    cell.value <= table.max
  );
}
