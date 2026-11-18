"""Generates the start tables for the reference engine i6_30_na (16 x 16, docs/plan.md §5.1).

The shapes are plausible estimates, not measurements (research.md §2.2, §2.7, §3.2):
- VE: rises to a broad maximum around 4500-6000 1/min (intake tuning), lower at part load
  because of residual gas (exhaust flows back during valve overlap at high vacuum).
- Lambda target: 1.0 at part load, enriched towards full load (charge cooling, max torque).
- Ignition MBT: more advance at part load and high speed (slower burn in crank angle terms).
- Ignition (stock tune): MBT minus a knock margin at high load and low speed.
  Around idle it is retarded far from MBT (torque reserve, as real ECUs do).

Run from lab/:  .venv/bin/python scripts/generate_i6_30_na_tables.py
The output files are normal ECU tables; after generating they may be edited by hand.
"""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import numpy as np

from tuning_lab.table import write_table_json

TABLE_DIR = Path(__file__).resolve().parents[2] / "shared" / "tables" / "i6_30_na"

RPM_BINS = [800, 1100, 1400, 1800, 2200, 2600, 3000, 3500, 4000, 4500, 5000, 5500, 6000,
            6500, 7000, 7500]  # fmt: skip
MAP_BINS_KPA = [20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 90, 100, 105]  # fmt: skip

FULL_LOAD_KPA = 100.0

# Full-load VE over speed (fraction), interpolation points.
VE_WOT_RPM = [800, 1500, 2500, 3500, 4500, 5500, 6500, 7500]
VE_WOT = [0.80, 0.87, 0.95, 1.00, 1.04, 1.05, 1.03, 0.94]
# At high vacuum exhaust gas flows back into the intake during valve overlap (residual gas),
# so VE drops at low manifold pressure. Linear in MAP from this factor at 0 kPa to 1 at 100 kPa.
VE_PART_LOAD_FACTOR_AT_ZERO_MAP = 0.50

LAMBDA_PART_LOAD = 1.00
LAMBDA_FULL_LOAD = 0.88
LAMBDA_ENRICH_START_KPA = 70.0
LAMBDA_ENRICH_FULL_KPA = 95.0

# MBT at full load over speed [deg BTDC] plus extra advance at low load.
MBT_WOT_RPM = [800, 1500, 2500, 3500, 4500, 5500, 6500, 7500]
MBT_WOT_DEG = [12.0, 18.0, 24.0, 28.0, 31.0, 33.0, 34.0, 35.0]
MBT_EXTRA_AT_ZERO_LOAD_DEG = 18.0

# Knock margin of the stock tune at full load [deg], ramps in above 70 kPa.
KNOCK_MARGIN_RPM = [800, 1500, 2500, 3500, 4500, 5500, 6500, 7500]
KNOCK_MARGIN_DEG = [2.0, 4.0, 4.0, 3.0, 2.0, 1.5, 1.0, 1.0]
KNOCK_MARGIN_START_KPA = 70.0

# Idle torque reserve: real ECUs idle with ignition well retarded from MBT, so that they can
# add torque instantly (advance) when a load comes on. Blends in below these speed/load limits.
IDLE_IGNITION_DEG = 4.0
IDLE_REGION_FULL_RPM = 900.0
IDLE_REGION_END_RPM = 1500.0
IDLE_REGION_FULL_KPA = 35.0
IDLE_REGION_END_KPA = 50.0

IGNITION_STEP_DEG = 0.5  # typical ECU resolution


def ve_fraction(rpm: float, map_kpa: float) -> float:
    full_load = float(np.interp(rpm, VE_WOT_RPM, VE_WOT))
    load = map_kpa / FULL_LOAD_KPA
    zero_map_factor = VE_PART_LOAD_FACTOR_AT_ZERO_MAP
    part_load_factor = zero_map_factor + (1.0 - zero_map_factor) * load
    return full_load * part_load_factor


