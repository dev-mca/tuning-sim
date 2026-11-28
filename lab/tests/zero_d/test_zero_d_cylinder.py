"""0D cylinder cycle: motoring, energy balance, determinism, step size (research.md §7, §9)."""

from dataclasses import replace

import numpy as np
import pytest

from conftest import stock_point
from tuning_lab import units
from tuning_lab.constants import LOWER_HEATING_VALUE_GASOLINE_J_KG
from tuning_lab.zero_d import gas
from tuning_lab.zero_d.cylinder import (
    CycleResult,
    SolverSettings,
    ZeroDOperatingPoint,
    simulate_cycle,
    steps_per_cycle,
)
from tuning_lab.zero_d.engine import ZeroDEngineResult
from tuning_lab.zero_d.parameters import ZeroDEngine
from tuning_lab.zero_d.validation import full_load_manifold_pressure_pa

MOTORING_RPM = 2000.0
CLOSED_PHASE_HALF_WIDTH_DEG = 120.0
"""IVC is 130 deg before and EVO 130 deg after firing TDC; stay inside with a margin."""


def motoring_point(
    heat_transfer: bool, engine: ZeroDEngine
) -> tuple[ZeroDEngine, ZeroDOperatingPoint]:
    variant = replace(engine, heat_transfer=replace(engine.heat_transfer, enabled=heat_transfer))
    point = ZeroDOperatingPoint(
        speed_rad_s=units.rpm_to_rad_s(MOTORING_RPM),
        intake_pressure_pa=100_000.0,
        ignition_deg=0.0,
        fired=False,
    )
    return variant, point


@pytest.fixture(scope="module")
def motoring_adiabatic(zero_d_engine: ZeroDEngine) -> CycleResult:
    return simulate_cycle(*motoring_point(False, zero_d_engine))


@pytest.fixture(scope="module")
def motoring_with_heat(zero_d_engine: ZeroDEngine) -> CycleResult:
    return simulate_cycle(*motoring_point(True, zero_d_engine))


def _pressure_at(result: CycleResult, angles_deg: np.ndarray) -> np.ndarray:
    trace = result.trace
    return np.interp(angles_deg, trace.crank_angle_deg, trace.pressure_pa)


def _polytropic_exponent(result: CycleResult, from_deg: float, to_deg: float) -> float:
    """Slope of ln p over -ln V during compression: p V^n = const."""
    trace = result.trace
    window = (trace.crank_angle_deg >= from_deg) & (trace.crank_angle_deg <= to_deg)
    slope = np.polyfit(np.log(trace.volume_m3[window]), np.log(trace.pressure_pa[window]), 1)[0]
    return float(-slope)


# --- motoring (no combustion) ---


def test_adiabatic_motoring_pressure_is_symmetric_around_tdc(
    motoring_adiabatic: CycleResult,
) -> None:
    # Closed, adiabatic, no flow: compression and expansion are the same isentrope (§7.1).
    angles = np.linspace(1.0, CLOSED_PHASE_HALF_WIDTH_DEG, 120)
    np.testing.assert_allclose(
        _pressure_at(motoring_adiabatic, angles), _pressure_at(motoring_adiabatic, -angles),
        rtol=1e-4,
    )  # fmt: skip


def test_adiabatic_motoring_exponent_equals_gamma(motoring_adiabatic: CycleResult) -> None:
    n = _polytropic_exponent(motoring_adiabatic, -100.0, -30.0)
    gamma_range = (gas.gamma(800.0), gas.gamma(300.0))
    assert gamma_range[0] <= n <= gamma_range[1]


def test_motoring_with_wall_heat_is_plausible(motoring_with_heat: CycleResult) -> None:
    # Heywood ch. 12: polytropic exponent of compression about 1.3; wall heat makes the peak
    # come slightly before TDC ("thermodynamic loss angle") and lowers expansion pressure.
    n = _polytropic_exponent(motoring_with_heat, -100.0, -30.0)
    assert 1.25 <= n <= 1.38
    trace = motoring_with_heat.trace
    peak_angle = trace.crank_angle_deg[np.argmax(trace.pressure_pa)]
    assert -3.0 <= peak_angle <= 0.0
    angles = np.array([30.0, 60.0, 90.0])
    assert np.all(
        _pressure_at(motoring_with_heat, angles) < _pressure_at(motoring_with_heat, -angles)
    )


def test_motoring_peak_pressure_magnitude(motoring_with_heat: CycleResult) -> None:
    # p_max ~ p_IVC * (V_IVC / V_c)^n with an effective compression ratio of ~9: 15-25 bar.
    assert 15.0e5 <= motoring_with_heat.trace.pressure_pa.max() <= 25.0e5


def test_motoring_needs_net_work(motoring_with_heat: CycleResult) -> None:
    # A motored engine absorbs work (pumping + heat loss), research.md §2.8.
    assert motoring_with_heat.energy.net_work_j < 0.0


# --- fired cycle: energy and mass ---


