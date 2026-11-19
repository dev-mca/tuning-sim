import { PROTOCOL_VERSION, type ServerMessage } from './messages.ts';

/** Message types the UI understands; others are ignored as docs/interface.md requires. */
const KNOWN_TYPES: ReadonlySet<string> = new Set<ServerMessage['type']>([
  'hello',
  'state',
  'table',
  'ack',
  'error',
  'dynoProgress',
  'dynoResult',
]);

export type ParseResult =
  | { kind: 'message'; message: ServerMessage }
  | { kind: 'ignored'; reason: string }
  | { kind: 'invalid'; reason: string };

/**
 * Turns a WebSocket text frame into a ServerMessage. Checks the envelope (version, type,
 * payload object) and the fields the UI cannot live without. Data from a socket is untrusted
 * input, so a broken frame must not crash the UI.
 */
export function parseServerMessage(text: string): ParseResult {
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    return { kind: 'invalid', reason: 'not JSON' };
  }
  if (!isRecord(data)) {
    return { kind: 'invalid', reason: 'not a JSON object' };
  }
  if (data.v !== PROTOCOL_VERSION) {
    return { kind: 'invalid', reason: `unsupported protocol version ${String(data.v)}` };
  }
  if (typeof data.type !== 'string') {
    return { kind: 'invalid', reason: 'missing type' };
  }
  if (!KNOWN_TYPES.has(data.type)) {
    return { kind: 'ignored', reason: `unknown type '${data.type}'` };
  }
  const payload = data.payload ?? {};
  if (!isRecord(payload)) {
    return { kind: 'invalid', reason: 'payload is not an object' };
  }
  if (data.id !== undefined && typeof data.id !== 'string') {
    return { kind: 'invalid', reason: 'id is not a string' };
  }
  const missing = missingField(data.type, payload);
  if (missing !== null) {
    return { kind: 'invalid', reason: `${data.type}: missing or wrong field '${missing}'` };
  }
  // The envelope and the essential fields have been checked above; the rest is trusted.
  return { kind: 'message', message: { ...data, payload } as unknown as ServerMessage };
}

/** Fields per type that the UI reads without further checks. */
const REQUIRED_FIELDS: Readonly<
  Record<string, readonly [string, 'number' | 'string' | 'array' | 'object'][]>
> = {
  hello: [
    ['engine', 'object'],
    ['tables', 'array'],
  ],
  state: [
    ['t_s', 'number'],
    ['rpm', 'number'],
    ['map_kPa', 'number'],
  ],
  table: [
    ['id', 'string'],
    ['values', 'array'],
    ['xAxis', 'object'],
    ['yAxis', 'object'],
  ],
  error: [['message', 'string']],
  dynoProgress: [['progress', 'number']],
  dynoResult: [
    ['rpm', 'array'],
    ['torque_Nm', 'array'],
    ['power_kW', 'array'],
  ],
};

function missingField(type: string, payload: Record<string, unknown>): string | null {
  for (const [name, kind] of REQUIRED_FIELDS[type] ?? []) {
    const value = payload[name];
    const ok =
      kind === 'array'
        ? Array.isArray(value)
        : kind === 'object'
          ? isRecord(value)
          : typeof value === kind;
    if (!ok) {
      return name;
    }
  }
  return null;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}
