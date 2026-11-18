"""2D lookup tables (ECU maps) with bilinear interpolation, docs/research.md §2.3.

The JSON format follows docs/plan.md §5.1 (TunerStudio-like): axis bins plus a value grid
``values[y][x]``, stored in user-facing units (1/min, kPa, %). ``load_table_si`` converts axes
and values to SI at this edge. Exception: crank angles stay in degrees (suffix ``_deg``),
because all ignition formulas in research.md are written in degrees.
"""

import json
from bisect import bisect_right
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

from tuning_lab import units
from tuning_lab.units import FloatArray

MIN_BINS_PER_AXIS = 2

# Converters from the unit written in the JSON file to the SI unit used inside the model.
_TO_SI: dict[str, Callable[[float], float]] = {
    "1/min": units.rpm_to_rad_s,  # engine speed -> rad/s
    "kPa": units.kpa_to_pa,  # pressure -> Pa
    "%": lambda value: value / 100.0,  # percent -> fraction
    "lambda": lambda value: value,  # dimensionless
    "deg BTDC": lambda value: value,  # crank angle stays in degrees (see module docstring)
}


@dataclass(frozen=True)
class TableBin:
    """Position of a value between two bins: lower bin index and fraction t in [0, 1]."""

    index: int
    fraction: float


class Table2D:
    """Immutable 2D table, z = f(x, y), bilinear interpolation, clamped at the edges."""

    def __init__(
        self, x_bins: Sequence[float], y_bins: Sequence[float], values: Sequence[Sequence[float]]
    ) -> None:
        self._x_bins = _validated_bins(x_bins, "x")
        self._y_bins = _validated_bins(y_bins, "y")
        grid = np.asarray(values, dtype=np.float64)
        expected_shape = (len(self._y_bins), len(self._x_bins))
        if grid.shape != expected_shape:
            raise ValueError(f"values must have shape {expected_shape} (y, x), got {grid.shape}")
        if not np.all(np.isfinite(grid)):
            raise ValueError("values must be finite")
        grid.flags.writeable = False
        self._values = grid

    @property
    def x_bins(self) -> FloatArray:
        return self._x_bins

    @property
    def y_bins(self) -> FloatArray:
        return self._y_bins

    @property
    def values(self) -> FloatArray:
        return self._values

    def with_offset(self, delta: float) -> "Table2D":
        """Copy with every cell shifted by delta (e.g. ignition +3 deg)."""
        return Table2D(self._x_bins, self._y_bins, self._values + delta)

    def scaled(self, factor: float) -> "Table2D":
        """Copy with every cell multiplied by factor (e.g. VE -10 %)."""
        return Table2D(self._x_bins, self._y_bins, self._values * factor)

    def filled(self, value: float) -> "Table2D":
        """Copy with the same axes and every cell set to value (e.g. lambda 1.0 everywhere)."""
        return Table2D(self._x_bins, self._y_bins, np.full_like(self._values, value))

    def lookup(self, x: float, y: float) -> float:
        """Bilinear interpolation (research.md §2.3); inputs outside the axes are clamped."""
        bx = _find_bin(self._x_bins, x)
        by = _find_bin(self._y_bins, y)
        t, u = bx.fraction, by.fraction
        i, j = bx.index, by.index
        v = self._values
        return float(
            (1.0 - t) * (1.0 - u) * v[j, i]
            + t * (1.0 - u) * v[j, i + 1]
            + (1.0 - t) * u * v[j + 1, i]
            + t * u * v[j + 1, i + 1]
        )


def _validated_bins(bins: Sequence[float], axis_name: str) -> FloatArray:
    array = np.asarray(bins, dtype=np.float64)
    if array.ndim != 1 or len(array) < MIN_BINS_PER_AXIS:
        raise ValueError(f"{axis_name} axis needs at least {MIN_BINS_PER_AXIS} bins")
    if not np.all(np.diff(array) > 0.0):
        raise ValueError(f"{axis_name} bins must be strictly increasing")
    array.flags.writeable = False
    return array


def _find_bin(bins: FloatArray, value: float) -> TableBin:
    """Binary search for i with bins[i] <= value <= bins[i+1]; clamps outside the axis."""
    last = len(bins) - 1
    if value <= bins[0]:
        return TableBin(0, 0.0)
    if value >= bins[last]:
        return TableBin(last - 1, 1.0)
    index = bisect_right(bins, value) - 1
    lower, upper = bins[index], bins[index + 1]
    return TableBin(index, float((value - lower) / (upper - lower)))


# --- JSON I/O ----------------------------------------------------------------------------


def read_table_json(path: Path) -> dict[str, Any]:
    """Reads a table file as a raw dict (user-facing units)."""
    with path.open(encoding="utf-8") as file:
        data: dict[str, Any] = json.load(file)
    return data


def write_table_json(path: Path, data: dict[str, Any]) -> None:
    """Writes a table file: one line per top-level field and per grid row (readable diffs)."""
    lines: list[str] = []
    for key, value in data.items():
        if key == "values":
            rows = ",\n".join("    " + json.dumps(row) for row in value)
            lines.append(f'  "values": [\n{rows}\n  ]')
        else:
            lines.append(f"  {json.dumps(key)}: {json.dumps(value, ensure_ascii=False)}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("{\n" + ",\n".join(lines) + "\n}\n", encoding="utf-8")


def table_from_dict_si(data: dict[str, Any]) -> Table2D:
    """Builds a Table2D in SI units from a raw table dict."""
    to_x = _converter(data["xAxis"]["unit"])
    to_y = _converter(data["yAxis"]["unit"])
    to_value = _converter(data["unit"])
    return Table2D(
        [to_x(b) for b in data["xAxis"]["bins"]],
        [to_y(b) for b in data["yAxis"]["bins"]],
        [[to_value(v) for v in row] for row in data["values"]],
    )


def load_table_si(path: Path) -> Table2D:
    """Loads a table file and converts it to SI units."""
    return table_from_dict_si(read_table_json(path))


def _converter(unit: str) -> Callable[[float], float]:
    try:
        return _TO_SI[unit]
    except KeyError:
        raise ValueError(f"unknown unit {unit!r}, known: {sorted(_TO_SI)}") from None
