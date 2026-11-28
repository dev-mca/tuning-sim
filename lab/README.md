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

## 0D cylinder model (`src/tuning_lab/zero_d/`, iteration 11 prototype)

Cylinder-resolved model over crank angle (research.md §7): slider-crank volume (§1.4), first law
(§7.1), Wiebe heat release (§7.2), simplified Woschni wall heat (§7.3), isentropic valve flow
(§7.4). Intake and exhaust are constant-pressure reservoirs; RK4 at a fixed crank-angle step
(≤ 1°, ≤ 25 µs). Assumptions and the comparison with the MVM: `docs/adr/0010-zero-d-cylinder-model.md`.

```bash
.venv/bin/python -m tuning_lab.zero_d.report   # MVM vs 0D tables + plots -> lab/output/zero_d/
```

```python
from tuning_lab import units
from tuning_lab.zero_d.cylinder import ZeroDOperatingPoint
from tuning_lab.zero_d.engine import run_operating_point
from tuning_lab.zero_d.parameters import load_zero_d_engine

engine = load_zero_d_engine()
point = ZeroDOperatingPoint(
    units.rpm_to_rad_s(3000.0), 100_000.0, ignition_deg=22.5, lambda_value=0.88
)
result = run_operating_point(engine, point)
print(result.brake_torque_nm, result.volumetric_efficiency, result.cylinder.trace.pressure_pa.max())
```

| Module | Content |
|---|---|
| `parameters.py` | Loads conrod length and valve timing; assumed valve sizes, Wiebe and wall data |
| `kinematics.py`, `gas.py` | V(θ), dV/dθ, wall area; u(T), h(T), γ(T) |
| `valves.py`, `combustion.py`, `heat_transfer.py` | Lift and orifice flow, Wiebe, Woschni |
| `cylinder.py` | Cycle integration until periodic; p-V trace, energy and mass integrals |
| `engine.py` | IMEP, torque, VE, EGT, exhaust mass flow of all cylinders (sound source) |
| `validation.py`, `report.py` | Comparison with the MVM at the same operating points, plots |

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
