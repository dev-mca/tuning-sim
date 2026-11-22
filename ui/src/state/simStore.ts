import { EditHistory, type EditStep } from '../domain/editHistory.ts';
import { applyCells, readCells } from '../domain/tableEdit.ts';
import {
  createTuneFile,
  differingCells,
  parseTuneFile,
  type TuneFile,
  tuneChanges,
} from '../domain/tuneFile.ts';
import {
  type CellValue,
  type AudioSettings,
  type ClientMessage,
  type DynoResultPayload,
  type EngineState,
  type HelloPayload,
  type OverrunSettings,
  PROTOCOL_VERSION,
  type ServerMessage,
  type StartSweepPayload,
  type TableData,
  type TableId,
} from '../protocol/messages.ts';
import type { ConnectionStatus, SimSource, SourceKind } from '../sim/SimSource.ts';
import { addRun, type DynoRun, removeRun, toggleRun } from './dynoRuns.ts';
import { describeDifferences, tuneDifferences } from './tuneSummary.ts';

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
  /** Short message for the user: errors from the simulation or results such as "tune loaded". */
  toast: Toast | null;
  /** Where the source connects to, shown when the connection is down. */
  sourceAddress: string;
  /** The user's sound settings; sent to simcore after every hello. */
  audio: AudioSettings;
  /** ECU overrun settings (pops & bangs); null until simcore reported them. */
  overrun: OverrunSettings | null;
  /** Overrun settings as first received, for "revert". */
  originalOverrun: OverrunSettings | null;
}

export interface Toast {
  tone: 'error' | 'info';
  text: string;
}

const IDLE_SWEEP: SweepStatus = { active: false, progress: 0, rpm: 0 };

export const DEFAULT_AUDIO: AudioSettings = { volume: 0.7, mute: false };

