/**
 * Fake engine for UI development without simcore (plan.md §6: fake mode).
 * It is deliberately simple: first-order lags towards plausible targets, so that the gauges and
 * the live cursor move like on a real engine. It is not physics; the numbers only need to have
 * the right magnitude and react to pedal and tables. Real physics lives in simcore.
 */
import { interpolate } from '../../domain/table2d.ts';
import { STOICHIOMETRIC_AFR_GASOLINE } from '../../domain/units.ts';
import type {
  EngineInfo,
  EngineState,
  LaunchSettings,
  StartSweepPayload,
} from '../../protocol/messages.ts';
import { smoothTowards } from '../../domain/smoothing.ts';
import {
  goldenAt,
  powerFromTorque_kW,
  requireTable,
  type TableSet,
  tuneEffect,
} from './fakeDyno.ts';
import { createRandom, noise } from './random.ts';
import { REVLIMIT_HYSTERESIS_RPM } from './referenceData.ts';

/** Manifold pressure at idle and at wide-open throttle of the NA reference engine. */
const MAP_IDLE_KPA = 30;
const MAP_WOT_KPA = 100;
/** Manifold pressure on overrun (throttle closed at high rpm). */
const MAP_OVERRUN_KPA = 20;
/** Above idle + this, a closed throttle means overrun with fuel cut (ECU, research.md §3). */
const OVERRUN_CUT_MARGIN_RPM = 400;
/** Free-revving target overshoots the limiter at full pedal, so the limiter becomes visible. */
const REV_TARGET_OVERSHOOT_RPM = 400;
/** How fast rpm falls while the limiter cuts fuel. */
const LIMITER_DROP_RPM_S = 3000;
/** Pedal above this counts as an open throttle for the 2-step (simcore: closedThrottle). */
const LAUNCH_MIN_PEDAL = 0.02;

const RPM_RISE_TIME_S = 0.45;
const RPM_FALL_TIME_S = 0.9;
const MAP_TIME_S = 0.08;
const EGT_TIME_S = 1.5;
const IAT_TIME_S = 6;

const EGT_IDLE_C = 380;
const EGT_FULL_LOAD_RISE_C = 420;
const EGT_PER_DEG_RETARD_C = 2.5;
const IAT_AMBIENT_C = 25;
const IAT_FULL_LOAD_RISE_C = 14;

const LAMBDA_NOISE = 0.004;
const CATALYST_SHARE_OF_EGT = 0.85;
const RPM_NOISE = 4;

/** Mechanical losses so that torque at a closed throttle becomes negative (engine braking). */
const FRICTION_BASE_NM = 12;
const FRICTION_PER_RPM_NM = 0.004;

export const DEFAULT_SWEEP_RATE_RPM_S = 300;

interface SweepState {
  rpmFrom: number;
  rpmTo: number;
  rate_rpm_s: number;
  pedal: number;
}

export class FakeEngine {
  private readonly engine: EngineInfo;
  private readonly random: () => number;
  private readonly reference: TableSet;
  private tune: TableSet;

  private time_s = 0;
  private pedal = 0;
  private rpm: number;
  private map_kPa = MAP_IDLE_KPA;
  private egt_C = EGT_IDLE_C;
  private iat_C = IAT_AMBIENT_C;
  private limiterCut = false;
  private sweep: SweepState | null = null;
  private launch: LaunchSettings = { enabled: false, launch_rpm: 4000, retard_deg: 0 };
  private launchSwitch = false;

  constructor(engine: EngineInfo, reference: TableSet, seed: number) {
    this.engine = engine;
    this.reference = reference;
    this.tune = reference;
    this.random = createRandom(seed);
    this.rpm = engine.idle_rpm;
  }

  setPedal(value: number): void {
    this.pedal = clamp01(value);
  }

  setLaunch(settings: LaunchSettings): void {
    this.launch = { ...settings };
  }

  setLaunchSwitch(held: boolean): void {
    this.launchSwitch = held;
  }

  setTune(tune: TableSet): void {
    this.tune = tune;
  }

  startSweep(request: StartSweepPayload): void {
    this.sweep = {
      rpmFrom: request.rpmFrom,
      rpmTo: request.rpmTo,
      rate_rpm_s: request.rate_rpm_s ?? DEFAULT_SWEEP_RATE_RPM_S,
      pedal: clamp01(request.pedal ?? 1),
    };
    this.rpm = request.rpmFrom;
  }

  stopSweep(): void {
    this.sweep = null;
  }

  get sweepActive(): boolean {
    return this.sweep !== null;
  }

  get sweepProgress(): number {
    if (this.sweep === null) {
      return 0;
    }
    const span = this.sweep.rpmTo - this.sweep.rpmFrom;
    return clamp01((this.rpm - this.sweep.rpmFrom) / span);
  }

  /** Advances the fake engine by dt. Returns true in the step in which a sweep finished. */
  step(dt_s: number): boolean {
    this.time_s += dt_s;
    const sweepFinished = this.sweep !== null ? this.stepSweep(dt_s, this.sweep) : false;
    if (this.sweep === null && !sweepFinished) {
      this.stepFreeRevving(dt_s);
    }
    this.map_kPa = smoothTowards(this.map_kPa, this.mapTarget(), dt_s, MAP_TIME_S);
    const load = this.load();
    const effect = tuneEffect(this.tune, this.reference, this.rpm, this.map_kPa);
    const egtTarget =
      EGT_IDLE_C + EGT_FULL_LOAD_RISE_C * load + EGT_PER_DEG_RETARD_C * effect.retard_deg;
    this.egt_C = smoothTowards(this.egt_C, egtTarget, dt_s, EGT_TIME_S);
    const iatTarget = IAT_AMBIENT_C + IAT_FULL_LOAD_RISE_C * load;
    this.iat_C = smoothTowards(this.iat_C, iatTarget, dt_s, IAT_TIME_S);
    return sweepFinished;
  }

