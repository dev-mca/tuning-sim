"""Clutch, gearbox, car on the roller dyno, launch and flat shift (research.md §8, §3.4)."""

from dataclasses import replace
from itertools import pairwise

import pytest

from tuning_lab import units
from tuning_lab.car import CarSimulation, CarState
from tuning_lab.drivetrain import ClutchState, road_load_n
from tuning_lab.dyno import run_sweep
from tuning_lab.ecu import LaunchSettings, Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.roller import run_roller

STEPS_PER_SECOND = 1000
LAUNCH = LaunchSettings(
    enabled=True, launch_speed_rad_s=units.rpm_to_rad_s(4000.0), retard_deg=10.0, flat_shift=True
)


def tuned_car(i6: EngineDefinition, launch: LaunchSettings = LAUNCH) -> CarSimulation:
    return CarSimulation(i6, replace(Tune.from_tables(i6.tables), launch=launch))


def run(car: CarSimulation, throttle: float, seconds: float) -> list[CarState]:
    return [car.step(throttle) for _ in range(round(seconds * STEPS_PER_SECOND))]


def rpm(state: CarState) -> float:
    return units.rad_s_to_rpm(state.engine.speed_rad_s)


def kmh(state: CarState) -> float:
    return units.ms_to_kmh(state.drivetrain.vehicle_speed_m_s)


def launched_car(
    i6: EngineDefinition, launch: LaunchSettings = LAUNCH
) -> tuple[CarSimulation, list[CarState]]:
    """1st gear, clutch pressed, 2-step held at full throttle for 2 s, then clutch released."""
    car = tuned_car(i6, launch)
    car.drivetrain.set_clutch_pedal(True)
    assert car.drivetrain.shift(1)
    run(car, 0.0, 0.5)
    run(car, 1.0, 2.0)
    car.drivetrain.set_clutch_pedal(False)
    return car, run(car, 1.0, 3.0)


def test_overall_ratio_and_road_load_are_plausible(i6: EngineDefinition) -> None:
    parameters = i6.drivetrain
    assert parameters.overall_ratio(1) == pytest.approx(4.11 * 3.15)
    assert road_load_n(parameters, 0.0) == 0.0
    # research.md §8.2: about 300 N air + 180 N rolling at 100 km/h.
    assert 400.0 < road_load_n(parameters, units.kmh_to_ms(100.0)) < 600.0


def test_in_neutral_the_engine_revs_freely_and_the_car_stands(i6: EngineDefinition) -> None:
    car = CarSimulation(i6)
    states = run(car, 1.0, 3.0)
    assert max(rpm(state) for state in states) > 6500.0
    assert all(state.drivetrain.vehicle_speed_m_s == 0.0 for state in states)
    assert all(state.drivetrain.clutch_state is ClutchState.OPEN for state in states)


def test_clutch_pressed_while_standing_arms_the_two_step(i6: EngineDefinition) -> None:
    car = tuned_car(i6)
    car.drivetrain.set_clutch_pedal(True)
    car.drivetrain.shift(1)
    held = run(car, 1.0, 2.0)[STEPS_PER_SECOND:]
    assert all(state.engine.command.launch_active for state in held)
    assert min(rpm(state) for state in held) > 3700.0
    assert max(rpm(state) for state in held) < 4150.0


def test_launch_slips_the_clutch_briefly_then_locks_without_chatter(i6: EngineDefinition) -> None:
    _, after_release = launched_car(i6)
    assert not after_release[-1].engine.command.launch_active  # clutch up: 2-step off
    states = [state.drivetrain.clutch_state for state in after_release]
    first_lock = states.index(ClutchState.LOCKED)
    assert first_lock < STEPS_PER_SECOND  # locked within a second
    changes = sum(1 for a, b in pairwise(states) if a is not b)
    assert changes <= 3  # open -> slipping -> locked, no stick-slip chatter
    assert kmh(after_release[-1]) > 40.0


def test_locked_clutch_couples_engine_and_car_kinematically(i6: EngineDefinition) -> None:
    _, after_release = launched_car(i6)
    state = after_release[-1]
    assert state.drivetrain.clutch_state is ClutchState.LOCKED
    expected = (
        state.engine.speed_rad_s
        * i6.drivetrain.wheel_radius_m
        / i6.drivetrain.overall_ratio(state.drivetrain.gear)
    )
    assert state.drivetrain.vehicle_speed_m_s == pytest.approx(expected, rel=1e-9)


