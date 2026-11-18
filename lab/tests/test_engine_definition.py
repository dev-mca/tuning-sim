import pytest

from tuning_lab import units
from tuning_lab.engine_definition import EngineDefinition

FULL_CYCLE_DEG = 720.0


def test_displacement_from_bore_and_stroke(i6: EngineDefinition) -> None:
    # 6 * pi/4 * 0.084^2 * 0.090 m^3 = 2.993 l (plan.md rounds to "2.998 l")
    assert i6.displacement_m3 == pytest.approx(2.9926e-3, rel=1.0e-4)


def test_firing_angles_are_evenly_spaced_in_firing_order(i6: EngineDefinition) -> None:
    angles_in_order = [i6.firing_angles_deg[cylinder - 1] for cylinder in i6.firing_order]
    spacing = FULL_CYCLE_DEG / i6.cylinders  # research.md §1.2: 120 deg for an I6
    assert angles_in_order == pytest.approx([k * spacing for k in range(i6.cylinders)])


def test_loaded_values_are_si(i6: EngineDefinition) -> None:
    assert i6.rev_limit_rad_s == pytest.approx(units.rpm_to_rad_s(7000.0))
    assert i6.friction.c0_pa == pytest.approx(55_000.0)
    assert i6.intake.manifold_volume_m3 == pytest.approx(0.0035)


def test_mean_piston_speed_at_rev_limit_is_plausible(i6: EngineDefinition) -> None:
    # research.md §1.4 and §10: v_p = 2 S N / 60, 15-22 m/s at rated speed.
    mean_piston_speed = 2.0 * i6.stroke_m * units.rad_s_to_rpm(i6.rev_limit_rad_s) / 60.0
    assert 15.0 <= mean_piston_speed <= 22.0
