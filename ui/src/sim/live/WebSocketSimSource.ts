import type { ClientMessage, ServerMessage } from '../../protocol/messages.ts';
import { parseServerMessage } from '../../protocol/parse.ts';
import type { ConnectionStatus, SimSource } from '../SimSource.ts';

/** The part of the browser WebSocket API this client uses; tests pass a fake implementation. */
export interface WebSocketLike {
  readonly readyState: number;
  send(data: string): void;
  close(): void;
  onopen: ((event: Event) => void) | null;
  onclose: ((event: CloseEvent) => void) | null;
  onerror: ((event: Event) => void) | null;
  onmessage: ((event: MessageEvent) => void) | null;
}

export type WebSocketFactory = (url: string) => WebSocketLike;

/** WebSocket.OPEN; repeated here because the fake socket in tests has no static constants. */
const SOCKET_OPEN = 1;

/** Default address of simcore (docs/interface.md). */
export const DEFAULT_SIMCORE_URL = 'ws://127.0.0.1:47800';

export interface ReconnectPolicy {
  initialDelay_ms: number;
  maxDelay_ms: number;
  factor: number;
}

/**
 * Exponential backoff: 0.5 s, 1 s, 2 s, 4 s, then every 5 s. Fast when simcore was only
 * restarted, without flooding the console while it is not running at all.
 */
export const DEFAULT_RECONNECT: ReconnectPolicy = {
  initialDelay_ms: 500,
  maxDelay_ms: 5000,
  factor: 2,
};

export interface WebSocketSimSourceOptions {
  url?: string;
  createSocket?: WebSocketFactory;
  reconnect?: ReconnectPolicy;
  /** Receives frames that could not be parsed; default logs a warning. */
  onInvalidMessage?: (reason: string) => void;
}

/**
 * SimSource that talks to a running simcore over WebSocket (protocol v1).
 * Reconnects automatically until disconnect() is called. Messages sent while the connection is
 * down are dropped: the store re-synchronises after the next hello instead of replaying them.
 */
export class WebSocketSimSource implements SimSource {
  readonly kind = 'live';

  private readonly url: string;
  private readonly createSocket: WebSocketFactory;
  private readonly policy: ReconnectPolicy;
  private readonly onInvalidMessage: (reason: string) => void;
  private readonly messageListeners = new Set<(message: ServerMessage) => void>();
  private readonly statusListeners = new Set<(status: ConnectionStatus) => void>();
  private socket: WebSocketLike | null = null;
  private retryTimer: ReturnType<typeof setTimeout> | undefined;
  private retryDelay_ms: number;
  private wanted = false;

  constructor(options: WebSocketSimSourceOptions = {}) {
    this.url = options.url ?? DEFAULT_SIMCORE_URL;
    this.createSocket = options.createSocket ?? ((url) => new WebSocket(url));
    this.policy = options.reconnect ?? DEFAULT_RECONNECT;
    this.retryDelay_ms = this.policy.initialDelay_ms;
    this.onInvalidMessage =
      options.onInvalidMessage ??
      ((reason) => {
        console.warn(`simcore: ignored invalid message (${reason})`);
      });
  }

  get address(): string {
    return this.url;
  }

  connect(): void {
    if (this.wanted) {
      return;
    }
    this.wanted = true;
    this.open();
  }

  disconnect(): void {
    this.wanted = false;
    clearTimeout(this.retryTimer);
    this.retryTimer = undefined;
    const socket = this.socket;
    this.socket = null;
    if (socket !== null) {
      detach(socket);
      socket.close();
    }
    this.emitStatus('disconnected');
  }

  send(message: ClientMessage): void {
    if (this.socket?.readyState === SOCKET_OPEN) {
      this.socket.send(JSON.stringify(message));
    }
  }

  onMessage(listener: (message: ServerMessage) => void): () => void {
    this.messageListeners.add(listener);
    return () => this.messageListeners.delete(listener);
  }

  onStatus(listener: (status: ConnectionStatus) => void): () => void {
    this.statusListeners.add(listener);
    return () => this.statusListeners.delete(listener);
  }

  private open(): void {
    this.emitStatus('connecting');
    let socket: WebSocketLike;
    try {
      socket = this.createSocket(this.url);
    } catch {
      // e.g. an invalid URL; treat it like a failed connection attempt.
      this.scheduleReconnect();
      return;
    }
    this.socket = socket;
    socket.onopen = () => {
      this.retryDelay_ms = this.policy.initialDelay_ms;
      this.emitStatus('connected');
    };
    socket.onmessage = (event) => {
      this.receive(event.data);
    };
    socket.onclose = () => {
      if (this.socket === socket) {
        this.socket = null;
        this.scheduleReconnect();
      }
    };
    // Browsers always follow an error with close, so reconnecting is handled there.
    socket.onerror = null;
  }

  private scheduleReconnect(): void {
    this.emitStatus('disconnected');
    if (!this.wanted) {
      return;
    }
    const delay = this.retryDelay_ms;
    this.retryDelay_ms = Math.min(delay * this.policy.factor, this.policy.maxDelay_ms);
    this.retryTimer = setTimeout(() => {
      this.retryTimer = undefined;
      if (this.wanted) {
        this.open();
      }
    }, delay);
  }

  private receive(data: unknown): void {
    if (typeof data !== 'string') {
      this.onInvalidMessage('binary frame');
      return;
    }
    const result = parseServerMessage(data);
    if (result.kind === 'invalid') {
      this.onInvalidMessage(result.reason);
      return;
    }
    if (result.kind === 'message') {
      for (const listener of this.messageListeners) {
        listener(result.message);
      }
    }
  }

  private emitStatus(status: ConnectionStatus): void {
    for (const listener of this.statusListeners) {
      listener(status);
    }
  }
}

function detach(socket: WebSocketLike): void {
  socket.onopen = null;
  socket.onclose = null;
  socket.onerror = null;
  socket.onmessage = null;
}
