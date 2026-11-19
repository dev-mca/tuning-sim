/**
 * Colours for canvas drawing (heatmap, uPlot). DOM elements use the CSS tokens in index.css;
 * the values below must stay in sync with them. Canvas cannot read Tailwind classes, hence the copy.
 */
export const SURFACE = {
  canvas: '#0b0d10',
  panel: '#14171c',
  raised: '#1b1f26',
  line: '#262a31',
  grid: '#1f232a',
  ink: '#e6e8eb',
  muted: '#8a919c',
  accent: '#ff8a1f',
  redline: '#ff4d2e',
} as const;

/**
 * Categorical colours for dyno runs, in fixed order (never cycled). Dark-mode steps of the
 * dataviz reference palette, validated against the panel surface #14171c with
 * validate_palette.js: all checks pass (worst adjacent CVD dE 8.4, normal-vision dE 19.3).
 */
export const RUN_COLORS = [
  '#3987e5',
  '#d95926',
  '#199e70',
  '#c98500',
  '#d55181',
  '#0ca30c',
  '#9085e9',
  '#e66767',
] as const;

export function runColor(slot: number): string {
  return RUN_COLORS[slot % RUN_COLORS.length] ?? SURFACE.ink;
}

/**
 * Sequential heatmap ramp: one hue (blue), dark = low, light = high. A single hue keeps the
 * order readable for everyone; rainbow scales invent borders that are not in the data.
 */
export const HEATMAP_RAMP = [
  '#0d366b',
  '#104281',
  '#184f95',
  '#1c5cab',
  '#256abf',
  '#2a78d6',
  '#3987e5',
  '#5598e7',
  '#6da7ec',
  '#86b6ef',
  '#9ec5f4',
  '#b7d3f6',
  '#cde2fb',
] as const;
