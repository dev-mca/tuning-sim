"""Unit conversions at the edges of the system.

Internally everything is SI (docs/research.md, "Grundsätze" and §0). These helpers convert
user-facing units (rpm, bar, PS, °C) to SI and back. They accept floats and numpy arrays.
"""

import math

import numpy as np
import numpy.typing as npt

type FloatArray = npt.NDArray[np.float64]

# The functions below are generic: `[T: (float, FloatArray)]` means T is either a float or a
# float array, and the result has the same type as the argument (like <T> in Java generics).

SECONDS_PER_MINUTE = 60.0
PASCAL_PER_BAR = 1.0e5
WATT_PER_PS = 735.5  # metric horsepower (research.md §0)
KELVIN_OFFSET = 273.15


def rpm_to_rad_s[T: (float, FloatArray)](speed_rpm: T) -> T:
    """Engine speed N [1/min] to angular velocity omega [rad/s]: omega = 2*pi*N/60 (§0)."""
    return speed_rpm * (2.0 * math.pi / SECONDS_PER_MINUTE)


def rad_s_to_rpm[T: (float, FloatArray)](speed_rad_s: T) -> T:
    """Angular velocity omega [rad/s] to engine speed N [1/min]."""
    return speed_rad_s * (SECONDS_PER_MINUTE / (2.0 * math.pi))


def bar_to_pa[T: (float, FloatArray)](pressure_bar: T) -> T:
    """Pressure [bar] to [Pa]."""
    return pressure_bar * PASCAL_PER_BAR


def pa_to_bar[T: (float, FloatArray)](pressure_pa: T) -> T:
    """Pressure [Pa] to [bar]."""
    return pressure_pa / PASCAL_PER_BAR


def w_to_ps[T: (float, FloatArray)](power_w: T) -> T:
    """Power [W] to metric horsepower [PS]."""
    return power_w / WATT_PER_PS


def celsius_to_kelvin[T: (float, FloatArray)](temperature_c: T) -> T:
    """Temperature [°C] to [K]."""
    return temperature_c + KELVIN_OFFSET
