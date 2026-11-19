import { EditHistory, type TableEdit } from '../domain/editHistory.ts';
import { applyCells, readCells } from '../domain/tableEdit.ts';
import {
  type CellValue,
  type ClientMessage,
  type DynoResultPayload,
  type EngineState,
  type HelloPayload,
  PROTOCOL_VERSION,
  type ServerMessage,
  type StartSweepPayload,
  type TableData,
  type TableId,
} from '../protocol/messages.ts';
import type { ConnectionStatus, SimSource, SourceKind } from '../sim/SimSource.ts';
import { addRun, type DynoRun, removeRun, toggleRun } from './dynoRuns.ts';

export interface SweepStatus {
  active: boolean;
  progress: number;
  rpm: number;
}

/**
 * Everything React renders from. It changes rarely (connection, tables, runs, sweep), so a new
 * snapshot object per change is cheap. The 60 Hz engine state is deliberately NOT in here:
 * re-rendering React 60 times a second would be wasteful; gauges read it per animation frame
 * through `latestState()` instead.
 */
export interface SimSnapshot {
  sourceKind: SourceKind;
  status: ConnectionStatus;
  hello: HelloPayload | null;
  tables: ReadonlyMap<TableId, TableData>;
  /** Tables as first received, to mark edited cells. */
  originalTables: ReadonlyMap<TableId, TableData>;
  runs: readonly DynoRun[];
  sweep: SweepStatus;
  /** Discrete engine flags that the React UI shows; changes at most a few times a second. */
  engineMode: { dynoMode: EngineState['dynoMode']; cutActive: boolean };
  canUndo: boolean;
  canRedo: boolean;
  lastError: string | null;
}

const IDLE_SWEEP: SweepStatus = { active: false, progress: 0, rpm: 0 };

/**
 * Connects a SimSource to the UI (Java analogy: an observable service with listeners).
 * It implements the subscribe/getSnapshot contract of React's useSyncExternalStore.
 */
export class SimStore {
  private readonly source: SimSource;
  private readonly history = new EditHistory();
  private readonly listeners = new Set<() => void>();
  private readonly unsubscribers: (() => void)[] = [];
  private snapshot: SimSnapshot;
  private state: EngineState | null = null;
  private nextRequestId = 1;
  private nextRunNumber = 1;

  constructor(source: SimSource) {
    this.source = source;
    this.snapshot = {
      sourceKind: source.kind,
      status: 'disconnected',
      hello: null,
      tables: new Map(),
      originalTables: new Map(),
      runs: [],
      sweep: IDLE_SWEEP,
      engineMode: { dynoMode: 'free', cutActive: false },
      canUndo: false,
      canRedo: false,
      lastError: null,
    };
  }

  start(): void {
    this.unsubscribers.push(
      this.source.onMessage((message) => {
        this.handle(message);
      }),
      this.source.onStatus((status) => {
        this.update({ status });
      }),
    );
    this.source.connect();
  }

  stop(): void {
    this.source.disconnect();
    for (const unsubscribe of this.unsubscribers.splice(0)) {
      unsubscribe();
    }
  }

  // --- useSyncExternalStore contract -------------------------------------------------------

  readonly subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  readonly getSnapshot = (): SimSnapshot => this.snapshot;

  /** The most recent 60 Hz state, read by animation-frame loops (gauges, live cursor). */
  latestState(): EngineState | null {
    return this.state;
  }

  // --- commands --------------------------------------------------------------------------

  setPedal(value: number): void {
    this.send({ type: 'setPedal', payload: { value } });
  }

  startSweep(request: StartSweepPayload): void {
    this.send({ type: 'startSweep', payload: request });
  }

  stopSweep(): void {
    this.send({ type: 'stopSweep', payload: {} });
  }

  /**
   * Applies cell changes locally right away (optimistic update, so editing feels instant),
   * sends them to the simulation and records them for undo.
   */
  editTable(tableId: TableId, changes: readonly CellValue[]): void {
    const table = this.snapshot.tables.get(tableId);
    if (table === undefined || changes.length === 0) {
      return;
    }
    const before = readCells(table, changes);
    this.history.record({ tableId, before, after: [...changes] });
    this.applyAndSend(tableId, changes);
  }

