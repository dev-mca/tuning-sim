import type { EngineState } from '../../protocol/messages.ts';

/** "N" in neutral, otherwise the gear number; "–" before simcore reports a gearbox. */
export function gearLabel(gear: number | null | undefined): string {
  if (gear === undefined || gear === null) {
    return '–';
  }
  return gear === 0 ? 'N' : String(gear);
}

export type ClutchStatus = 'pressed' | 'slipping' | 'locked' | 'open';

/**
 * What the clutch is doing, as a driver would say it: pedal pressed (open), slipping while it
 * closes or under too much torque, locked (engine and car turn together), or open in neutral.
 */
export function clutchStatus(state: EngineState): ClutchStatus {
  if (state.gear === 0 || state.gear === undefined || state.gear === null) {
    return 'open';
  }
  if (state.clutchLocked === true) {
    return 'locked';
  }
  return state.clutch === 0 ? 'pressed' : 'slipping';
}