def lambda_target(_rpm: float, map_kpa: float) -> float:
    blend = np.clip(
        (map_kpa - LAMBDA_ENRICH_START_KPA) / (LAMBDA_ENRICH_FULL_KPA - LAMBDA_ENRICH_START_KPA),
        0.0,
        1.0,
    )
    return float(LAMBDA_PART_LOAD + blend * (LAMBDA_FULL_LOAD - LAMBDA_PART_LOAD))


def ignition_mbt_deg(rpm: float, map_kpa: float) -> float:
    full_load = float(np.interp(rpm, MBT_WOT_RPM, MBT_WOT_DEG))
    unload = 1.0 - min(map_kpa, FULL_LOAD_KPA) / FULL_LOAD_KPA
    return full_load + MBT_EXTRA_AT_ZERO_LOAD_DEG * unload


def ignition_stock_deg(rpm: float, map_kpa: float) -> float:
    margin = float(np.interp(rpm, KNOCK_MARGIN_RPM, KNOCK_MARGIN_DEG))
    load_share = np.clip(
        (map_kpa - KNOCK_MARGIN_START_KPA) / (FULL_LOAD_KPA - KNOCK_MARGIN_START_KPA), 0.0, 1.0
    )
    knock_limited = ignition_mbt_deg(rpm, map_kpa) - margin * float(load_share)
    idle_weight = idle_region_weight(rpm, map_kpa)
    return knock_limited + idle_weight * (IDLE_IGNITION_DEG - knock_limited)


def idle_region_weight(rpm: float, map_kpa: float) -> float:
    """1 inside the idle region, 0 outside, linear blend in between (speed and load)."""
    speed_weight = np.clip(
        (IDLE_REGION_END_RPM - rpm) / (IDLE_REGION_END_RPM - IDLE_REGION_FULL_RPM), 0.0, 1.0
    )
    load_weight = np.clip(
        (IDLE_REGION_END_KPA - map_kpa) / (IDLE_REGION_END_KPA - IDLE_REGION_FULL_KPA), 0.0, 1.0
    )
    return float(speed_weight * load_weight)


def round_to(value: float, step: float) -> float:
    return round(value / step) * step


def build_table(
    table_id: str,
    name: str,
    unit: str,
    value_min: float,
    value_max: float,
    cell: Callable[[float, float], float],
) -> dict[str, Any]:
    return {
        "id": table_id,
        "name": name,
        "unit": unit,
        "xAxis": {"name": "rpm", "unit": "1/min", "bins": RPM_BINS},
        "yAxis": {"name": "map", "unit": "kPa", "bins": MAP_BINS_KPA},
        "min": value_min,
        "max": value_max,
        "values": [[cell(rpm, map_kpa) for rpm in RPM_BINS] for map_kpa in MAP_BINS_KPA],
    }


def main() -> None:
    tables = [
        build_table(
            "ve",
            "Volumetric Efficiency",
            "%",
            0,
            150,
            lambda rpm, p: round(100.0 * ve_fraction(rpm, p), 1),
        ),
        build_table(
            "lambdaTarget",
            "Lambda Target",
            "lambda",
            0.6,
            1.5,
            lambda rpm, p: round(lambda_target(rpm, p), 3),
        ),
        build_table(
            "ignition",
            "Ignition Advance",
            "deg BTDC",
            -10,
            50,
            lambda rpm, p: round_to(ignition_stock_deg(rpm, p), IGNITION_STEP_DEG),
        ),
        build_table(
            "ignitionMbt",
            "Ignition Advance for Best Torque (engine model, not ECU)",
            "deg BTDC",
            -10,
            60,
            lambda rpm, p: round(ignition_mbt_deg(rpm, p), 1),
        ),
    ]
    for table in tables:
        path = TABLE_DIR / f"{table['id']}.json"
        write_table_json(path, table)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
