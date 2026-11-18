"""Throttle and manifold, research.md §2.5."""

import math

import numpy as np
import pytest

from tuning_lab.constants import AMBIENT_PRESSURE_PA, AMBIENT_TEMPERATURE_K, GAMMA_AIR
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.intake import (
    PRESSURE_RATIO_LINEAR,
    critical_pressure_ratio,
    flow_function,
    manifold_pressure_derivative_pa_s,
    orifice_mass_flow_kg_s,
    throttle_area_m2,
)


def test_critical_pressure_ratio_of_air() -> None:
    assert critical_pressure_ratio() == pytest.approx(0.528, abs=1.0e-3)


def test_choked_flow_function_matches_closed_form() -> None:
    gamma = GAMMA_AIR
    choked = math.sqrt(gamma * (2.0 / (gamma + 1.0)) ** ((gamma + 1.0) / (gamma - 1.0)))
    assert flow_function(0.2) == pytest.approx(choked)
    assert flow_function(critical_pressure_ratio()) == pytest.approx(choked)


def test_flow_function_is_zero_without_pressure_drop() -> None:
    assert flow_function(1.0) == 0.0
    assert flow_function(1.2) == 0.0


def test_flow_function_is_continuous_and_falls_towards_one() -> None:
    ratios = np.linspace(0.3, 1.0, 2001)
    psi = np.array([flow_function(float(r)) for r in ratios])
    assert np.all(np.diff(psi) <= 1.0e-12)  # never increases with the pressure ratio
    assert np.max(np.abs(np.diff(psi))) < 0.01  # no jumps (incl. at Pi_crit and Pi_linear)
    left = flow_function(PRESSURE_RATIO_LINEAR - 1.0e-9)
    right = flow_function(PRESSURE_RATIO_LINEAR + 1.0e-9)
    assert left == pytest.approx(right, rel=1.0e-6)


def test_throttle_area_grows_with_position(i6: EngineDefinition) -> None:
    intake = i6.intake
    areas = [throttle_area_m2(p, intake) for p in np.linspace(0.0, 1.0, 11)]
    assert np.all(np.diff(areas) > 0.0)
    assert areas[0] == pytest.approx(intake.throttle_leak_area_m2)
    bore_area = math.pi / 4.0 * intake.throttle_diameter_m**2
    assert areas[-1] == pytest.approx(bore_area + intake.throttle_leak_area_m2)


def test_orifice_flow_reverses_with_the_pressure_difference() -> None:
    forward = orifice_mass_flow_kg_s(1.0e-3, 0.8, 101_325.0, AMBIENT_TEMPERATURE_K, 50_000.0)
    backward = orifice_mass_flow_kg_s(1.0e-3, 0.8, 50_000.0, AMBIENT_TEMPERATURE_K, 101_325.0)
    assert forward > 0.0
    assert backward < 0.0


def test_wide_open_throttle_flow_magnitude(i6: EngineDefinition) -> None:
    # Choked flow through a 70 mm throttle at ambient: order of 0.7 kg/s, far above what a
    # 3 l engine needs (about 0.2 kg/s at 7000 1/min), so WOT manifold pressure stays near p0.
    area = throttle_area_m2(1.0, i6.intake)
    flow = orifice_mass_flow_kg_s(
        area, i6.intake.throttle_discharge_coefficient, AMBIENT_PRESSURE_PA,
        AMBIENT_TEMPERATURE_K, 30_000.0,
    )  # fmt: skip
    assert 0.5 < flow < 1.0


def test_manifold_pressure_rises_when_more_air_enters_than_leaves() -> None:
    assert manifold_pressure_derivative_pa_s(0.02, 0.01, 300.0, 0.0035) > 0.0
    assert manifold_pressure_derivative_pa_s(0.01, 0.02, 300.0, 0.0035) < 0.0
    assert manifold_pressure_derivative_pa_s(0.01, 0.01, 300.0, 0.0035) == 0.0
