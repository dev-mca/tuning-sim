import { describe, expect, it } from 'vitest';

import { parseServerMessage } from './parse.ts';

describe('parseServerMessage', () => {
  it('accepts a valid state message', () => {
    const result = parseServerMessage(
      JSON.stringify({ v: 1, type: 'state', payload: { t_s: 1, rpm: 800, map_kPa: 30 } }),
    );
    expect(result.kind).toBe('message');
  });

  it('defaults a missing payload to an empty object, like simcore', () => {
    const result = parseServerMessage(JSON.stringify({ v: 1, type: 'ack', id: '3' }));
    expect(result).toEqual({
      kind: 'message',
      message: { v: 1, type: 'ack', id: '3', payload: {} },
    });
  });

  it('accepts overrun settings and rejects them without a boolean fuel cut', () => {
    const valid =
      '{"v":1,"type":"overrunSettings","payload":{"fuelCut":false,"retard_deg":35,"enrichment_pct":10}}';
    expect(parseServerMessage(valid).kind).toBe('message');
    const invalid =
      '{"v":1,"type":"overrunSettings","payload":{"fuelCut":"no","retard_deg":35,"enrichment_pct":10}}';
    expect(parseServerMessage(invalid).kind).toBe('invalid');
  });

  it('ignores unknown message types (forward compatibility)', () => {
    expect(parseServerMessage('{"v":1,"type":"turboState","payload":{}}').kind).toBe('ignored');
  });

  it.each([
    ['not JSON', 'nope'],
    ['an array', '[1,2]'],
    ['another protocol version', '{"v":2,"type":"state","payload":{}}'],
    ['a missing type', '{"v":1,"payload":{}}'],
    ['a non-object payload', '{"v":1,"type":"ack","payload":3}'],
    ['a numeric id', '{"v":1,"type":"ack","id":7,"payload":{}}'],
    ['a state without rpm', '{"v":1,"type":"state","payload":{"t_s":1,"map_kPa":30}}'],
    [
      'a table without values',
      '{"v":1,"type":"table","payload":{"id":"ve","xAxis":{},"yAxis":{}}}',
    ],
  ])('rejects %s', (_, text) => {
    expect(parseServerMessage(text).kind).toBe('invalid');
  });
});
