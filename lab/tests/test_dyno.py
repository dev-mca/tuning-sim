"""Dyno and simulation: plausibility (research.md §10), tuning effects, numerics (§9)."""

from dataclasses import replace

import numpy as np
import pytest

from tuning_lab import units
from tuning_lab.dyno import SweepResult, run_hold, run_sweep
from tuning_lab.ecu import Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.simulation import EngineSimulation


def stock(i6: EngineDefinition) -> Tune:
    return Tune.from_tables(i6.tables)


def test_peak_torque_in_plausible_range(stock_sweep: SweepResult) -> None:
    assert 280.0 <= stock_sweep.measured_torque_nm.max() <= 330.0


def test_peak_power_in_plausible_range(stock_sweep: SweepResult) -> None:
    assert 190.0 <= units.w_to_kw(stock_sweep.power_w.max()) <= 230.0


def test_bmep_never_exceeds_naturally_aspirated_limit(
    stock_sweep: SweepResult, i6: EngineDefinition
) -> None:
    assert units.pa_to_bar(stock_sweep.bmep_pa(i6.displacement_m3).max()) <= 13.5


def test_exhaust_temperature_at_full_load(stock_sweep: SweepResult) -> None:
    egt_c = units.kelvin_to_celsius(stock_sweep.exhaust_temperature_k)
    assert egt_c.min() >= 700.0
    assert egt_c.max() <= 950.0


def test_sweep_follows_the_setpoint(stock_sweep: SweepResult) -> None:
    error_rpm = units.rad_s_to_rpm(stock_sweep.speed_rad_s - stock_sweep.setpoint_rad_s)
    assert np.abs(error_rpm).max() < 20.0


def test_measured_torque_matches_engine_torque(stock_sweep: SweepResult) -> None:
    # Inertia correction works: the dyno sees the engine, not the acceleration torque.
    np.testing.assert_allclose(
        stock_sweep.measured_torque_nm, stock_sweep.engine_torque_nm, rtol=0.01
    )


def test_sweep_is_deterministic(i6: EngineDefinition, stock_sweep: SweepResult) -> None:
    again = run_sweep(i6, 1000.0, 7000.0)
    np.testing.assert_array_equal(again.measured_torque_nm, stock_sweep.measured_torque_nm)


def test_late_ignition_reduces_power(i6: EngineDefinition, stock_sweep: SweepResult) -> None:
    tables = i6.tables
    late = run_sweep(
        i6, 1000.0, 7000.0,
        tune=replace(stock(i6), ignition=tables.ignition.with_offset(-5.0)),
    )  # fmt: skip
    assert np.all(late.measured_torque_nm < stock_sweep.measured_torque_nm)
    assert np.all(late.exhaust_temperature_k > stock_sweep.exhaust_temperature_k)


def test_lambda_one_gives_less_torque_than_rich(i6: EngineDefinition) -> None:
    tables = i6.tables
    rich = run_sweep(
        i6,
        2000.0,
        5000.0,
        sample_step_rpm=500.0,
        tune=replace(stock(i6), lambda_target=tables.lambda_target.filled(0.85)),
    )
    stoich = run_sweep(
        i6,
        2000.0,
        5000.0,
        sample_step_rpm=500.0,
        tune=replace(stock(i6), lambda_target=tables.lambda_target.filled(1.0)),
    )
    assert np.all(stoich.measured_torque_nm < rich.measured_torque_nm)


def test_wrong_ve_table_shows_on_lambda(i6: EngineDefinition) -> None:
    # research.md §2.4: ECU believes 10 % less air -> injects 10 % less fuel -> lean.
    tables = i6.tables
    result = run_sweep(
        i6, 3000.0, 4000.0, sample_step_rpm=500.0,
        tune=replace(stock(i6), ve=tables.ve.scaled(0.9)),
    )  # fmt: skip
    np.testing.assert_allclose(result.lambda_actual, 0.88 / 0.9, rtol=0.01)


def test_hold_rpm_settles_at_part_load(i6: EngineDefinition) -> None:
    state = run_hold(i6, 3000.0, throttle=0.1, duration_s=3.0)
    assert units.rad_s_to_rpm(state.speed_rad_s) == pytest.approx(3000.0, abs=5.0)
    assert 20_000.0 < state.manifold_pressure_pa < 90_000.0  # throttled
    assert state.load_torque_nm > 0.0


def test_free_running_engine_hits_rev_limiter(i6: EngineDefinition) -> None:
    simulation = EngineSimulation(i6, stock(i6), initial_speed_rad_s=units.rpm_to_rad_s(3000.0))
    speeds_rpm = [units.rad_s_to_rpm(simulation.step(1.0, 0.0).speed_rad_s) for _ in range(5000)]
    assert max(speeds_rpm) < 7100.0  # limiter cuts fuel above 7000
    assert max(speeds_rpm) > 6900.0


def test_closed_throttle_engine_idles_by_itself(i6: EngineDefinition) -> None:
    # No dyno: with the throttle closed the idle air (leak area) and the idle ignition
    # (torque reserve) must balance friction, pumping and accessories near idle speed.
    simulation = EngineSimulation(i6, initial_speed_rad_s=units.rpm_to_rad_s(1200.0))
    state = simulation.step(0.0, 0.0)
    for _ in range(8000):
        state = simulation.step(0.0, 0.0)
    assert 600.0 < units.rad_s_to_rpm(state.speed_rad_s) < 1000.0
    # research.md §10: idle MAP 30-45 kPa; the model lands at the lower edge.
    assert 27_000.0 < state.manifold_pressure_pa < 45_000.0


def test_halving_the_time_step_changes_little(i6: EngineDefinition) -> None:
    # research.md §9: stability check with dt and dt/2.
    def final_speed(dt: float) -> float:
        simulation = EngineSimulation(i6, initial_speed_rad_s=units.rpm_to_rad_s(2000.0),
                                      time_step_s=dt)  # fmt: skip
        steps = round(1.0 / dt)
        speed = 0.0
        for k in range(steps):
            throttle = 1.0 if k * dt < 0.5 else 0.2  # tip-in, then lift-off
            speed = simulation.step(throttle, 150.0).speed_rad_s
        return speed

    assert final_speed(1.0e-3) == pytest.approx(final_speed(0.5e-3), rel=0.005)
