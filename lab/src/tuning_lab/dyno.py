"""Engine dyno: brake with a speed controller, holdRpm and sweep (docs/research.md §2.12).

The brake is an absorbing dyno (eddy current): it can only take torque, never drive the
engine. A PI controller sets the brake torque so that the engine speed follows the setpoint.
Measured torque = brake torque + J * d(omega)/dt (inertia correction during the ramp),
filtered with a first-order low-pass (tau = 50 ms, research.md §2.12 and §9).
"""

from dataclasses import dataclass

import numpy as np

from tuning_lab import units
from tuning_lab.ecu import Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.mvm import torque_to_bmep_pa
from tuning_lab.simulation import EngineSimulation, SimulationState
from tuning_lab.table import Table2D
from tuning_lab.units import FloatArray

CONTROL_BANDWIDTH_RAD_S = 20.0
"""Closed-loop bandwidth of the speed controller (about 3 Hz): fast enough to follow a sweep
ramp, slow compared with the 1 kHz simulation."""

MAX_BRAKE_TORQUE_NM = 1000.0
MEASUREMENT_FILTER_TIME_CONSTANT_S = 0.05
DEFAULT_SETTLE_TIME_S = 3.0
DEFAULT_SWEEP_RATE_RPM_S = 300.0
DEFAULT_SAMPLE_STEP_RPM = 100.0


class SpeedController:
    """PI controller with anti-windup: brake torque from speed error (research.md §9).

    Gains from the plant J d(omega)/dt = T: Kp = J * w_c, Ki = Kp * w_c / 4
    gives a well-damped loop with bandwidth w_c.
    """

    def __init__(self, inertia_kg_m2: float, bandwidth_rad_s: float = CONTROL_BANDWIDTH_RAD_S):
        self._kp = inertia_kg_m2 * bandwidth_rad_s
        self._ki = self._kp * bandwidth_rad_s / 4.0
        self._integral_nm = 0.0

    def preset_integral(self, torque_nm: float) -> None:
        """Starts the integrator at an expected torque (bumpless start)."""
        self._integral_nm = torque_nm

    def update(self, speed_rad_s: float, setpoint_rad_s: float, dt: float) -> float:
        # Positive error = engine too fast = more brake torque.
        error = speed_rad_s - setpoint_rad_s
        unclamped = self._kp * error + self._integral_nm + self._ki * error * dt
        output = min(max(unclamped, 0.0), MAX_BRAKE_TORQUE_NM)
        # Anti-windup: only integrate while the output is not saturated.
        if output == unclamped:
            self._integral_nm += self._ki * error * dt
        return output


class LowPassFilter:
    """First-order low-pass, y += alpha (x - y), alpha = dt / (tau + dt) (research.md §9)."""

    def __init__(self, time_constant_s: float, dt: float) -> None:
        self._alpha = dt / (time_constant_s + dt)
        self._value: float | None = None

    def update(self, sample: float) -> float:
        if self._value is None:
            self._value = sample
        else:
            self._value += self._alpha * (sample - self._value)
        return self._value


@dataclass(frozen=True)
class SweepResult:
    """Dyno sweep sampled at fixed setpoint speeds (SI units, numpy arrays)."""

    setpoint_rad_s: FloatArray
    speed_rad_s: FloatArray
    measured_torque_nm: FloatArray
    """What the dyno measures: brake torque + inertia correction, filtered."""
    engine_torque_nm: FloatArray
    """Brake torque of the engine model (no measurement effects)."""
    manifold_pressure_pa: FloatArray
    lambda_actual: FloatArray
    ignition_deg: FloatArray
    air_mass_flow_kg_s: FloatArray
    exhaust_temperature_k: FloatArray

    @property
    def power_w(self) -> FloatArray:
        """P = T * omega (research.md §2.9)."""
        return self.measured_torque_nm * self.speed_rad_s

    def bmep_pa(self, displacement_m3: float) -> FloatArray:
        return torque_to_bmep_pa(1.0, displacement_m3) * self.measured_torque_nm


