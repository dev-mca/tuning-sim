"""Throttle and intake manifold (filling and emptying model), docs/research.md §2.5."""

import math

from tuning_lab.constants import GAMMA_AIR, GAS_CONSTANT_AIR_J_KG_K
from tuning_lab.engine_definition import IntakeParameters

FULL_OPEN_ANGLE_DEG = 90.0

PRESSURE_RATIO_LINEAR = 0.99
"""Above this pressure ratio the flow function is replaced by a straight line to zero at 1.

Psi(Pi) has an infinite slope at Pi = 1 (sqrt behaviour). That makes the manifold equation
extremely stiff near wide-open throttle. A linear section keeps the slope finite while changing
the flow by less than 1 %. Common regularisation in mean-value models (Guzzella & Onder). [prüfen]
"""


def critical_pressure_ratio(gamma: float = GAMMA_AIR) -> float:
    """Pi_crit = (2/(gamma+1))^(gamma/(gamma-1)), about 0.528 for air (research.md §2.5)."""
    return (2.0 / (gamma + 1.0)) ** (gamma / (gamma - 1.0))


def _psi_subsonic(pressure_ratio: float, gamma: float) -> float:
    return math.sqrt(
        2.0
        * gamma
        / (gamma - 1.0)
        * (pressure_ratio ** (2.0 / gamma) - pressure_ratio ** ((gamma + 1.0) / gamma))
    )


def flow_function(pressure_ratio: float, gamma: float = GAMMA_AIR) -> float:
    """Dimensionless flow function Psi(Pi) of a compressible orifice, Pi = p_down / p_up.

    research.md §2.5: subsonic formula above Pi_crit, constant (choked flow) below.
    """
    if pressure_ratio >= 1.0:
        return 0.0
    if pressure_ratio > PRESSURE_RATIO_LINEAR:
        psi_at_linear = _psi_subsonic(PRESSURE_RATIO_LINEAR, gamma)
        return psi_at_linear * (1.0 - pressure_ratio) / (1.0 - PRESSURE_RATIO_LINEAR)
    pressure_ratio = max(pressure_ratio, critical_pressure_ratio(gamma))
    return _psi_subsonic(pressure_ratio, gamma)


def throttle_area_m2(throttle: float, intake: IntakeParameters) -> float:
    """Open area of a throttle plate for throttle position 0..1 (research.md §2.5).

    A = pi D^2/4 * (1 - cos(alpha)/cos(alpha_0)) plus a small leak area for idle.
    Position 0 = closed angle alpha_0, position 1 = 90 deg (fully open).
    """
    position = min(max(throttle, 0.0), 1.0)
    closed_deg = intake.throttle_closed_angle_deg
    angle = math.radians(closed_deg + position * (FULL_OPEN_ANGLE_DEG - closed_deg))
    bore_area = math.pi / 4.0 * intake.throttle_diameter_m**2
    plate_area = bore_area * (1.0 - math.cos(angle) / math.cos(math.radians(closed_deg)))
    return plate_area + intake.throttle_leak_area_m2


def orifice_mass_flow_kg_s(
    area_m2: float,
    discharge_coefficient: float,
    upstream_pressure_pa: float,
    upstream_temperature_k: float,
    downstream_pressure_pa: float,
) -> float:
    """Mass flow through an orifice: Cd * A * p_up / sqrt(R T_up) * Psi(p_down/p_up).

    Positive from upstream to downstream. If the downstream pressure is higher, the flow
    reverses (negative result), e.g. when the manifold pressure exceeds ambient.
    """
    if downstream_pressure_pa > upstream_pressure_pa:
        return -orifice_mass_flow_kg_s(
            area_m2,
            discharge_coefficient,
            downstream_pressure_pa,
            upstream_temperature_k,
            upstream_pressure_pa,
        )
    density_term = upstream_pressure_pa / math.sqrt(
        GAS_CONSTANT_AIR_J_KG_K * upstream_temperature_k
    )
    psi = flow_function(downstream_pressure_pa / upstream_pressure_pa)
    return discharge_coefficient * area_m2 * density_term * psi


def manifold_pressure_derivative_pa_s(
    throttle_flow_kg_s: float,
    cylinder_flow_kg_s: float,
    manifold_temperature_k: float,
    manifold_volume_m3: float,
) -> float:
    """Isothermal filling and emptying: dp_m/dt = R T_m / V_m * (mdot_thr - mdot_air), §2.5."""
    return (
        GAS_CONSTANT_AIR_J_KG_K
        * manifold_temperature_k
        / manifold_volume_m3
        * (throttle_flow_kg_s - cylinder_flow_kg_s)
    )
