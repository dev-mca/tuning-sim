"""Engine model at fixed operating points: units, magnitudes and trends (research.md §2)."""

import math

import pytest

from tuning_lab import units
from tuning_lab.constants import AFR_STOICH_GASOLINE, AMBIENT_TEMPERATURE_K
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.mvm import (
    EngineModel,
    OperatingPoint,
    air_density_kg_m3,
    ignition_efficiency,
    lambda_efficiency,
    mean_effective_pressure_to_torque_nm,
    torque_to_bmep_pa,
)

SPEED_4000 = units.rpm_to_rad_s(4000.0)
FULL_LOAD_PA = 100_000.0


def full_load_point(model: EngineModel, lambda_value: float, ignition_deg: float) -> OperatingPoint:
    air = model.air_mass_per_cycle_kg(SPEED_4000, FULL_LOAD_PA, AMBIENT_TEMPERATURE_K)
    return OperatingPoint(
        speed_rad_s=SPEED_4000,
        manifold_pressure_pa=FULL_LOAD_PA,
        manifold_temperature_k=AMBIENT_TEMPERATURE_K,
        fuel_mass_per_cycle_kg=air / (lambda_value * AFR_STOICH_GASOLINE),
        ignition_deg=ignition_deg,
    )


def mbt_at_4000(i6: EngineDefinition) -> float:
    return i6.tables.ignition_mbt.lookup(SPEED_4000, FULL_LOAD_PA)


def test_air_density_at_standard_conditions() -> None:
    assert air_density_kg_m3(101_325.0, 288.15) == pytest.approx(1.225, abs=1.0e-3)


def test_mep_torque_conversion_example_from_research() -> None:
    # research.md §2.9: 3.0 l NA with 13 bar -> about 310 Nm.
    assert mean_effective_pressure_to_torque_nm(13.0e5, 0.003) == pytest.approx(310.4, abs=0.5)
    assert torque_to_bmep_pa(310.4, 0.003) == pytest.approx(13.0e5, rel=1.0e-3)


def test_ignition_efficiency_is_one_at_mbt_and_quadratic_around_it(i6: EngineDefinition) -> None:
    combustion = i6.combustion
    assert ignition_efficiency(30.0, 30.0, combustion) == 1.0
    loss_10 = 1.0 - ignition_efficiency(20.0, 30.0, combustion)
    assert 0.04 <= loss_10 <= 0.07  # research.md §2.7: 10 deg late costs about 4-7 %
    assert ignition_efficiency(25.0, 30.0, combustion) == ignition_efficiency(
        35.0, 30.0, combustion
    )
    assert ignition_efficiency(-60.0, 30.0, combustion) == 0.0  # never negative


def test_lambda_efficiency_peaks_slightly_rich(i6: EngineDefinition) -> None:
    combustion = i6.combustion
    assert lambda_efficiency(combustion.lambda_optimum, combustion) == 1.0
    assert lambda_efficiency(1.0, combustion) < 1.0
    assert lambda_efficiency(1.2, combustion) < lambda_efficiency(1.0, combustion)
    assert lambda_efficiency(math.inf, combustion) == 0.0


def test_full_load_air_mass_per_cycle(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    air = model.air_mass_per_cycle_kg(SPEED_4000, FULL_LOAD_PA, AMBIENT_TEMPERATURE_K)
    # VE about 1.0 * 1.17 kg/m^3 * 3 l = about 3.5 g per cycle.
    assert 3.0e-3 < air < 4.0e-3


def test_full_load_torque_and_exhaust_temperature_are_plausible(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    out = model.evaluate(full_load_point(model, 0.88, mbt_at_4000(i6)))
    bmep_bar = units.pa_to_bar(torque_to_bmep_pa(out.brake_torque_nm, i6.displacement_m3))
    assert 11.0 <= bmep_bar <= 13.5  # research.md §10, NA engine
    assert 700.0 <= units.kelvin_to_celsius(out.exhaust_temperature_k) <= 950.0
    assert out.lambda_actual == pytest.approx(0.88)
    assert out.friction_torque_nm > 0.0
    assert out.indicated_torque_nm > out.brake_torque_nm


def test_late_ignition_costs_torque_and_heats_the_exhaust(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    at_mbt = model.evaluate(full_load_point(model, 0.88, mbt_at_4000(i6)))
    late = model.evaluate(full_load_point(model, 0.88, mbt_at_4000(i6) - 10.0))
    assert late.brake_torque_nm < at_mbt.brake_torque_nm
    assert late.exhaust_temperature_k > at_mbt.exhaust_temperature_k


def test_rich_mixture_gives_more_torque_than_stoichiometric(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    rich = model.evaluate(full_load_point(model, 0.88, mbt_at_4000(i6)))
    stoich = model.evaluate(full_load_point(model, 1.0, mbt_at_4000(i6)))
    gain = rich.brake_torque_nm / stoich.brake_torque_nm - 1.0
    assert 0.01 < gain < 0.08


def test_fuel_cut_gives_negative_torque_and_infinite_lambda(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    point = OperatingPoint(SPEED_4000, 30_000.0, AMBIENT_TEMPERATURE_K, 0.0, 30.0)
    out = model.evaluate(point)
    assert math.isinf(out.lambda_actual)
    assert out.indicated_torque_nm == 0.0
    assert out.brake_torque_nm < 0.0  # engine braking: friction and pumping


def test_part_load_has_pumping_losses(i6: EngineDefinition) -> None:
    model = EngineModel(i6)
    point = OperatingPoint(SPEED_4000, 40_000.0, AMBIENT_TEMPERATURE_K, 1.0e-4, 35.0)
    out = model.evaluate(point)
    assert out.pumping_torque_nm > 10.0  # about 60 kPa pressure difference
