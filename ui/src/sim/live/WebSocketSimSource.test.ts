import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import type { ServerMessage } from '../../protocol/messages.ts';
import type { ConnectionStatus } from '../SimSource.ts';
import { WebSocketSimSource, type WebSocketLike } from './WebSocketSimSource.ts';

/** Test double for the browser WebSocket: the test opens, closes and feeds it by hand. */
class FakeSocket implements WebSocketLike {
  readyState = 0;
  sent: string[] = [];
  closed = false;
  onopen: ((event: Event) => void) | null = null;
  onclose: ((event: CloseEvent) => void) | null = null;
  onerror: ((event: Event) => void) | null = null;
  onmessage: ((event: MessageEvent) => void) | null = null;

  send(data: string): void {
    this.sent.push(data);
  }

  close(): void {
    this.closed = true;
    this.readyState = 3;
  }

  serverOpens(): void {
    this.readyState = 1;
    this.onopen?.(new Event('open'));
  }

  serverSends(data: unknown): void {
    const text = typeof data === 'string' ? data : JSON.stringify(data);
    this.onmessage?.(new MessageEvent('message', { data: text }));
  }

  serverCloses(): void {
    this.readyState = 3;
    // Node has no global CloseEvent before v23; the client never reads the event.
    this.onclose?.(new Event('close') as CloseEvent);
  }
}

interface Harness {
  source: WebSocketSimSource;
  sockets: FakeSocket[];
  statuses: ConnectionStatus[];
  messages: ServerMessage[];
  invalid: string[];
  current: () => FakeSocket;
}

function harness(): Harness {
  const sockets: FakeSocket[] = [];
  const statuses: ConnectionStatus[] = [];
  const messages: ServerMessage[] = [];
  const invalid: string[] = [];
  const source = new WebSocketSimSource({
    url: 'ws://test',
    createSocket: () => {
      const socket = new FakeSocket();
      sockets.push(socket);
      return socket;
    },
    onInvalidMessage: (reason) => invalid.push(reason),
  });
  source.onStatus((status) => statuses.push(status));
  source.onMessage((message) => messages.push(message));
  const current = (): FakeSocket => {
    const socket = sockets.at(-1);
    if (socket === undefined) throw new Error('no socket created');
    return socket;
  };
  return { source, sockets, statuses, messages, invalid, current };
}

describe('WebSocketSimSource', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('reports connecting and connected', () => {
    const h = harness();
    h.source.connect();
    h.current().serverOpens();
    expect(h.statuses).toEqual(['connecting', 'connected']);
  });

  it('sends commands as JSON only while connected', () => {
    const h = harness();
    h.source.connect();
    h.source.send({ v: 1, type: 'setPedal', payload: { value: 0.5 } });
    expect(h.current().sent).toEqual([]);
    h.current().serverOpens();
    h.source.send({ v: 1, type: 'setPedal', id: '1', payload: { value: 0.5 } });
    expect(JSON.parse(h.current().sent[0] ?? '')).toEqual({
      v: 1,
      type: 'setPedal',
      id: '1',
      payload: { value: 0.5 },
    });
  });

  it('forwards valid messages and reports invalid ones', () => {
    const h = harness();
    h.source.connect();
    h.current().serverOpens();
    h.current().serverSends({ v: 1, type: 'ack', id: '4', payload: {} });
    h.current().serverSends('garbage');
    h.current().serverSends({ v: 1, type: 'somethingNew', payload: {} });
    expect(h.messages.map((m) => m.type)).toEqual(['ack']);
    expect(h.invalid).toEqual(['not JSON']);
  });

  it('reconnects with exponential backoff and resets it after success', () => {
    const h = harness();
    h.source.connect();
    h.current().serverCloses(); // simcore not running
    expect(h.sockets).toHaveLength(1);
    vi.advanceTimersByTime(499);
    expect(h.sockets).toHaveLength(1);
    vi.advanceTimersByTime(1);
    expect(h.sockets).toHaveLength(2);

    h.current().serverCloses();
    vi.advanceTimersByTime(999);
    expect(h.sockets).toHaveLength(2);
    vi.advanceTimersByTime(1);
    expect(h.sockets).toHaveLength(3);

    h.current().serverOpens();
    h.current().serverCloses(); // simcore restarted: back to the short delay
    vi.advanceTimersByTime(500);
    expect(h.sockets).toHaveLength(4);
  });

  it('caps the reconnect delay', () => {
    const h = harness();
    h.source.connect();
    for (let attempt = 0; attempt < 10; attempt++) {
      h.current().serverCloses();
      vi.advanceTimersByTime(5000);
    }
    const before = h.sockets.length;
    h.current().serverCloses();
    vi.advanceTimersByTime(5000);
    expect(h.sockets.length).toBe(before + 1);
  });

  it('stops reconnecting after disconnect and closes the socket', () => {
    const h = harness();
    h.source.connect();
    h.current().serverOpens();
    const socket = h.current();
    h.source.disconnect();
    expect(socket.closed).toBe(true);
    expect(h.statuses.at(-1)).toBe('disconnected');
    vi.advanceTimersByTime(60_000);
    expect(h.sockets).toHaveLength(1);
  });

  it('can connect again after disconnect', () => {
    const h = harness();
    h.source.connect();
    h.source.disconnect();
    h.source.connect();
    expect(h.sockets).toHaveLength(2);
  });

  it('retries when the socket cannot even be created', () => {
    let attempts = 0;
    const source = new WebSocketSimSource({
      createSocket: () => {
        attempts++;
        throw new SyntaxError('invalid url');
      },
    });
    source.connect();
    vi.advanceTimersByTime(500);
    expect(attempts).toBe(2);
    source.disconnect();
  });
});