def test_cycle_converges(full_load: ZeroDEngineResult) -> None:
    assert full_load.cylinder.converged
    assert full_load.cylinder.cycles < 20


def test_first_law_bookkeeping_is_exact(full_load: ZeroDEngineResult) -> None:
    energy = full_load.cylinder.energy
    assert abs(energy.balance_residual_j) <= 1e-9 * energy.heat_release_j


def test_energy_balance_closes_over_the_periodic_cycle(full_load: ZeroDEngineResult) -> None:
    # Periodic state: dU ~ 0, so Q_comb = W + Q_wall + (H_out - H_in) (§7.1).
    energy = full_load.cylinder.energy
    assert abs(energy.internal_energy_change_j) <= 1e-4 * energy.heat_release_j


def test_heat_release_equals_fuel_energy(
    full_load: ZeroDEngineResult, zero_d_engine: ZeroDEngine
) -> None:
    cylinder = full_load.cylinder
    expected = (
        cylinder.burned_fuel_kg
        * LOWER_HEATING_VALUE_GASOLINE_J_KG
        * zero_d_engine.combustion.combustion_efficiency
    )
    assert cylinder.energy.heat_release_j == pytest.approx(expected, rel=1e-4)


def test_work_matches_closed_integral_of_the_pv_trace(full_load: ZeroDEngineResult) -> None:
    trace = full_load.cylinder.trace
    work_from_trace = np.trapezoid(trace.pressure_pa, trace.volume_m3)
    assert work_from_trace == pytest.approx(full_load.cylinder.energy.net_work_j, rel=2e-3)


def test_mass_is_conserved_over_the_cycle(full_load: ZeroDEngineResult) -> None:
    cylinder = full_load.cylinder
    assert cylinder.trace.mass_kg[-1] == pytest.approx(cylinder.trace.mass_kg[0], rel=1e-5)
    inducted_mixture = cylinder.inducted_air_kg + cylinder.fuel_kg * (
        cylinder.inducted_air_kg / cylinder.trapped_air_kg
    )
    assert cylinder.exhaust_mass_kg == pytest.approx(inducted_mixture, rel=1e-3)


def test_burned_fraction_trace_goes_from_zero_to_one(full_load: ZeroDEngineResult) -> None:
    burned = full_load.cylinder.trace.burned_fraction
    assert burned[0] == 0.0
    assert burned[-1] == pytest.approx(1.0, abs=1e-6)
    assert np.all(np.diff(burned) >= -1e-12)


# --- numerics ---


def test_simulation_is_deterministic(
    zero_d_engine: ZeroDEngine, full_load_point: ZeroDOperatingPoint
) -> None:
    first = simulate_cycle(zero_d_engine, full_load_point)
    second = simulate_cycle(zero_d_engine, full_load_point)
    np.testing.assert_array_equal(first.trace.pressure_pa, second.trace.pressure_pa)
    np.testing.assert_array_equal(
        first.trace.exhaust_mass_flow_kg_s, second.trace.exhaust_mass_flow_kg_s
    )
    assert first.energy == second.energy


def test_default_step_at_rev_limit_is_at_most_two_degrees(zero_d_engine: ZeroDEngine) -> None:
    # docs/plan.md iteration 11: step <= 2 deg crank angle at the rev limit.
    steps = steps_per_cycle(zero_d_engine.definition.rev_limit_rad_s, SolverSettings())
    assert 720.0 / steps <= 2.0


@pytest.mark.parametrize("speed_rpm", [3000.0, 7000.0])
def test_results_converge_with_the_step_size(zero_d_engine: ZeroDEngine, speed_rpm: float) -> None:
    # research.md §9: compare dt and dt/2 (here 2 deg vs 0.5 deg); results barely change.
    definition = zero_d_engine.definition
    manifold = full_load_manifold_pressure_pa(definition, units.rpm_to_rad_s(speed_rpm))
    point = stock_point(definition, speed_rpm, manifold)
    coarse = simulate_cycle(
        zero_d_engine, point, SolverSettings(max_step_deg=2.0, max_time_step_s=1.0)
    )
    fine = simulate_cycle(
        zero_d_engine, point, SolverSettings(max_step_deg=0.5, max_time_step_s=1.0)
    )
    assert coarse.step_deg == pytest.approx(2.0)
    assert coarse.energy.net_work_j == pytest.approx(fine.energy.net_work_j, rel=2e-3)
    assert coarse.trapped_air_kg == pytest.approx(fine.trapped_air_kg, rel=2e-3)
    assert coarse.exhaust_temperature_k == pytest.approx(fine.exhaust_temperature_k, rel=2e-3)


def test_too_long_steps_fail_loudly(zero_d_engine: ZeroDEngine) -> None:
    # Explicit RK4 is only conditionally stable; at low speed 4 deg lasts 0.7 ms.
    point = stock_point(zero_d_engine.definition, 1000.0, 100_000.0)
    with pytest.raises(FloatingPointError):
        simulate_cycle(zero_d_engine, point, SolverSettings(max_step_deg=4.0, max_time_step_s=1.0))
