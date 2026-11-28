"""Physical constants and fuel properties (SI units, docs/research.md §0)."""

import math

GAS_CONSTANT_AIR_J_KG_K = 287.05
"""Specific gas constant of dry air R [J/(kg K)]."""

GAMMA_AIR = 1.40
"""Isentropic exponent of air."""

CP_EXHAUST_J_KG_K = 1150.0
"""Specific heat capacity of exhaust gas at constant pressure [J/(kg K)], approximate."""

AFR_STOICH_GASOLINE = 14.7
"""Stoichiometric air/fuel mass ratio of E0 gasoline."""

LOWER_HEATING_VALUE_GASOLINE_J_KG = 43.0e6
"""Lower heating value of gasoline [J/kg] (research.md §0: 42-44 MJ/kg)."""

AMBIENT_PRESSURE_PA = 101_325.0
"""Standard ambient pressure [Pa]."""

AMBIENT_TEMPERATURE_K = 298.15
"""Ambient (and, for now, intake manifold) temperature [K], 25 degC."""

RADIANS_PER_CYCLE_4_STROKE = 4.0 * math.pi
"""One four-stroke cycle takes two revolutions = 4 pi rad (research.md §1.1)."""

STANDARD_GRAVITY_M_S2 = 9.80665
"""Standard acceleration of gravity g [m/s^2] (rolling resistance, research.md §8.2)."""