def run_hold(
    definition: EngineDefinition,
    speed_rpm: float,
    throttle: float,
    duration_s: float,
    tune: Tune | None = None,
) -> SimulationState:
    """holdRpm: brakes the engine to a constant speed and returns the final state."""
    setpoint = units.rpm_to_rad_s(speed_rpm)
    simulation = EngineSimulation(definition, tune, initial_speed_rad_s=setpoint)
    controller = SpeedController(definition.inertia_kg_m2)
    steps = round(duration_s / simulation.time_step_s)
    load = 0.0
    state: SimulationState | None = None
    for _ in range(steps):
        state = simulation.step(throttle, load)
        load = controller.update(state.speed_rad_s, setpoint, simulation.time_step_s)
    if state is None:
        raise ValueError("duration_s must cover at least one time step")
    return state


def run_sweep(
    definition: EngineDefinition,
    speed_from_rpm: float,
    speed_to_rpm: float,
    rate_rpm_s: float = DEFAULT_SWEEP_RATE_RPM_S,
    throttle: float = 1.0,
    tune: Tune | None = None,
    true_ve: Table2D | None = None,
    sample_step_rpm: float = DEFAULT_SAMPLE_STEP_RPM,
    settle_time_s: float = DEFAULT_SETTLE_TIME_S,
) -> SweepResult:
    """sweep: hold the start speed until settled, then ramp the setpoint (research.md §2.12).

    Samples are taken whenever the setpoint passes the next multiple of sample_step_rpm, so
    the result is deterministic and independent of the time step.
    """
    if speed_to_rpm <= speed_from_rpm or rate_rpm_s <= 0.0:
        raise ValueError("need speed_to_rpm > speed_from_rpm and rate_rpm_s > 0")
    start = units.rpm_to_rad_s(speed_from_rpm)
    simulation = EngineSimulation(definition, tune, true_ve, initial_speed_rad_s=start)
    dt = simulation.time_step_s
    controller = SpeedController(definition.inertia_kg_m2)
    torque_filter = LowPassFilter(MEASUREMENT_FILTER_TIME_CONSTANT_S, dt)
    sample_setpoints_rpm = np.arange(speed_from_rpm, speed_to_rpm + 0.5 * sample_step_rpm,
                                     sample_step_rpm)  # fmt: skip
    rows: list[tuple[float, ...]] = []

    settle_steps = round(settle_time_s / dt)
    ramp_steps = round((speed_to_rpm - speed_from_rpm) / rate_rpm_s / dt)
    load = 0.0
    previous_speed = start
    next_sample = 0
    for step_index in range(settle_steps + ramp_steps + 1):
        ramp_time = max(0, step_index - settle_steps) * dt
        setpoint_rpm = min(speed_from_rpm + rate_rpm_s * ramp_time, speed_to_rpm)
        state = simulation.step(throttle, load)
        acceleration = (state.speed_rad_s - previous_speed) / dt
        previous_speed = state.speed_rad_s
        measured = torque_filter.update(load + definition.inertia_kg_m2 * acceleration)
        load = controller.update(state.speed_rad_s, units.rpm_to_rad_s(setpoint_rpm), dt)

        is_ramping = step_index >= settle_steps
        if (
            is_ramping
            and next_sample < len(sample_setpoints_rpm)
            and setpoint_rpm >= sample_setpoints_rpm[next_sample] - 1.0e-9
        ):
            engine = state.engine
            rows.append(
                (
                    units.rpm_to_rad_s(float(sample_setpoints_rpm[next_sample])),
                    state.speed_rad_s,
                    measured,
                    engine.brake_torque_nm,
                    state.manifold_pressure_pa,
                    engine.lambda_actual,
                    state.command.ignition_deg,
                    engine.air_mass_flow_kg_s,
                    engine.exhaust_temperature_k,
                )
            )
            next_sample += 1

    columns = np.array(rows, dtype=np.float64).T
    return SweepResult(*columns)
