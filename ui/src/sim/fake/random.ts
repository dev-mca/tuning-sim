/**
 * Small seeded pseudo-random generator (mulberry32, public-domain algorithm by Tommy Ettinger).
 * Math.random() cannot be seeded; with a fixed seed the fake data is the same on every run,
 * which keeps tests deterministic (plan.md §2.3: determinism).
 */
export function createRandom(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** Uniform noise in [-amplitude, +amplitude]. */
export function noise(random: () => number, amplitude: number): number {
  return (random() * 2 - 1) * amplitude;
}
