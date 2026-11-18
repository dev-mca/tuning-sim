"""Table2D: research.md §2.3 lists the properties to test."""

import json
from pathlib import Path

import numpy as np
import pytest

from tuning_lab import units
from tuning_lab.paths import SHARED_DIR
from tuning_lab.table import Table2D, load_table_si, read_table_json, write_table_json

X_BINS = [0.0, 10.0, 30.0]
Y_BINS = [100.0, 200.0]
# f(x, y) = x + 0.01 * y is bilinear, so interpolation must reproduce it exactly.
VALUES = [[x + 0.01 * y for x in X_BINS] for y in Y_BINS]


@pytest.fixture
def table() -> Table2D:
    return Table2D(X_BINS, Y_BINS, VALUES)


def test_exact_at_every_node(table: Table2D) -> None:
    for j, y in enumerate(Y_BINS):
        for i, x in enumerate(X_BINS):
            assert table.lookup(x, y) == pytest.approx(VALUES[j][i])


def test_reproduces_bilinear_function_between_nodes(table: Table2D) -> None:
    for x, y in [(5.0, 150.0), (20.0, 120.0), (29.9, 199.0), (0.1, 100.1)]:
        assert table.lookup(x, y) == pytest.approx(x + 0.01 * y)


def test_linear_along_one_axis() -> None:
    table = Table2D([0.0, 1.0], [0.0, 1.0], [[0.0, 10.0], [0.0, 10.0]])
    samples = [table.lookup(x, 0.5) for x in np.linspace(0.0, 1.0, 11)]
    np.testing.assert_allclose(np.diff(samples), 1.0)


def test_clamps_outside_the_axes(table: Table2D) -> None:
    assert table.lookup(-100.0, 150.0) == pytest.approx(table.lookup(0.0, 150.0))
    assert table.lookup(1.0e6, 150.0) == pytest.approx(table.lookup(30.0, 150.0))
    assert table.lookup(5.0, -1.0) == pytest.approx(table.lookup(5.0, 100.0))
    assert table.lookup(5.0, 1.0e6) == pytest.approx(table.lookup(5.0, 200.0))


def test_continuous_across_bin_borders(table: Table2D) -> None:
    epsilon = 1.0e-9
    left = table.lookup(10.0 - epsilon, 150.0)
    right = table.lookup(10.0 + epsilon, 150.0)
    assert left == pytest.approx(right, abs=1.0e-6)


@pytest.mark.parametrize(
    ("x_bins", "y_bins", "values", "message"),
    [
        ([0.0, 0.0], [0.0, 1.0], [[1, 2], [3, 4]], "strictly increasing"),
        ([1.0, 0.0], [0.0, 1.0], [[1, 2], [3, 4]], "strictly increasing"),
        ([0.0], [0.0, 1.0], [[1], [3]], "at least 2"),
        ([0.0, 1.0], [0.0, 1.0], [[1, 2, 3], [3, 4, 5]], "shape"),
        ([0.0, 1.0], [0.0, 1.0], [[1, float("nan")], [3, 4]], "finite"),
    ],
)
def test_rejects_invalid_tables(
    x_bins: list[float], y_bins: list[float], values: list[list[float]], message: str
) -> None:
    with pytest.raises(ValueError, match=message):
        Table2D(x_bins, y_bins, values)


def test_tables_are_immutable(table: Table2D) -> None:
    with pytest.raises(ValueError, match="read-only"):
        table.values[0, 0] = 99.0


def test_variants_do_not_change_the_original(table: Table2D) -> None:
    assert table.with_offset(1.0).lookup(5.0, 150.0) == pytest.approx(table.lookup(5.0, 150.0) + 1)
    assert table.scaled(2.0).lookup(5.0, 150.0) == pytest.approx(2.0 * table.lookup(5.0, 150.0))
    assert table.filled(0.85).lookup(17.0, 133.0) == pytest.approx(0.85)
    assert table.lookup(0.0, 100.0) == pytest.approx(VALUES[0][0])


def test_json_round_trip_and_unit_conversion(tmp_path: Path) -> None:
    raw = {
        "id": "ve",
        "name": "Volumetric Efficiency",
        "unit": "%",
        "xAxis": {"name": "rpm", "unit": "1/min", "bins": [1000, 2000]},
        "yAxis": {"name": "map", "unit": "kPa", "bins": [50, 100]},
        "min": 0,
        "max": 150,
        "values": [[80.0, 90.0], [95.0, 100.0]],
    }
    path = tmp_path / "ve.json"
    write_table_json(path, raw)
    assert read_table_json(path) == raw
    json.loads(path.read_text(encoding="utf-8"))  # valid JSON

    table = load_table_si(path)
    np.testing.assert_allclose(table.x_bins, units.rpm_to_rad_s(np.array([1000.0, 2000.0])))
    np.testing.assert_allclose(table.y_bins, [50_000.0, 100_000.0])
    assert table.lookup(units.rpm_to_rad_s(2000.0), 100_000.0) == pytest.approx(1.0)


def test_unknown_unit_is_rejected(tmp_path: Path) -> None:
    raw = {
        "id": "x",
        "unit": "furlong",
        "xAxis": {"unit": "1/min", "bins": [1, 2]},
        "yAxis": {"unit": "kPa", "bins": [1, 2]},
        "values": [[1, 2], [3, 4]],
    }
    path = tmp_path / "x.json"
    write_table_json(path, raw)
    with pytest.raises(ValueError, match="unknown unit"):
        load_table_si(path)


@pytest.mark.parametrize("table_id", ["ve", "lambdaTarget", "ignition", "ignitionMbt"])
def test_shipped_tables_are_16_by_16_and_within_limits(table_id: str) -> None:
    raw = read_table_json(SHARED_DIR / "tables" / "i6_30_na" / f"{table_id}.json")
    values = np.array(raw["values"])
    assert values.shape == (16, 16)
    assert values.min() >= raw["min"]
    assert values.max() <= raw["max"]
