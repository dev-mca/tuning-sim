"""0D engine results: plausibility (research.md §10), trends, exhaust pulses, MVM comparison."""

from dataclasses import replace

import numpy as np
import pytest

from conftest import stock_point
from tuning_lab import units
from tuning_lab.mvm import torque_to_bmep_pa
from tuning_lab.zero_d.engine import ZeroDEngineResult, run_operating_point
from tuning_lab.zero_d.parameters import ZeroDEngine
from tuning_lab.zero_d.validation import (
    ComparisonPoint,
    full_load_comparison,
    full_load_manifold_pressure_pa,
    part_load_comparison,
)

PART_LOAD_RPM = 3000.0
PART_LOAD_PA = 60_000.0


@pytest.fixture(scope="module")
def full_load_points(zero_d_engine: ZeroDEngine) -> list[ComparisonPoint]:
    return full_load_comparison(zero_d_engine)


@pytest.fixture(scope="module")
def part_load_points(zero_d_engine: ZeroDEngine) -> list[ComparisonPoint]:
    return part_load_comparison(zero_d_engine)


# --- plausibility at full load (research.md §10) ---


def test_full_load_bmep_is_in_the_na_range(
    full_load: ZeroDEngineResult, zero_d_engine: ZeroDEngine
) -> None:
    displacement = zero_d_engine.definition.displacement_m3
    bmep = torque_to_bmep_pa(full_load.brake_torque_nm, displacement)
    # research.md §10: 11-13.5 bar; the 0D model lacks intake tuning, so allow down to 10.
    assert 10.0e5 <= bmep <= 13.5e5
    assert 10.5e5 <= full_load.imep_gross_pa <= 14.5e5


def test_full_load_air_and_residuals_are_plausible(full_load: ZeroDEngineResult) -> None:
    # research.md §10: VE 0.85-1.05; without pipe dynamics (no ram effect) the 0D model is
    # at the low end, so the lower bound is relaxed to 0.78.
    assert 0.78 <= full_load.volumetric_efficiency <= 1.05
    assert 0.02 <= full_load.cylinder.residual_gas_fraction <= 0.08


def test_full_load_pressure_trace_is_plausible(full_load: ZeroDEngineResult) -> None:
    # Naturally aspirated gasoline engine near MBT: 40-80 bar, peak 10-20 deg ATDC.
    assert 40.0e5 <= full_load.peak_pressure_pa <= 80.0e5
    assert 10.0 <= full_load.peak_pressure_angle_deg <= 20.0
    assert 5.0 <= full_load.ca50_deg <= 15.0


def test_full_load_efficiency_and_wall_heat_are_plausible(full_load: ZeroDEngineResult) -> None:
    energy = full_load.cylinder.energy
    gross_efficiency = energy.gross_work_j / energy.heat_release_j
    # Below the Otto limit 1 - eps^(1-gamma) ~ 0.5 (research.md §2.6), above the MVM's 0.36.
    assert 0.36 <= gross_efficiency <= 0.48
    assert 0.05 <= energy.wall_heat_j / energy.heat_release_j <= 0.30


def test_exhaust_temperature_is_hot_but_bounded(full_load: ZeroDEngineResult) -> None:
    # Enthalpy-averaged at the valve, no port heat loss: above the 700-950 degC thermocouple
    # range of research.md §10 (see ADR 0010), but it must not be absurd.
    egt_c = units.kelvin_to_celsius(full_load.exhaust_temperature_k)
    assert 750.0 <= egt_c <= 1150.0


# --- trends ---


def test_late_ignition_costs_torque_and_heats_the_exhaust(
    zero_d_engine: ZeroDEngine, full_load: ZeroDEngineResult
) -> None:
    # research.md §2.7: retard moves combustion into the expansion stroke.
    point = full_load.cylinder.point
    late_point = replace(point, ignition_deg=point.ignition_deg - 10.0)
    late = run_operating_point(zero_d_engine, late_point)
    assert late.indicated_torque_net_nm < full_load.indicated_torque_net_nm
    assert late.exhaust_temperature_k > full_load.exhaust_temperature_k + 20.0


def test_best_torque_is_near_the_mbt_table(zero_d_engine: ZeroDEngine) -> None:
    # The burn duration is chosen so that a spark at table MBT gives CA50 = 8 deg ATDC; the
    # 0D model's own optimum must then lie close to that MBT.
    definition = zero_d_engine.definition
    speed = units.rpm_to_rad_s(PART_LOAD_RPM)
    mbt = definition.tables.ignition_mbt.lookup(speed, PART_LOAD_PA)
    base = stock_point(definition, PART_LOAD_RPM, PART_LOAD_PA)
    advances = mbt + np.arange(-12.0, 12.1, 3.0)
    results = [
        run_operating_point(zero_d_engine, replace(base, ignition_deg=float(a))) for a in advances
    ]
    torques = [result.indicated_torque_net_nm for result in results]
    best = float(advances[int(np.argmax(torques))])
    assert abs(best - mbt) <= 6.0


