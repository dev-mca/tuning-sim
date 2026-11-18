# tuning-lab

Python calculation lab for tuning-sim: reference implementations of the physics
(see `../docs/research.md`), plots, and golden files in `../shared/golden/` that the
C++ tests in `core/` compare against. Not a second simulator.

```bash
cd lab
python3 -m venv .venv
.venv/bin/pip install -e ".[dev]"
.venv/bin/pytest
.venv/bin/ruff check . && .venv/bin/ruff format --check .

.venv/bin/python -m tuning_lab.report      # dyno plots -> lab/output/*.png
.venv/bin/python -m tuning_lab.golden      # regenerate shared/golden/i6_30_na_sweep.csv
.venv/bin/python scripts/generate_i6_30_na_tables.py   # regenerate the start tables
```

## Modules (`src/tuning_lab/`)

| Module | Content | research.md |
|---|---|---|
| `units.py` | Unit conversions at the edges (rpm, kPa, bar, kW, PS, °C) | §0 |
| `constants.py` | Gas constant, gamma, AFR, heating value, ambient | §0 |
| `table.py` | `Table2D`: bilinear interpolation, clamping, JSON I/O, SI conversion | §2.3 |
| `engine_definition.py` | Loads `shared/engines/<id>.json` into SI dataclasses | – |
| `intake.py` | Throttle as compressible orifice, manifold filling model | §2.5 |
| `mvm.py` | Engine model: air, fuel, lambda, torque, losses, exhaust temperature | §2.1–2.10 |
| `ecu.py` | Speed-density ECU (`Tune` = ve, lambdaTarget, ignition), rev limiter | §2.4, §3 |
| `simulation.py` | Fixed 1 ms step: ECU → manifold (10 substeps) → model → crank | §2.11, §9 |
| `dyno.py` | PI speed controller, `run_hold`, `run_sweep` with inertia correction | §2.12 |
| `golden.py`, `report.py` | Golden CSV and plots | – |

## Units

SI inside (m, kg, s, Pa, K, rad/s). Exception: crank angles stay in degrees (`_deg`),
as in all ignition formulas of research.md. Tables in `shared/tables/` are stored in
user-facing units (1/min, kPa, %, deg BTDC) and converted when loaded.

## Experiments

A `Tune` is immutable; variants are made with `dataclasses.replace`:

```python
from dataclasses import replace
from tuning_lab.dyno import run_sweep
from tuning_lab.ecu import Tune
from tuning_lab.engine_definition import load_engine
from tuning_lab.paths import engine_path

engine = load_engine(engine_path("i6_30_na"))
stock = Tune.from_tables(engine.tables)
late = replace(stock, ignition=stock.ignition.with_offset(-5.0))
result = run_sweep(engine, 1000, 7000, tune=late)
print(result.measured_torque_nm.max())
```
