"""Gas properties of the cylinder charge for the 0D model (docs/research.md §0, §7.1).

[assumption] One ideal gas for fresh mixture, residual and burned gas: R of air and a specific
heat c_v that rises linearly with temperature. The line is fitted to the single-zone
correlation gamma(T) = 1.338 - 6.0e-5 T + 1.0e-8 T^2 (Brunt et al., SAE 981052; used in
Heywood-style heat-release analysis) [prüfen]: gamma ~ 1.32 at 300 K, ~ 1.25 at 2500 K.
A constant gamma = 1.4 (air) would overestimate the work of the expansion stroke.

Because c_v is linear in T, u(T) is a quadratic and T(u) has a closed form. The model can then
integrate the internal energy U = m u directly, which keeps the first law exact (§7.1).
"""

import math

from tuning_lab.constants import GAS_CONSTANT_AIR_J_KG_K

CV_AT_ZERO_K_J_KG_K = 850.0
"""Intercept of c_v(T) = c0 + k T [J/(kg K)], see module docstring."""

CV_SLOPE_J_KG_K2 = 0.12
"""Slope k of c_v(T) [J/(kg K^2)], see module docstring."""


def specific_heat_cv_j_kg_k(temperature_k: float) -> float:
    """c_v(T) = c0 + k T."""
    return CV_AT_ZERO_K_J_KG_K + CV_SLOPE_J_KG_K2 * temperature_k


def gamma(temperature_k: float) -> float:
    """Isentropic exponent gamma = c_p / c_v = 1 + R / c_v (ideal gas, §0)."""
    return 1.0 + GAS_CONSTANT_AIR_J_KG_K / specific_heat_cv_j_kg_k(temperature_k)


def internal_energy_j_kg(temperature_k: float) -> float:
    """u(T) = integral of c_v dT from 0 K = c0 T + k T^2 / 2."""
    return temperature_k * (CV_AT_ZERO_K_J_KG_K + 0.5 * CV_SLOPE_J_KG_K2 * temperature_k)


def enthalpy_j_kg(temperature_k: float) -> float:
    """h(T) = u(T) + R T (ideal gas)."""
    return internal_energy_j_kg(temperature_k) + GAS_CONSTANT_AIR_J_KG_K * temperature_k


def temperature_from_internal_energy_k(internal_energy: float) -> float:
    """Inverse of u(T): positive root of k/2 T^2 + c0 T - u = 0."""
    c0 = CV_AT_ZERO_K_J_KG_K
    k = CV_SLOPE_J_KG_K2
    return (math.sqrt(c0 * c0 + 2.0 * k * internal_energy) - c0) / k


def temperature_from_enthalpy_k(enthalpy: float) -> float:
    """Inverse of h(T): positive root of k/2 T^2 + (c0 + R) T - h = 0."""
    c = CV_AT_ZERO_K_J_KG_K + GAS_CONSTANT_AIR_J_KG_K
    k = CV_SLOPE_J_KG_K2
    return (math.sqrt(c * c + 2.0 * k * enthalpy) - c) / k
