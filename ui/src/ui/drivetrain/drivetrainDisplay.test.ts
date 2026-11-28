import { describe, expect, it } from 'vitest';

import type { EngineState } from '../../protocol/messages.ts';
import { clutchStatus, gearLabel } from './drivetrainDisplay.ts';

const base: EngineState = {
  t_s: 0,
  rpm: 2000,
  pedal: 0,
  map_kPa: 40,
  lambda: 1,
  lambdaTarget: 1,
  ignition_deg: 30,
  torque_Nm: 0,
  power_kW: 0,
  airflow_kg_s: 0.01,
  fuelflow_kg_s: 0.001,
  egt_C: 500,
  dynoMode: 'free',
  sweepProgress: 0,
  cutActive: false,
};

describe('drivetrain display', () => {
  it('labels neutral, gears and a missing gearbox', () => {
    expect(gearLabel(0)).toBe('N');
    expect(gearLabel(4)).toBe('4');
    expect(gearLabel(null)).toBe('–');
    expect(gearLabel(undefined)).toBe('–');
  });

  it('tells pressed, slipping, locked and open apart', () => {
    expect(clutchStatus({ ...base, gear: 0, clutch: 1 })).toBe('open');
    expect(clutchStatus({ ...base, gear: 1, clutch: 0, clutchLocked: false })).toBe('pressed');
    expect(clutchStatus({ ...base, gear: 1, clutch: 0.5, clutchLocked: false })).toBe('slipping');
    expect(clutchStatus({ ...base, gear: 1, clutch: 1, clutchLocked: true })).toBe('locked');
  });
});
