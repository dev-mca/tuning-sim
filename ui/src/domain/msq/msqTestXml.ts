/** Test helper: builds small synthetic .msq documents inline (no real ECU files, ADR 0012). */

export interface ConstantSpec {
  name: string;
  units?: string;
  rows?: number;
  cols?: number;
  /** Text content as it should appear in the file. */
  content: string;
}

export function constant(
  name: string,
  units: string,
  values: readonly (readonly number[])[] | readonly number[],
): ConstantSpec {
  const is2d = values.length > 0 && Array.isArray(values[0]);
  const rows = is2d ? (values as readonly (readonly number[])[]) : [values as readonly number[]];
  return is2d
    ? {
        name,
        units,
        rows: rows.length,
        cols: rows[0]?.length ?? 0,
        content: rows.map((row) => row.join(' ')).join('\n'),
      }
    : { name, units, rows: rows[0]?.length ?? 0, cols: 1, content: rows[0]?.join('\n') ?? '' };
}

export function msqXml(constants: readonly ConstantSpec[]): string {
  const body = constants
    .map((spec) => {
      const attributes = [
        `name="${spec.name}"`,
        spec.units === undefined ? '' : `units="${spec.units}"`,
        spec.rows === undefined ? '' : `rows="${String(spec.rows)}"`,
        spec.cols === undefined ? '' : `cols="${String(spec.cols)}"`,
      ]
        .filter((attribute) => attribute !== '')
        .join(' ');
      return `    <constant ${attributes}>${spec.content}</constant>`;
    })
    .join('\n');
  return [
    '<?xml version="1.0" encoding="UTF-8"?>',
    '<msq xmlns="http://www.msefi.com/:msq">',
    '  <page number="0">',
    body,
    '  </page>',
    '</msq>',
  ].join('\n');
}
