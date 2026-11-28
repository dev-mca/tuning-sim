"""Roller dyno run ("Durchzug"): full throttle in one gear, wheel power over engine speed.

The car starts locked in gear a little below the first sample speed and accelerates freely on
the road-load rollers (research.md §8.4). The roller measures the drive force at the tyres;
wheel power = F * v. Divided by the overall ratio the wheel torque becomes an engine-equivalent
torque, which is lower than the engine's own torque by the drivetrain losses and the torque
that accelerates the engine's inertia: exactly the difference a roller dyno shows.
"""

from dataclasses import dataclass

import numpy as np

from tuning_lab import units
from tuning_lab.car import CarSimulation
from tuning_lab.ecu import Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.units import FloatArray

RUN_UP_RPM = 300.0
"""The run starts this far below the first sample so that the manifold has settled."""
DEFAULT_SAMPLE_STEP_RPM = 100.0
MAX_RUN_TIME_S = 60.0


@dataclass(frozen=True)
class RollerResult:
    gear: int
    speed_rad_s: FloatArray
    """Engine speed at each sample."""
    vehicle_speed_m_s: FloatArray
    wheel_force_n: FloatArray
    wheel_torque_nm: FloatArray
    engine_torque_nm: FloatArray
    """Brake torque of the engine model at the same instant (for comparison)."""
    time_s: FloatArray

    @property
    def wheel_power_w(self) -> FloatArray:
        return self.wheel_force_n * self.vehicle_speed_m_s


def run_roller(
    definition: EngineDefinition,
    gear: int,
    speed_from_rpm: float,
    speed_to_rpm: float,
    tune: Tune | None = None,
    sample_step_rpm: float = DEFAULT_SAMPLE_STEP_RPM,
) -> RollerResult:
    if not 1 <= gear <= definition.drivetrain.gear_count:
        raise ValueError("gear must be a forward gear")
    if speed_to_rpm <= speed_from_rpm:
        raise ValueError("need speed_to_rpm > speed_from_rpm")
    car = CarSimulation(definition, tune)
    start = units.rpm_to_rad_s(speed_from_rpm - RUN_UP_RPM)
    car.engine.set_speed_rad_s(start)
    car.drivetrain.place_in_gear(gear, start)
    sample_rpm = np.arange(speed_from_rpm, speed_to_rpm + 0.5 * sample_step_rpm,
                           sample_step_rpm)  # fmt: skip
    rows: list[tuple[float, ...]] = []
    steps = round(MAX_RUN_TIME_S / car.engine.time_step_s)
    for _ in range(steps):
        state = car.step(1.0)
        rpm = units.rad_s_to_rpm(state.engine.speed_rad_s)
        while len(rows) < len(sample_rpm) and rpm >= sample_rpm[len(rows)]:
            drivetrain = state.drivetrain
            rows.append(
                (
                    state.engine.speed_rad_s,
                    drivetrain.vehicle_speed_m_s,
                    drivetrain.wheel_force_n,
                    drivetrain.wheel_torque_nm,
                    state.engine.engine.brake_torque_nm,
                    state.engine.time_s,
                )
            )
        if len(rows) == len(sample_rpm):
            break
    columns = np.array(rows, dtype=float).T
    return RollerResult(gear, *columns)
