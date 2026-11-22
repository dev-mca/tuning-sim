"""Overrun settings, late combustion and catalyst temperature (research.md §2.10a, §3.3)."""

from dataclasses import replace

import pytest

from tuning_lab import units
from tuning_lab.ecu import OverrunSettings, Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.simulation import EngineSimulation, SimulationState

POPS = OverrunSettings(fuel_cut=False, retard_deg=35.0, enrichment=0.1)


def coasting(i6: EngineDefinition, settings: OverrunSettings, steps: int) -> list[SimulationState]:
    """Lift-off from 5000 1/min without a brake."""
    tune = replace(Tune.from_tables(i6.tables), overrun=settings)
    simulation = EngineSimulation(i6, tune, initial_speed_rad_s=units.rpm_to_rad_s(5000.0))
    return [simulation.step(0.0, 0.0) for _ in range(steps)]


def test_stock_tune_cuts_fuel_in_overrun(i6: EngineDefinition) -> None:
    state = coasting(i6, OverrunSettings(), 10)[-1]
    assert state.command.fuel_cut
    assert not state.command.overrun_fueling
    assert state.command.fuel_mass_per_cycle_kg == 0.0


def test_without_fuel_cut_the_ecu_keeps_fuelling_with_the_map_ignition(
    i6: EngineDefinition,
) -> None:
    plain = coasting(i6, OverrunSettings(fuel_cut=False), 10)[-1]
    assert not plain.command.fuel_cut
    assert plain.command.overrun_fueling
    assert plain.command.fuel_mass_per_cycle_kg > 0.0
    assert plain.engine.late_combustion_fraction == 0.0  # normal ignition: burns in the cylinder


def test_pops_tune_retards_enriches_and_burns_late(i6: EngineDefinition) -> None:
    # First step: both start from the same state, so the map values are identical.
    plain = coasting(i6, OverrunSettings(fuel_cut=False), 1)[-1]
    pops = coasting(i6, POPS, 1)[-1]
    assert pops.command.ignition_deg == pytest.approx(plain.command.ignition_deg - 35.0)
    assert pops.command.fuel_mass_per_cycle_kg == pytest.approx(
        plain.command.fuel_mass_per_cycle_kg * 1.1, rel=1e-3
    )
    assert pops.engine.late_combustion_fraction > 0.3
    assert pops.engine.afterburn_temperature_rise_k > 100.0


def test_late_combustion_starts_above_the_idle_torque_reserve(i6: EngineDefinition) -> None:
    # Idle ignition sits about 20 deg behind MBT (torque reserve) and must not count as late.
    simulation = EngineSimulation(i6)
    state = simulation.step(0.0, 0.0)
    for _ in range(3000):
        state = simulation.step(0.0, 0.0)
    assert state.engine.late_combustion_fraction == 0.0


def test_catalyst_cools_in_fuel_cut_and_heats_with_pops(i6: EngineDefinition) -> None:
    start_k = i6.afterburn.catalyst_initial_k
    stock = coasting(i6, OverrunSettings(), 1500)
    pops = coasting(i6, POPS, 1500)
    assert stock[-1].catalyst_temperature_k < start_k
    assert pops[-1].catalyst_temperature_k > start_k + 50.0


def test_catalyst_follows_a_first_order_lag(i6: EngineDefinition) -> None:
    states = coasting(i6, OverrunSettings(), 2)
    first, second = states
    tau = i6.afterburn.catalyst_time_constant_s
    target = second.engine.exhaust_temperature_k + second.engine.afterburn_temperature_rise_k
    expected = first.catalyst_temperature_k + 1.0e-3 * (target - first.catalyst_temperature_k) / tau
    assert second.catalyst_temperature_k == pytest.approx(expected, rel=1e-12)
