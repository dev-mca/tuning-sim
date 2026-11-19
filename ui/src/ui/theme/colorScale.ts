import { HEATMAP_RAMP } from './palette.ts';

export interface CellColors {
  fill: string;
  /** Dark or light text, whichever is readable on `fill`. */
  text: string;
}

interface Rgb {
  r: number;
  g: number;
  b: number;
}

const DARK_TEXT = '#0b0d10';
const LIGHT_TEXT = '#f4f5f7';
/** Above this relative luminance dark text has more contrast than light text (WCAG crossover). */
const TEXT_SWITCH_LUMINANCE = 0.179;

/**
 * Colour for a normalised value t in [0, 1] (clamped), interpolated between the ramp steps.
 * NaN maps to the lowest step so a broken value never crashes the renderer.
 */
export function sequentialColor(t: number, ramp: readonly string[] = HEATMAP_RAMP): CellColors {
  const clamped = Number.isNaN(t) ? 0 : Math.min(Math.max(t, 0), 1);
  const position = clamped * (ramp.length - 1);
  const index = Math.min(Math.floor(position), ramp.length - 2);
  const lower = parseHex(ramp[index] ?? '#000000');
  const upper = parseHex(ramp[index + 1] ?? ramp[index] ?? '#000000');
  const f = position - index;
  const rgb = {
    r: lower.r + (upper.r - lower.r) * f,
    g: lower.g + (upper.g - lower.g) * f,
    b: lower.b + (upper.b - lower.b) * f,
  };
  return {
    fill: toHex(rgb),
    text: relativeLuminance(rgb) > TEXT_SWITCH_LUMINANCE ? DARK_TEXT : LIGHT_TEXT,
  };
}

/** Normalises value into [0, 1] over [min, max]; a flat range maps to the middle. */
export function normalise(value: number, min: number, max: number): number {
  return max === min ? 0.5 : (value - min) / (max - min);
}

/** WCAG 2 relative luminance of an sRGB colour. */
export function relativeLuminance({ r, g, b }: Rgb): number {
  const channel = (value: number): number => {
    const c = value / 255;
    return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
  };
  return 0.2126 * channel(r) + 0.7152 * channel(g) + 0.0722 * channel(b);
}

export function parseHex(hex: string): Rgb {
  const match = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(hex);
  if (match === null) {
    throw new Error(`not a #rrggbb colour: '${hex}'`);
  }
  const [, r = '0', g = '0', b = '0'] = match;
  return { r: parseInt(r, 16), g: parseInt(g, 16), b: parseInt(b, 16) };
}

function toHex({ r, g, b }: Rgb): string {
  const part = (value: number): string => Math.round(value).toString(16).padStart(2, '0');
  return `#${part(r)}${part(g)}${part(b)}`;
}
