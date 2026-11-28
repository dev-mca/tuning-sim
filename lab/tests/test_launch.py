"""Launch control / 2-step while standing (research.md §3.4, iteration 7c)."""

from dataclasses import replace

import pytest

from tuning_lab import units
from tuning_lab.ecu import LaunchSettings, Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.mvm import EngineModel, OperatingPoint
from tuning_lab.simulation import EngineSimulation, SimulationState

TWO_STEP = LaunchSettings(
    enabled=True, launch_speed_rad_s=units.rpm_to_rad_s(4000.0), retard_deg=10.0
)
STEPS_PER_SECOND = 1000


def full_throttle(
    i6: EngineDefinition, settings: LaunchSettings, armed: bool, steps: int
) -> list[SimulationState]:
    """Free-running engine (no brake), launch switch as given, full throttle from idle."""
    tune = replace(Tune.from_tables(i6.tables), launch=settings)
    simulation = EngineSimulation(i6, tune)
    simulation.set_launch_switch(armed)
    return [simulation.step(1.0, 0.0) for _ in range(steps)]


def peak_rpm(states: list[SimulationState]) -> float:
    return max(units.rad_s_to_rpm(state.speed_rad_s) for state in states)


def test_stock_tune_has_launch_control_switched_off() -> None:
    assert not LaunchSettings().enabled


def test_without_launch_the_engine_revs_to_the_normal_limiter(i6: EngineDefinition) -> None:
    disabled = full_throttle(i6, LaunchSettings(), armed=True, steps=3 * STEPS_PER_SECOND)
    not_armed = full_throttle(i6, TWO_STEP, armed=False, steps=3 * STEPS_PER_SECOND)
    for states in (disabled, not_armed):
        assert peak_rpm(states) > 6500.0
        assert not any(state.command.launch_active for state in states)
        assert not any(state.command.spark_cut for state in states)


def test_two_step_holds_the_engine_at_the_launch_speed(i6: EngineDefinition) -> None:
    states = full_throttle(i6, TWO_STEP, armed=True, steps=3 * STEPS_PER_SECOND)
    settled = states[STEPS_PER_SECOND:]
    speeds = [units.rad_s_to_rpm(state.speed_rad_s) for state in settled]
    assert min(speeds) > 3700.0
    assert max(speeds) < 4150.0
    assert all(state.command.launch_active for state in settled)


def test_two_step_cuts_the_spark_but_keeps_injecting(i6: EngineDefinition) -> None:
    settled = full_throttle(i6, TWO_STEP, armed=True, steps=3 * STEPS_PER_SECOND)[1000:]
    cut = [state for state in settled if state.command.spark_cut]
    firing = [state for state in settled if not state.command.spark_cut]
    assert cut  # bounces between cut and firing like a limiter
    assert firing
    for state in cut:
        assert not state.command.fuel_cut
        assert state.command.fuel_mass_per_cycle_kg > 0.0
        assert state.engine.indicated_torque_nm == 0.0
        assert state.engine.late_combustion_fraction == 1.0
        assert state.engine.afterburn_temperature_rise_k > 100.0


def test_two_step_retards_the_ignition_while_active(i6: EngineDefinition) -> None:
    plain = full_throttle(i6, LaunchSettings(), armed=True, steps=1)[-1]
    launch = full_throttle(i6, TWO_STEP, armed=True, steps=1)[-1]
    assert launch.command.launch_active
    assert launch.command.ignition_deg == pytest.approx(plain.command.ignition_deg - 10.0)


def test_two_step_needs_an_open_throttle(i6: EngineDefinition) -> None:
    tune = replace(Tune.from_tables(i6.tables), launch=TWO_STEP)
    simulation = EngineSimulation(i6, tune)
    simulation.set_launch_switch(True)
    state = simulation.step(0.0, 0.0)
    assert not state.command.launch_active


def test_releasing_the_switch_lets_the_engine_rev_past_the_launch_speed(
    i6: EngineDefinition,
) -> None:
    tune = replace(Tune.from_tables(i6.tables), launch=TWO_STEP)
    simulation = EngineSimulation(i6, tune)
    simulation.set_launch_switch(True)
    for _ in range(2 * STEPS_PER_SECOND):
        simulation.step(1.0, 0.0)
    simulation.set_launch_switch(False)
    released = [simulation.step(1.0, 0.0) for _ in range(2 * STEPS_PER_SECOND)]
    assert peak_rpm(released) > 6000.0


def test_two_step_heats_the_catalyst(i6: EngineDefinition) -> None:
    plain = full_throttle(i6, LaunchSettings(), armed=True, steps=3 * STEPS_PER_SECOND)
    launch = full_throttle(i6, TWO_STEP, armed=True, steps=3 * STEPS_PER_SECOND)
    assert launch[-1].catalyst_temperature_k > plain[-1].catalyst_temperature_k + 50.0


def test_spark_cut_burns_nothing_in_the_cylinder(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    point = OperatingPoint(
        speed_rad_s=units.rpm_to_rad_s(4000.0),
        manifold_pressure_pa=100.0e3,
        manifold_temperature_k=298.15,
        fuel_mass_per_cycle_kg=2.0e-4,
        ignition_deg=20.0,
    )
    firing = model.evaluate(point)
    cut = model.evaluate(replace(point, spark_cut=True))
    assert firing.indicated_torque_nm > 100.0
    assert cut.indicated_torque_nm == 0.0
    assert cut.brake_torque_nm < 0.0  # only friction, pumping and accessories remain
    assert cut.exhaust_temperature_k == pytest.approx(298.15)  # no heat from combustion
    assert cut.afterburn_temperature_rise_k > firing.afterburn_temperature_rise_k
    assert cut.fuel_mass_flow_kg_s == firing.fuel_mass_flow_kg_s  # fuel is still injected
