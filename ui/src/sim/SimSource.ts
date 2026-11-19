import type { ClientMessage, ServerMessage } from '../protocol/messages.ts';

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected';

export type SourceKind = 'fake' | 'live';

/**
 * Port to the simulation: the UI only talks to this interface (hexagonal architecture,
 * docs/adr/0001). FakeSimSource implements it now; the WebSocket client in iteration 6
 * implements it too, so nothing above this interface has to change.
 * Java analogy: an interface with two implementations, injected into the store.
 */
export interface SimSource {
  readonly kind: SourceKind;
  /** Human-readable location, e.g. the WebSocket URL; shown when the connection is down. */
  readonly address: string;
  connect(): void;
  disconnect(): void;
  send(message: ClientMessage): void;
  /** Registers a listener for incoming messages and returns a function that removes it. */
  onMessage(listener: (message: ServerMessage) => void): () => void;
  onStatus(listener: (status: ConnectionStatus) => void): () => void;
}
