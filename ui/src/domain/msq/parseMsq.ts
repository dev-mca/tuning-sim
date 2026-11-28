/**
 * Reader for TunerStudio .msq files (ADR 0012). An .msq is XML:
 * `<msq>` → `<page>` → `<constant name rows cols units>` with whitespace-separated values in
 * display units. This module only turns the XML into typed constants; which constant means
 * what is decided in msqMapping.ts.
 */

/** An .msq file that cannot be used; the message is shown to the user. */
export class MsqError extends Error {
  override readonly name = 'MsqError';
}

export interface MsqConstant {
  name: string;
  /** The `units` attribute as written in the file, '' if missing. */
  units: string;
  rows: number;
  cols: number;
  /**
   * Values in file order (row-major, row 0 first), or null if the content is not a list of
   * numbers (strings and option names such as `"Off"`), which the import never needs.
   */
  values: number[] | null;
}

export interface MsqDocument {
  /** Firmware signature from `<versionInfo signature>`, '' if missing; shown in warnings. */
  signature: string;
  constants: ReadonlyMap<string, MsqConstant>;
}

/**
 * Parses the XML of an .msq file. Uses the platform DOMParser (browser; jsdom in tests), so
 * entities, comments and namespaces are handled by a real XML parser.
 */
export function parseMsq(text: string, parser: DOMParser = new DOMParser()): MsqDocument {
  const doc = parser.parseFromString(text, 'application/xml');
  if (doc.getElementsByTagName('parsererror').length > 0) {
    throw new MsqError('the file is not well-formed XML');
  }
  if (doc.documentElement.localName !== 'msq') {
    throw new MsqError('this is not a TunerStudio .msq file (root element is not <msq>)');
  }
  const constants = new Map<string, MsqConstant>();
  // The root usually carries a default namespace (xmlns="http://www.msefi.com/:msq"); '*'
  // matches the element in any namespace.
  for (const element of doc.getElementsByTagNameNS('*', 'constant')) {
    const constant = readConstant(element);
    if (!constants.has(constant.name)) {
      constants.set(constant.name, constant); // first definition wins, like page order
    }
  }
  const versionInfo = doc.getElementsByTagNameNS('*', 'versionInfo')[0];
  return { signature: versionInfo?.getAttribute('signature') ?? '', constants };
}

function readConstant(element: Element): MsqConstant {
  const name = element.getAttribute('name');
  if (name === null || name === '') {
    throw new MsqError('a <constant> has no name');
  }
  return {
    name,
    units: element.getAttribute('units') ?? '',
    rows: readDimension(element, 'rows', name),
    cols: readDimension(element, 'cols', name),
    values: readNumbers(element.textContent),
  };
}

/** rows/cols default to 1 (scalar constants have neither). */
function readDimension(element: Element, attribute: 'rows' | 'cols', name: string): number {
  const text = element.getAttribute(attribute);
  if (text === null) {
    return 1;
  }
  const value = Number(text);
  if (!Number.isInteger(value) || value < 1) {
    throw new MsqError(`constant '${name}' has an invalid ${attribute} value '${text}'`);
  }
  return value;
}

function readNumbers(text: string | null): number[] | null {
  const tokens = (text ?? '')
    .trim()
    .split(/\s+/)
    .filter((token) => token !== '');
  if (tokens.length === 0) {
    return null;
  }
  const values = tokens.map(Number);
  return values.every((value) => Number.isFinite(value)) ? values : null;
}