/** What a request was about, so an error reply can be handled (e.g. re-fetch a table). */
interface PendingRequest {
  type: ClientMessage['type'];
  tableId?: TableId;
}

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
  private pedal = 0;
  /** Description of the tune at the start of the running sweep, for the run's note. */
  private sweepTune: string | null = null;
  private readonly pending = new Map<string, PendingRequest>();

  constructor(source: SimSource, audio: AudioSettings = DEFAULT_AUDIO) {
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
      toast: null,
      sourceAddress: source.address,
      audio,
      overrun: null,
      originalOverrun: null,
    };
  }

  start(): void {
    this.unsubscribers.push(
      this.source.onMessage((message) => {
        this.handle(message);
      }),
      this.source.onStatus((status) => {
        this.handleStatus(status);
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
    this.pedal = value;
    this.send({ type: 'setPedal', payload: { value } });
  }

  setAudio(audio: AudioSettings): void {
    const volume = Math.min(Math.max(audio.volume, 0), 1);
    this.update({ audio: { volume, mute: audio.mute } });
    this.send({ type: 'setAudio', payload: { volume, mute: audio.mute } });
  }

  startSweep(request: StartSweepPayload): void {
    this.sweepTune = this.describeTune();
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
    this.history.record([{ tableId, before, after: [...changes] }]);
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

  /**
   * Changes the overrun settings (optimistic, like table edits); simcore confirms by
   * broadcasting overrunSettings, or rejects with an error and the store asks again.
   */
  setOverrun(settings: OverrunSettings): void {
    this.update({ overrun: settings });
    this.send({ type: 'setOverrunSettings', payload: settings });
  }

  /** The editable tables (and overrun settings) as a tune file, or null while unknown. */
  exportTune(savedAt: Date): TuneFile | null {
    const engine = this.snapshot.hello?.engine.id;
    if (engine === undefined || this.snapshot.tables.size === 0) {
      return null;
    }
    return createTuneFile(
      engine,
      this.snapshot.tables.values(),
      savedAt,
      this.snapshot.overrun ?? undefined,
    );
  }

  /**
   * Loads a tune file: validates everything first (throws TuneFileError), then sends the
   * differing cells as one undo step. Returns the number of changed cells.
   */
  importTune(text: string): number {
    const engine = this.snapshot.hello?.engine.id;
    if (engine === undefined) {
      throw new Error('no engine connected');
    }
    const tune = parseTuneFile(text);
    const changes = tuneChanges(tune, engine, this.snapshot.tables);
    this.applyStep(changes);
    if (tune.overrun !== undefined) {
      this.setOverrun(tune.overrun); // settings are not part of the undo history
    }
    return changes.reduce((sum, change) => sum + change.cells.length, 0);
  }

  /**
   * Restores tables (one undo step) and overrun settings as they came from the simulation.
   */
  revertToOriginal(): void {
    const changes = [...this.snapshot.tables.values()].flatMap((table) => {
      const original = this.snapshot.originalTables.get(table.id);
      return original === undefined || !table.editable
        ? []
        : [{ tableId: table.id, cells: differingCells(original, table) }];
    });
    this.applyStep(changes.filter((change) => change.cells.length > 0));
    if (this.snapshot.originalOverrun !== null) {
      this.setOverrun(this.snapshot.originalOverrun);
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

  notify(tone: Toast['tone'], text: string): void {
    this.update({ toast: { tone, text } });
  }

  dismissToast(): void {
    this.update({ toast: null });
  }

  // --- incoming messages -----------------------------------------------------------------

  private handleStatus(status: ConnectionStatus): void {
    if (status !== 'connected') {
      // Nothing is known about the simulation until the next hello; gauges fall back to zero.
      this.state = null;
      this.pending.clear();
      this.update({ status, sweep: IDLE_SWEEP });
      return;
    }
    this.update({ status });
  }

  private handle(message: ServerMessage): void {
    const request = message.id === undefined ? undefined : this.pending.get(message.id);
    if (message.id !== undefined) {
      this.pending.delete(message.id);
    }
    switch (message.type) {
      case 'hello':
        this.handleHello(message.payload);
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
        this.handleError(message.payload.message, request);
        return;
      case 'overrunSettings':
        this.update({
          overrun: message.payload,
          originalOverrun: this.snapshot.originalOverrun ?? message.payload,
        });
        return;
      case 'ack':
        return;
    }
  }

  /**
   * After every (re)connect the simulation is the source of truth: fetch all tables again and
   * repeat the pedal and the sound settings, which may have been lost while disconnected.
   */
  private handleHello(hello: HelloPayload): void {
    const engineChanged = this.snapshot.hello?.engine.id !== hello.engine.id;
    this.update({ hello, ...(engineChanged ? { originalTables: new Map() } : {}) });
    for (const table of hello.tables) {
      this.send({ type: 'getTable', payload: { id: table.id } });
    }
    this.send({ type: 'setPedal', payload: { value: this.pedal } });
    this.send({ type: 'setAudio', payload: this.snapshot.audio });
    this.send({ type: 'getOverrunSettings', payload: {} });
  }

  /** A rejected table edit leaves the optimistic local copy wrong, so fetch the real table. */
  private handleError(message: string, request: PendingRequest | undefined): void {
    this.notify('error', message);
    if (request?.type === 'setTableCells' && request.tableId !== undefined) {
      this.send({ type: 'getTable', payload: { id: request.tableId } });
    }
    if (request?.type === 'setOverrunSettings') {
      this.send({ type: 'getOverrunSettings', payload: {} });
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
    const note = this.sweepTune ?? this.describeTune();
    this.sweepTune = null;
    this.update({ runs: addRun(this.snapshot.runs, runNumber, result, note), sweep: IDLE_SWEEP });
  }

  // --- helpers ---------------------------------------------------------------------------

  private describeTune(): string {
    return describeDifferences(tuneDifferences(this.snapshot.tables, this.snapshot.originalTables));
  }

  /** Records several table changes as one undo step and sends them. */
  private applyStep(changes: readonly { tableId: TableId; cells: CellValue[] }[]): void {
    const step = changes.flatMap(({ tableId, cells }) => {
      const table = this.snapshot.tables.get(tableId);
      return table === undefined
        ? []
        : [{ tableId, before: readCells(table, cells), after: cells }];
    });
    this.history.record(step);
    this.replay(step, 'after');
  }

  private replay(step: EditStep | undefined, side: 'before' | 'after'): void {
    for (const edit of step ?? []) {
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
    const tableId =
      message.type === 'setTableCells' || message.type === 'getTable'
        ? message.payload.id
        : undefined;
    this.pending.set(id, { type: message.type, tableId });
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