def test_lower_manifold_pressure_means_less_torque_and_more_pumping(
    zero_d_engine: ZeroDEngine, full_load: ZeroDEngineResult
) -> None:
    definition = zero_d_engine.definition
    rpm = units.rad_s_to_rpm(full_load.cylinder.point.speed_rad_s)
    part = run_operating_point(zero_d_engine, stock_point(definition, rpm, 50_000.0))
    assert part.brake_torque_nm < 0.6 * full_load.brake_torque_nm
    assert part.pmep_pa > full_load.pmep_pa
    assert part.cylinder.residual_gas_fraction > full_load.cylinder.residual_gas_fraction


# --- exhaust pulses (future sound source, research.md §7.5) ---


def test_exhaust_pulses_follow_the_firing_order(
    full_load: ZeroDEngineResult, zero_d_engine: ZeroDEngine
) -> None:
    flows = full_load.exhaust_mass_flow_by_cylinder_kg_s
    angles = zero_d_engine.definition.firing_angles_deg
    assert flows.shape == (6, 720)
    step = int(full_load.exhaust_angle_deg[1] - full_load.exhaust_angle_deg[0])
    for cylinder, phase in enumerate(angles):
        np.testing.assert_allclose(flows[cylinder], np.roll(flows[0], int(phase) // step))


def test_total_exhaust_flow_is_dominated_by_the_third_order(full_load: ZeroDEngineResult) -> None:
    # research.md §1.2: an evenly firing I6 has 3 pulses per revolution = 6 per 720 deg.
    spectrum = np.abs(np.fft.rfft(full_load.total_exhaust_mass_flow_kg_s))
    assert int(np.argmax(spectrum[1:])) + 1 == 6


def test_mean_exhaust_flow_matches_the_air_and_fuel_flow(full_load: ZeroDEngineResult) -> None:
    cylinder = full_load.cylinder
    fuel_share = 1.0 + cylinder.fuel_kg / cylinder.trapped_air_kg
    mean_flow = float(np.mean(full_load.total_exhaust_mass_flow_kg_s))
    assert mean_flow == pytest.approx(full_load.air_mass_flow_kg_s * fuel_share, rel=0.01)


# --- comparison with the mean-value model ---


def test_full_load_manifold_pressure_is_just_below_ambient(zero_d_engine: ZeroDEngine) -> None:
    for rpm in (1000.0, 7000.0):
        manifold = full_load_manifold_pressure_pa(zero_d_engine.definition, units.rpm_to_rad_s(rpm))
        assert 95_000.0 <= manifold <= 101_325.0


def test_full_load_torque_and_air_are_within_25_percent_of_the_mvm(
    full_load_points: list[ComparisonPoint],
) -> None:
    for point in full_load_points:
        assert abs(point.brake_torque_deviation) <= 0.25, point.speed_rpm
        assert abs(point.indicated_torque_deviation) <= 0.25, point.speed_rpm
        assert abs(point.air_mass_deviation) <= 0.25, point.speed_rpm


def test_full_load_peak_torque_is_plausible(full_load_points: list[ComparisonPoint]) -> None:
    # research.md §10: 280-330 Nm; the 0D model without intake tuning may be a bit lower.
    peak = max(point.zero_d.brake_torque_nm for point in full_load_points)
    assert 250.0 <= peak <= 330.0


def test_part_load_pumping_matches_the_mvm(
    part_load_points: list[ComparisonPoint], zero_d_engine: ZeroDEngine
) -> None:
    # Both models must see the pumping loss p_exh - p_m (research.md §2.8) within 15 kPa.
    displacement = zero_d_engine.definition.displacement_m3
    for point in part_load_points:
        mvm_pmep = torque_to_bmep_pa(point.mvm.pumping_torque_nm, displacement)
        assert point.zero_d.pmep_pa == pytest.approx(mvm_pmep, abs=15_000.0), point.speed_rpm


def test_part_load_torque_is_positive_and_below_full_load(
    part_load_points: list[ComparisonPoint], full_load_points: list[ComparisonPoint]
) -> None:
    full_peak = max(point.zero_d.brake_torque_nm for point in full_load_points)
    for point in part_load_points:
        assert 0.0 < point.zero_d.brake_torque_nm < full_peak
