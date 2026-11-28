"""0D building blocks: kinematics, gas properties, valves, Wiebe, Woschni (research.md §1.4, §7)."""

import itertools
import math

import numpy as np
import pytest

from tuning_lab import units
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.intake import critical_pressure_ratio
from tuning_lab.zero_d import gas
from tuning_lab.zero_d.combustion import (
    burn_duration_deg,
    burn_rate_per_deg,
    burned_fraction,
    progress_at_fraction,
)
from tuning_lab.zero_d.heat_transfer import woschni_coefficient_w_m2_k
from tuning_lab.zero_d.kinematics import CylinderKinematics
from tuning_lab.zero_d.parameters import (
    DEFAULT_INTAKE_VALVES,
    CombustionAssumptions,
    ZeroDEngine,
)
from tuning_lab.zero_d.valves import orifice_mass_flow_kg_s, valve_flow_area_m2, valve_lift_m


@pytest.fixture(scope="module")
def kinematics(zero_d_engine: ZeroDEngine) -> CylinderKinematics:
    return CylinderKinematics(zero_d_engine.geometry)


# --- kinematics (§1.4) ---


def test_volume_at_tdc_is_clearance_and_at_bdc_adds_swept(kinematics: CylinderKinematics) -> None:
    assert kinematics.volume_m3(0.0) == pytest.approx(kinematics.clearance_volume_m3, rel=1e-12)
    assert kinematics.volume_m3(180.0) == pytest.approx(
        kinematics.clearance_volume_m3 + kinematics.swept_volume_m3, rel=1e-12
    )


def test_compression_ratio_is_reproduced(
    kinematics: CylinderKinematics, zero_d_engine: ZeroDEngine
) -> None:
    ratio = kinematics.volume_m3(180.0) / kinematics.volume_m3(0.0)
    assert ratio == pytest.approx(zero_d_engine.geometry.compression_ratio, rel=1e-12)


def test_swept_volume_matches_engine_displacement(
    kinematics: CylinderKinematics, i6: EngineDefinition
) -> None:
    assert i6.cylinders * kinematics.swept_volume_m3 == pytest.approx(i6.displacement_m3, rel=1e-12)


def test_volume_is_symmetric_and_periodic(kinematics: CylinderKinematics) -> None:
    angles = np.linspace(0.0, 180.0, 37)
    np.testing.assert_allclose(kinematics.volume_m3(angles), kinematics.volume_m3(-angles))
    np.testing.assert_allclose(kinematics.volume_m3(angles), kinematics.volume_m3(angles + 360.0))


def test_volume_derivative_matches_finite_difference(kinematics: CylinderKinematics) -> None:
    angles = np.linspace(-170.0, 170.0, 35)
    delta = 1.0e-4
    numeric = (kinematics.volume_m3(angles + delta) - kinematics.volume_m3(angles - delta)) / (
        2.0 * delta
    )
    np.testing.assert_allclose(
        kinematics.volume_derivative_m3_per_deg(angles), numeric, rtol=1e-6, atol=1e-12
    )


def test_volume_derivative_is_zero_at_dead_centres(kinematics: CylinderKinematics) -> None:
    assert kinematics.volume_derivative_m3_per_deg(0.0) == pytest.approx(0.0, abs=1e-15)
    assert kinematics.volume_derivative_m3_per_deg(180.0) == pytest.approx(0.0, abs=1e-15)


def test_piston_reaches_half_stroke_before_90_deg(kinematics: CylinderKinematics) -> None:
    # Finite conrod: the piston is past half stroke at 90 deg (Heywood ch. 2).
    stroke = kinematics.geometry.stroke_m
    assert kinematics.piston_position_m(90.0) > 0.5 * stroke


def test_mean_piston_speed_at_rev_limit_is_plausible(
    kinematics: CylinderKinematics, i6: EngineDefinition
) -> None:
    # research.md §10: 15-22 m/s at rated speed.
    assert 15.0 <= kinematics.mean_piston_speed_m_s(i6.rev_limit_rad_s) <= 22.0


def test_wall_area_grows_with_piston_travel(kinematics: CylinderKinematics) -> None:
    assert kinematics.wall_area_m2(180.0) > kinematics.wall_area_m2(0.0)
    assert kinematics.wall_area_m2(0.0) > 2.0 * kinematics.piston_area_m2


# --- gas properties ---


@pytest.mark.parametrize(
    ("temperature_k", "low", "high"), [(300.0, 1.31, 1.34), (2500.0, 1.24, 1.27)]
)
def test_gamma_is_in_the_single_zone_range(temperature_k: float, low: float, high: float) -> None:
    assert low <= gas.gamma(temperature_k) <= high


@pytest.mark.parametrize("temperature_k", [250.0, 298.15, 900.0, 2600.0])
def test_temperature_inverses_round_trip(temperature_k: float) -> None:
    u = gas.internal_energy_j_kg(temperature_k)
    h = gas.enthalpy_j_kg(temperature_k)
    assert gas.temperature_from_internal_energy_k(u) == pytest.approx(temperature_k, rel=1e-12)
    assert gas.temperature_from_enthalpy_k(h) == pytest.approx(temperature_k, rel=1e-12)