  /**
   * Like editTable, but computes the changes from the store's current table. Event handlers use
   * this, so several key presses in a row never work on a stale copy from the last render.
   */
  editTableWith(tableId: TableId, compute: (table: TableData) => readonly CellValue[]): void {
    const table = this.snapshot.tables.get(tableId);
    if (table !== undefined) {
      this.editTable(tableId, compute(table));
    }
  }

  undo(): void {
    this.replay(this.history.undo(), 'before');
  }

  redo(): void {
    this.replay(this.history.redo(), 'after');
  }

  toggleRunVisible(runId: number): void {
    this.update({ runs: toggleRun(this.snapshot.runs, runId) });
  }

  deleteRun(runId: number): void {
    this.update({ runs: removeRun(this.snapshot.runs, runId) });
  }

  dismissError(): void {
    this.update({ lastError: null });
  }

  // --- incoming messages -----------------------------------------------------------------

  private handle(message: ServerMessage): void {
    switch (message.type) {
      case 'hello':
        this.update({ hello: message.payload });
        for (const table of message.payload.tables) {
          this.send({ type: 'getTable', payload: { id: table.id } });
        }
        return;
      case 'state':
        this.handleState(message.payload);
        return;
      case 'table':
        this.handleTable(message.payload);
        return;
      case 'dynoProgress':
        this.update({ sweep: { active: true, ...message.payload } });
        return;
      case 'dynoResult':
        this.handleDynoResult(message.payload);
        return;
      case 'error':
        this.update({ lastError: message.payload.message });
        return;
      case 'ack':
        return;
    }
  }

  private handleState(state: EngineState): void {
    this.state = state;
    const mode = this.snapshot.engineMode;
    if (mode.dynoMode !== state.dynoMode || mode.cutActive !== state.cutActive) {
      this.update({ engineMode: { dynoMode: state.dynoMode, cutActive: state.cutActive } });
    }
    const sweepEnded = this.snapshot.sweep.active && state.dynoMode !== 'sweep';
    const sweepStarted = !this.snapshot.sweep.active && state.dynoMode === 'sweep';
    if (sweepEnded) {
      this.update({ sweep: IDLE_SWEEP });
    } else if (sweepStarted) {
      this.update({ sweep: { active: true, progress: state.sweepProgress, rpm: state.rpm } });
    }
  }

  private handleTable(table: TableData): void {
    const originals = this.snapshot.originalTables;
    this.update({
      tables: new Map(this.snapshot.tables).set(table.id, table),
      originalTables: originals.has(table.id) ? originals : new Map(originals).set(table.id, table),
    });
  }

  private handleDynoResult(result: DynoResultPayload): void {
    const runNumber = this.nextRunNumber++;
    this.update({ runs: addRun(this.snapshot.runs, runNumber, result), sweep: IDLE_SWEEP });
  }

  // --- helpers ---------------------------------------------------------------------------

  private replay(edit: TableEdit | undefined, side: 'before' | 'after'): void {
    if (edit !== undefined) {
      this.applyAndSend(edit.tableId, edit[side]);
    }
  }

  private applyAndSend(tableId: TableId, cells: readonly CellValue[]): void {
    const table = this.snapshot.tables.get(tableId);
    if (table === undefined) {
      return;
    }
    this.update({ tables: new Map(this.snapshot.tables).set(tableId, applyCells(table, cells)) });
    this.send({ type: 'setTableCells', payload: { id: tableId, cells: [...cells] } });
  }

  private send(message: DistributiveOmit<ClientMessage, 'v' | 'id'>): void {
    const id = String(this.nextRequestId++);
    this.source.send({ ...message, v: PROTOCOL_VERSION, id });
  }

  private update(changes: Partial<SimSnapshot>): void {
    this.snapshot = {
      ...this.snapshot,
      ...changes,
      canUndo: this.history.canUndo,
      canRedo: this.history.canRedo,
    };
    for (const listener of this.listeners) {
      listener();
    }
  }
}

/** Omit that keeps a union a union (plain Omit would merge all message types into one). */
type DistributiveOmit<T, K extends PropertyKey> = T extends unknown ? Omit<T, K> : never;