  state(): EngineState {
    const effect = tuneEffect(this.tune, this.reference, this.rpm, this.map_kPa);
    const launchActive = this.launchActive();
    const sparkCut = launchActive && this.limiterCut;
    const fuelCut = (this.limiterCut && !launchActive) || this.overrun();
    const golden = goldenAt(this.rpm);
    const friction_Nm = FRICTION_BASE_NM + FRICTION_PER_RPM_NM * this.rpm;
    const burns = !fuelCut && !sparkCut;
    const combustion_Nm = burns ? golden.torque_Nm * this.load() * effect.torqueFactor : 0;
    const mapIgnition_deg = interpolate(
      requireTable(this.tune, 'ignition'),
      this.rpm,
      this.map_kPa,
    );
    const torque_Nm = combustion_Nm - friction_Nm;
    const airflow_kg_s = golden.airflow_kg_s * (this.map_kPa / golden.map_kPa);
    const lambda = fuelCut ? null : effect.lambda + noise(this.random, LAMBDA_NOISE);
    return {
      t_s: this.time_s,
      rpm: this.rpm + noise(this.random, RPM_NOISE),
      pedal: this.currentPedal(),
      map_kPa: this.map_kPa,
      lambda,
      lambdaTarget: effect.lambdaTarget,
      ignition_deg: launchActive ? mapIgnition_deg - this.launch.retard_deg : mapIgnition_deg,
      torque_Nm,
      power_kW: powerFromTorque_kW(torque_Nm, this.rpm),
      airflow_kg_s,
      fuelflow_kg_s: fuelCut
        ? 0
        : airflow_kg_s / (effect.lambdaTarget * STOICHIOMETRIC_AFR_GASOLINE),
      egt_C: this.egt_C,
      dynoMode: this.sweep !== null ? 'sweep' : 'free',
      sweepProgress: this.sweepProgress,
      cutActive: fuelCut && !this.overrun(),
      iat_C: this.iat_C,
      // Rough stand-in: the catalyst sits a little below the exhaust temperature.
      catalyst_C: this.egt_C * CATALYST_SHARE_OF_EGT,
      overrunFueling: false,
      launchSwitch: this.launchSwitch,
      launchActive,
      sparkCut,
      // Naturally aspirated, no gearbox yet: the agreed fields are not available (plan.md §11).
      boost_kPa: null,
      turboSpeed_rpm: null,
      wastegateDuty_pct: null,
      knockRetard_deg: 0,
      gear: null,
      clutch: null,
      vehicleSpeed_kmh: null,
      wheelTorque_Nm: null,
    };
  }

  private stepSweep(dt_s: number, sweep: SweepState): boolean {
    this.limiterCut = false;
    this.rpm = Math.min(this.rpm + sweep.rate_rpm_s * dt_s, sweep.rpmTo);
    if (this.rpm >= sweep.rpmTo) {
      this.sweep = null;
      return true;
    }
    return false;
  }

  private stepFreeRevving(dt_s: number): void {
    this.updateLimiter();
    if (this.limiterCut) {
      this.rpm -= LIMITER_DROP_RPM_S * dt_s;
      return;
    }
    const target = this.rpmTarget();
    const timeConstant = target > this.rpm ? RPM_RISE_TIME_S : RPM_FALL_TIME_S;
    this.rpm = smoothTowards(this.rpm, target, dt_s, timeConstant);
  }

  /**
   * Limiter with hysteresis, like simcore's ECU (research.md §3): fuel cut at the rev limit,
   * or spark cut at the launch speed while the 2-step works.
   */
  private updateLimiter(): void {
    const limit_rpm = this.launchActive() ? this.launch.launch_rpm : this.engine.revLimit_rpm;
    if (this.rpm >= limit_rpm) {
      this.limiterCut = true;
    } else if (this.rpm < limit_rpm - REVLIMIT_HYSTERESIS_RPM) {
      this.limiterCut = false;
    }
  }

  private launchActive(): boolean {
    return (
      this.launch.enabled &&
      this.launchSwitch &&
      this.sweep === null &&
      this.pedal > LAUNCH_MIN_PEDAL
    );
  }

  private rpmTarget(): number {
    const top = this.engine.revLimit_rpm + REV_TARGET_OVERSHOOT_RPM;
    return this.engine.idle_rpm + this.pedal ** 1.5 * (top - this.engine.idle_rpm);
  }

  private mapTarget(): number {
    if (this.overrun()) {
      return MAP_OVERRUN_KPA;
    }
    // A throttle opens progressively: half pedal already gives most of the airflow.
    const opening = 1 - (1 - this.currentPedal()) ** 2.5;
    return MAP_IDLE_KPA + (MAP_WOT_KPA - MAP_IDLE_KPA) * opening;
  }

  private overrun(): boolean {
    return (
      this.sweep === null &&
      this.pedal === 0 &&
      this.rpm > this.engine.idle_rpm + OVERRUN_CUT_MARGIN_RPM
    );
  }

  /** 0 at overrun pressure, 1 at wide-open throttle. */
  private load(): number {
    return clamp01((this.map_kPa - MAP_OVERRUN_KPA) / (MAP_WOT_KPA - MAP_OVERRUN_KPA));
  }

  private currentPedal(): number {
    return this.sweep !== null ? this.sweep.pedal : this.pedal;
  }
}

function clamp01(value: number): number {
  return Math.min(Math.max(value, 0), 1);
}