def test_zero_to_hundred_is_plausible_for_the_saloon(i6: EngineDefinition) -> None:
    car, states = launched_car(i6)
    states += run(car, 1.0, 1.0)
    while kmh(states[-1]) < 100.0 and len(states) < 20 * STEPS_PER_SECOND:
        last = states[-1]
        if rpm(last) > 6500.0 and not car.drivetrain.shifting:
            car.drivetrain.shift(last.drivetrain.gear + 1)
        states.append(car.step(1.0))
    seconds = len(states) / STEPS_PER_SECOND
    assert 5.0 < seconds < 8.0  # 196 kW, 1550 kg: about 6 s


def test_clutch_pressed_while_moving_does_not_arm_the_two_step(i6: EngineDefinition) -> None:
    car, _ = launched_car(i6)
    car.drivetrain.set_clutch_pedal(True)
    state = car.step(1.0)
    assert not state.engine.command.launch_active


SHIFT_RPM = 5000.0
"""Well below the rev limiter, so a flare without flat shift is not capped by the limiter."""


def shift_at_full_throttle(i6: EngineDefinition, flat_shift: bool) -> list[CarState]:
    """Launch, then upshift 1 -> 2 at 5000 1/min without lifting; returns the interruption."""
    car = tuned_car(i6, replace(LAUNCH, flat_shift=flat_shift))
    car.drivetrain.set_clutch_pedal(True)
    car.drivetrain.shift(1)
    run(car, 1.0, 2.0)
    car.drivetrain.set_clutch_pedal(False)
    state = car.step(1.0)
    while rpm(state) < SHIFT_RPM or state.drivetrain.clutch_state is not ClutchState.LOCKED:
        state = car.step(1.0)
    assert car.drivetrain.shift(2)
    return run(car, 1.0, i6.drivetrain.shift_time_s)


def test_flat_shift_cuts_the_spark_and_keeps_the_engine_from_flaring(
    i6: EngineDefinition,
) -> None:
    flat = shift_at_full_throttle(i6, flat_shift=True)
    plain = shift_at_full_throttle(i6, flat_shift=False)
    assert all(state.engine.command.flat_shift_active for state in flat[:-1])
    assert all(state.engine.command.spark_cut for state in flat[:-1])
    assert not any(state.engine.command.flat_shift_active for state in plain)
    flat_rise = max(rpm(state) for state in flat) - rpm(flat[0])
    plain_rise = max(rpm(state) for state in plain) - rpm(plain[0])
    assert flat_rise < 50.0
    assert plain_rise > 300.0  # without flat shift a no-lift shift flares the engine


def test_shift_changes_the_gear_after_the_interruption(i6: EngineDefinition) -> None:
    car = CarSimulation(i6)
    assert not car.drivetrain.shift(7)  # no such gear
    assert car.drivetrain.shift(1)
    assert not car.drivetrain.shift(2)  # still shifting
    states = run(car, 0.0, i6.drivetrain.shift_time_s + 0.01)
    assert states[0].drivetrain.gear == 0
    assert states[-1].drivetrain.gear == 1


def test_a_stalled_engine_restarts_once_the_clutch_is_pressed(i6: EngineDefinition) -> None:
    car = CarSimulation(i6)
    car.drivetrain.shift(1)
    run(car, 0.0, 3.0)  # clutch released at idle in 1st
    car.engine.set_speed_rad_s(0.0)  # stalled
    car.drivetrain.set_clutch_pedal(True)
    state = car.step(0.0)
    assert rpm(state) < 1.0  # this step still reports the stalled crank
    assert car.engine.speed_rad_s == i6.idle_speed_rad_s  # the "starter" brought it back


def test_roller_shows_less_power_at_the_wheels_than_the_engine_dyno(
    i6: EngineDefinition,
) -> None:
    roller = run_roller(i6, 4, 3000.0, 6800.0, sample_step_rpm=200.0)
    engine = run_sweep(i6, 3000.0, 6800.0, sample_step_rpm=200.0)
    ratio = max(roller.wheel_power_w) / max(engine.power_w)
    assert 0.85 < ratio < i6.drivetrain.efficiency  # losses plus engine inertia
    gear_ratio = i6.drivetrain.overall_ratio(4)
    speed_check = roller.vehicle_speed_m_s * gear_ratio / i6.drivetrain.wheel_radius_m
    assert speed_check == pytest.approx(roller.speed_rad_s, rel=1e-9)
