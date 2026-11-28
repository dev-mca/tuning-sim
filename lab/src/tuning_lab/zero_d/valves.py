"""Valve lift and valve flow (docs/research.md §7.4 with the orifice equation of §2.5)."""

import math

from tuning_lab.constants import GAS_CONSTANT_AIR_J_KG_K
from tuning_lab.intake import flow_function
from tuning_lab.zero_d.parameters import ValveGeometry

DEG_PER_CYCLE = 720.0


def crank_angle_in_cycle_deg(theta_deg: float) -> float:
    """Maps any crank angle to 0..720 deg (one four-stroke cycle, research.md §1.1)."""
    return theta_deg % DEG_PER_CYCLE


def valve_lift_m(theta_deg: float, open_deg: float, close_deg: float, max_lift_m: float) -> float:
    """Lift curve L_v(theta) between opening and closing (research.md §7.4).

    [assumption] sin^2 profile ("cosine cam"): zero lift and zero slope at both ends, maximum
    in the middle. Real cams have ramps and a flatter top; the sin^2 shape has the same
    duration and peak but a little less area under the curve.
    """
    duration_deg = (close_deg - open_deg) % DEG_PER_CYCLE
    since_open_deg = (theta_deg - open_deg) % DEG_PER_CYCLE
    if since_open_deg >= duration_deg:
        return 0.0
    return max_lift_m * math.sin(math.pi * since_open_deg / duration_deg) ** 2


def valve_flow_area_m2(lift_m: float, valve: ValveGeometry) -> float:
    """A_v = n * pi d_v L_v, limited by the seat area n * pi d_v^2 / 4 (research.md §7.4).

    The curtain area pi d L equals the seat area at L = d/4; above that the port limits.
    """
    curtain_m2 = math.pi * valve.diameter_m * lift_m
    seat_m2 = math.pi / 4.0 * valve.diameter_m**2
    return valve.count * min(curtain_m2, seat_m2)


def orifice_mass_flow_kg_s(
    effective_area_m2: float,
    upstream_pressure_pa: float,
    upstream_temperature_k: float,
    downstream_pressure_pa: float,
    upstream_gamma: float,
) -> float:
    """mdot = C_d A p_up / sqrt(R T_up) * Psi(p_down / p_up) (research.md §2.5, §7.4).

    effective_area_m2 is C_d * A. Flow from upstream to downstream, >= 0. Psi is the
    existing function of the intake model (with its linear section near Pi = 1, which keeps
    the flow equation non-stiff when the valve is wide open and the pressures are nearly equal).
    """
    density_term = upstream_pressure_pa / math.sqrt(
        GAS_CONSTANT_AIR_J_KG_K * upstream_temperature_k
    )
    psi = flow_function(downstream_pressure_pa / upstream_pressure_pa, upstream_gamma)
    return effective_area_m2 * density_term * psi
