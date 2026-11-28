"""Slider-crank kinematics of one cylinder (docs/research.md §1.4).

Crank angle theta in deg, 0 = TDC. Functions accept floats and numpy arrays.
"""

import math

import numpy as np

from tuning_lab.units import FloatArray
from tuning_lab.zero_d.parameters import CylinderGeometry

RAD_PER_DEG = math.pi / 180.0


class CylinderKinematics:
    """Volume, dV/dtheta and wall area over crank angle for one cylinder."""

    def __init__(self, geometry: CylinderGeometry) -> None:
        self._geometry = geometry
        self._crank_radius_m = geometry.stroke_m / 2.0
        self._piston_area_m2 = math.pi / 4.0 * geometry.bore_m**2
        # §1.4: V_h = pi B^2 S / 4, V_c = V_h / (epsilon - 1)
        self._swept_volume_m3 = self._piston_area_m2 * geometry.stroke_m
        self._clearance_volume_m3 = self._swept_volume_m3 / (geometry.compression_ratio - 1.0)

    @property
    def geometry(self) -> CylinderGeometry:
        return self._geometry

    @property
    def swept_volume_m3(self) -> float:
        return self._swept_volume_m3

    @property
    def clearance_volume_m3(self) -> float:
        return self._clearance_volume_m3

    @property
    def piston_area_m2(self) -> float:
        return self._piston_area_m2

    def piston_position_m[T: (float, FloatArray)](self, theta_deg: T) -> T:
        """s(theta) = a + l - (a cos(theta) + sqrt(l^2 - a^2 sin^2(theta))) (§1.4)."""
        a = self._crank_radius_m
        rod = self._geometry.conrod_length_m
        theta = np.asarray(theta_deg) * RAD_PER_DEG
        sin_theta = np.sin(theta)
        position = a + rod - (a * np.cos(theta) + np.sqrt(rod * rod - (a * sin_theta) ** 2))
        return _like(theta_deg, position)

    def volume_m3[T: (float, FloatArray)](self, theta_deg: T) -> T:
        """V(theta) = V_c + pi B^2/4 * s(theta) (§1.4)."""
        return self._clearance_volume_m3 + self._piston_area_m2 * self.piston_position_m(theta_deg)

    def volume_derivative_m3_per_deg[T: (float, FloatArray)](self, theta_deg: T) -> T:
        """dV/dtheta from the derivative of s(theta) (§1.4), per deg crank angle.

        ds/dtheta = a sin(theta) (1 + a cos(theta) / sqrt(l^2 - a^2 sin^2(theta))) per rad.
        """
        a = self._crank_radius_m
        rod = self._geometry.conrod_length_m
        theta = np.asarray(theta_deg) * RAD_PER_DEG
        sin_theta = np.sin(theta)
        root = np.sqrt(rod * rod - (a * sin_theta) ** 2)
        ds_drad = a * sin_theta * (1.0 + a * np.cos(theta) / root)
        return _like(theta_deg, self._piston_area_m2 * ds_drad * RAD_PER_DEG)

    def wall_area_m2[T: (float, FloatArray)](self, theta_deg: T) -> T:
        """Gas-side wall area: head + piston crown + exposed liner (for §7.3).

        [assumption] Flat head and flat piston, each the bore area; the liner exposed above
        the piston has the height of the clearance volume plus s(theta).
        """
        clearance_height_m = self._clearance_volume_m3 / self._piston_area_m2
        liner_height_m = clearance_height_m + self.piston_position_m(theta_deg)
        return 2.0 * self._piston_area_m2 + math.pi * self._geometry.bore_m * liner_height_m

    def mean_piston_speed_m_s(self, speed_rad_s: float) -> float:
        """v_p = 2 S N / 60 = S omega / pi (§1.4)."""
        return self._geometry.stroke_m * speed_rad_s / math.pi


def _like[T: (float, FloatArray)](template: T, value: FloatArray) -> T:
    """Returns a float for float input and an array for array input."""
    if isinstance(template, np.ndarray):
        return value
    return float(value)  # type: ignore[return-value]