def test_internal_energy_derivative_is_cv() -> None:
    delta = 1.0e-3
    slope = (
        gas.internal_energy_j_kg(1000.0 + delta) - gas.internal_energy_j_kg(1000.0 - delta)
    ) / (2.0 * delta)
    assert slope == pytest.approx(gas.specific_heat_cv_j_kg_k(1000.0), rel=1e-9)


# --- valves (§7.4) ---


def test_valve_lift_is_zero_outside_and_maximal_in_the_middle() -> None:
    max_lift = 0.009
    assert valve_lift_m(100.0, 130.0, 370.0, max_lift) == 0.0
    assert valve_lift_m(130.0, 130.0, 370.0, max_lift) == 0.0
    assert valve_lift_m(400.0, 130.0, 370.0, max_lift) == 0.0
    assert valve_lift_m(250.0, 130.0, 370.0, max_lift) == pytest.approx(max_lift)


def test_valve_lift_handles_the_wrap_at_720_deg() -> None:
    # Window 650 -> 170 (through 720/0): open at 50 and at 710, closed at 300.
    assert valve_lift_m(50.0, 650.0, 170.0, 0.01) > 0.0
    assert valve_lift_m(710.0, 650.0, 170.0, 0.01) > 0.0
    assert valve_lift_m(-10.0, 650.0, 170.0, 0.01) == pytest.approx(
        valve_lift_m(710.0, 650.0, 170.0, 0.01)
    )
    assert valve_lift_m(300.0, 650.0, 170.0, 0.01) == 0.0


def test_valve_area_is_limited_by_the_seat() -> None:
    valve = DEFAULT_INTAKE_VALVES
    seat = valve.count * math.pi / 4.0 * valve.diameter_m**2
    small_lift = 0.001
    assert valve_flow_area_m2(small_lift, valve) == pytest.approx(
        valve.count * math.pi * valve.diameter_m * small_lift
    )
    assert valve_flow_area_m2(valve.max_lift_m, valve) == pytest.approx(seat)


def test_orifice_flow_is_zero_at_equal_pressure_and_constant_when_choked() -> None:
    area = 1.0e-4
    assert orifice_mass_flow_kg_s(area, 1.0e5, 300.0, 1.0e5, 1.4) == 0.0
    critical = critical_pressure_ratio(1.4)
    choked_a = orifice_mass_flow_kg_s(area, 1.0e5, 300.0, 0.2e5, 1.4)
    choked_b = orifice_mass_flow_kg_s(area, 1.0e5, 300.0, 0.9 * critical * 1.0e5, 1.4)
    assert choked_a == pytest.approx(choked_b)
    assert orifice_mass_flow_kg_s(area, 1.0e5, 300.0, 0.9e5, 1.4) < choked_a


# --- Wiebe (§7.2) ---


def test_wiebe_fraction_goes_from_zero_to_one_monotonically() -> None:
    assumptions = CombustionAssumptions()
    angles = np.linspace(-30.0, 60.0, 181)
    fractions = [burned_fraction(float(a), -20.0, 50.0, assumptions) for a in angles]
    assert fractions[0] == 0.0
    assert fractions[-1] == 1.0
    assert all(b >= a for a, b in itertools.pairwise(fractions))


def test_wiebe_rate_integrates_to_one() -> None:
    assumptions = CombustionAssumptions()
    angles = np.linspace(-20.0, 30.0, 20001)
    rates = np.array([burn_rate_per_deg(float(a), -20.0, 50.0, assumptions) for a in angles])
    assert np.trapezoid(rates, angles) == pytest.approx(1.0, rel=1e-5)


def test_progress_at_fraction_inverts_the_wiebe_function() -> None:
    assumptions = CombustionAssumptions()
    progress = progress_at_fraction(0.5, assumptions)
    assert burned_fraction(progress * 50.0, 0.0, 50.0, assumptions) == pytest.approx(0.5)


def test_burn_duration_at_full_load_mbt_is_in_the_typical_range(i6: EngineDefinition) -> None:
    # research.md §7.2: 40-70 deg; the low-speed end may be a little shorter.
    assumptions = CombustionAssumptions()
    for rpm in (2000.0, 4000.0, 6000.0):
        mbt = i6.tables.ignition_mbt.lookup(units.rpm_to_rad_s(rpm), 100_000.0)
        assert 35.0 <= burn_duration_deg(mbt, assumptions) <= 75.0


# --- Woschni (§7.3) ---


def test_woschni_coefficient_has_a_plausible_magnitude() -> None:
    # Near firing TDC (50 bar, 2000 K, w ~ 2.28 * 12 m/s) Heywood shows a few kW/(m^2 K).
    h_c = woschni_coefficient_w_m2_k(0.084, 50.0e5, 2000.0, 2.28 * 12.0)
    assert 500.0 <= h_c <= 5000.0
