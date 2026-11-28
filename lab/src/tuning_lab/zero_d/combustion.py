"""Heat release after Wiebe (docs/research.md §7.2)."""

import math

from tuning_lab.zero_d.parameters import CombustionAssumptions


def _wiebe_raw(progress: float, a: float, m: float) -> float:
    """1 - exp(-a progress^(m+1)), progress = (theta - theta_0) / delta_theta_b (§7.2)."""
    return 1.0 - math.exp(-a * progress ** (m + 1.0))


def burned_fraction(
    theta_deg: float, start_deg: float, duration_deg: float, assumptions: CombustionAssumptions
) -> float:
    """x_b(theta), normalised so that it goes exactly from 0 to 1 (research.md §7.2).

    The plain Wiebe function ends at 1 - exp(-a) (99.3 % for a = 5). Dividing by that value
    releases all of the fuel energy within the burn duration, which keeps the energy balance
    simple [assumption: the remaining 0.7 % are not left for the expansion stroke].
    """
    progress = (theta_deg - start_deg) / duration_deg
    if progress <= 0.0:
        return 0.0
    if progress >= 1.0:
        return 1.0
    a = assumptions.wiebe_efficiency_a
    m = assumptions.wiebe_form_m
    return _wiebe_raw(progress, a, m) / _wiebe_raw(1.0, a, m)


def burn_rate_per_deg(
    theta_deg: float, start_deg: float, duration_deg: float, assumptions: CombustionAssumptions
) -> float:
    """dx_b/dtheta of the normalised Wiebe function [1/deg] (research.md §7.2).

    d/dtheta (1 - exp(-a p^(m+1))) = a (m+1) / delta_theta_b * p^m * exp(-a p^(m+1)).
    """
    progress = (theta_deg - start_deg) / duration_deg
    if progress <= 0.0 or progress >= 1.0:
        return 0.0
    a = assumptions.wiebe_efficiency_a
    m = assumptions.wiebe_form_m
    rate = a * (m + 1.0) / duration_deg * progress**m * math.exp(-a * progress ** (m + 1.0))
    return rate / _wiebe_raw(1.0, a, m)


def progress_at_fraction(fraction: float, assumptions: CombustionAssumptions) -> float:
    """Inverse of the normalised Wiebe function: progress p where x_b = fraction."""
    a = assumptions.wiebe_efficiency_a
    m = assumptions.wiebe_form_m
    raw = fraction * _wiebe_raw(1.0, a, m)
    return (-math.log(1.0 - raw) / a) ** (1.0 / (m + 1.0))


def burn_duration_deg(ignition_mbt_deg: float, assumptions: CombustionAssumptions) -> float:
    """Burn duration delta_theta_b so that a spark at MBT puts CA50 at ca50_at_mbt_deg.

    [assumption] research.md §7.2 gives 40-70 deg but no rule. Here:
    CA50 = -theta_MBT + delay + p50 * delta_theta_b  =>  delta_theta_b = (theta_MBT + CA50
    - delay) / p50. A high MBT (low load, high speed) means a slow burn, as in a real engine.
    """
    p50 = progress_at_fraction(0.5, assumptions)
    duration = (
        ignition_mbt_deg + assumptions.ca50_at_mbt_deg - assumptions.ignition_delay_deg
    ) / p50
    return min(max(duration, assumptions.min_burn_duration_deg), assumptions.max_burn_duration_deg)
