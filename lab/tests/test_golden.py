"""The committed golden file must match what the model produces today."""

import csv
import io

import pytest

from tuning_lab.golden import (
    COLUMNS,
    I6_30_NA_SWEEP,
    LAUNCH_COLUMNS,
    LAUNCH_FILE_NAME,
    OVERRUN_COLUMNS,
    OVERRUN_FILE_NAME,
    render_golden_csv,
    render_launch_csv,
    render_overrun_csv,
)
from tuning_lab.paths import GOLDEN_DIR

RELATIVE_TOLERANCE = 1.0e-6  # last digits may differ between platforms (libm)


def parse(text: str) -> tuple[list[str], list[list[float]]]:
    lines = [line for line in text.splitlines() if not line.startswith("#")]
    reader = csv.reader(io.StringIO("\n".join(lines)))
    header = next(reader)
    return header, [[float(value) for value in row] for row in reader]


def test_golden_file_is_up_to_date() -> None:
    committed = (GOLDEN_DIR / I6_30_NA_SWEEP.file_name).read_text(encoding="utf-8")
    header, rows = parse(committed)
    fresh_header, fresh_rows = parse(render_golden_csv(I6_30_NA_SWEEP))
    assert header == list(COLUMNS) == fresh_header
    assert len(rows) == len(fresh_rows) == 61  # 1000..7000 in 100 1/min steps
    for row, fresh in zip(rows, fresh_rows, strict=True):
        assert row == pytest.approx(fresh, rel=RELATIVE_TOLERANCE, abs=1.0e-6), (
            "golden file outdated: run `python -m tuning_lab.golden`"
        )


def test_overrun_golden_file_is_up_to_date() -> None:
    committed = (GOLDEN_DIR / OVERRUN_FILE_NAME).read_text(encoding="utf-8")
    header, rows = parse(committed)
    fresh_header, fresh_rows = parse(render_overrun_csv())
    assert header == list(OVERRUN_COLUMNS) == fresh_header
    assert len(rows) == len(fresh_rows) == 120  # 6 s, one row every 50 ms
    for row, fresh in zip(rows, fresh_rows, strict=True):
        assert row == pytest.approx(fresh, rel=RELATIVE_TOLERANCE, abs=1.0e-6), (
            "overrun golden file outdated: run `python -m tuning_lab.golden`"
        )


def test_launch_golden_file_is_up_to_date() -> None:
    committed = (GOLDEN_DIR / LAUNCH_FILE_NAME).read_text(encoding="utf-8")
    header, rows = parse(committed)
    fresh_header, fresh_rows = parse(render_launch_csv())
    assert header == list(LAUNCH_COLUMNS) == fresh_header
    assert len(rows) == len(fresh_rows) == 100  # 5 s, one row every 50 ms
    for row, fresh in zip(rows, fresh_rows, strict=True):
        assert row == pytest.approx(fresh, rel=RELATIVE_TOLERANCE, abs=1.0e-6), (
            "launch golden file outdated: run `python -m tuning_lab.golden`"
        )
