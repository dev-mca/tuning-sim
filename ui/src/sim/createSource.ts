import { FakeSimSource } from './fake/FakeSimSource.ts';
import { DEFAULT_SIMCORE_URL, WebSocketSimSource } from './live/WebSocketSimSource.ts';
import type { SimSource, SourceKind } from './SimSource.ts';

const STORAGE_KEY = 'tuning-sim.source';
const SOURCE_PARAM = 'source';

/** simcore address; override with VITE_SIMCORE_URL in ui/.env.local, e.g. for another port. */
export function simcoreUrl(): string {
  const configured: unknown = import.meta.env.VITE_SIMCORE_URL;
  return typeof configured === 'string' && configured !== '' ? configured : DEFAULT_SIMCORE_URL;
}

export function createSource(kind: SourceKind): SimSource {
  return kind === 'live' ? new WebSocketSimSource({ url: simcoreUrl() }) : new FakeSimSource();
}

function isSourceKind(value: unknown): value is SourceKind {
  return value === 'live' || value === 'fake';
}

/**
 * Which source to start with: ?source=fake|live in the URL wins, then the last choice of this
 * browser, then live (the real simulation). Storage may be blocked, hence the try/catch.
 */
export function initialSourceKind(search: string = window.location.search): SourceKind {
  const fromUrl = new URLSearchParams(search).get(SOURCE_PARAM);
  if (isSourceKind(fromUrl)) {
    return fromUrl;
  }
  try {
    const stored = window.localStorage.getItem(STORAGE_KEY);
    if (isSourceKind(stored)) {
      return stored;
    }
  } catch {
    // Storage unavailable (private mode, blocked site data): fall through to the default.
  }
  return 'live';
}

export function rememberSourceKind(kind: SourceKind): void {
  try {
    window.localStorage.setItem(STORAGE_KEY, kind);
  } catch {
    // Not remembering the choice is acceptable.
  }
}
