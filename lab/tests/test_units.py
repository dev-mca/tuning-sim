import math

import numpy as np
import pytest

from tuning_lab import units


def test_rpm_to_rad_s_known_value() -> None:
    # 60 rpm is exactly one revolution per second = 2*pi rad/s.
    assert units.rpm_to_rad_s(60.0) == pytest.approx(2.0 * math.pi)


def test_rpm_round_trip() -> None:
    for speed_rpm in (0.0, 800.0, 3000.0, 7000.0):
        assert units.rad_s_to_rpm(units.rpm_to_rad_s(speed_rpm)) == pytest.approx(speed_rpm)


def test_rpm_conversion_works_on_arrays() -> None:
    speeds_rpm = np.array([1000.0, 3000.0, 6000.0])
    speeds_rad_s = units.rpm_to_rad_s(speeds_rpm)
    assert isinstance(speeds_rad_s, np.ndarray)
    np.testing.assert_allclose(speeds_rad_s, speeds_rpm * 2.0 * np.pi / 60.0)


def test_pressure_round_trip_and_magnitude() -> None:
    assert units.bar_to_pa(1.0) == pytest.approx(1.0e5)
    assert units.pa_to_bar(units.bar_to_pa(2.5)) == pytest.approx(2.5)


def test_power_magnitude() -> None:
    # Plausibility (research.md §10): 220 kW is about 299 PS.
    assert units.w_to_ps(220_000.0) == pytest.approx(299.1, abs=0.1)


def test_celsius_to_kelvin() -> None:
    assert units.celsius_to_kelvin(0.0) == pytest.approx(273.15)
    assert units.celsius_to_kelvin(-273.15) == pytest.approx(0.0)
