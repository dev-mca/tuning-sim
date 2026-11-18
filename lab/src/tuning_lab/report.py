"""Dyno plots for the reference engine (docs/plan.md iteration 2).

Run from lab/:  .venv/bin/python -m tuning_lab.report
Writes PNG files to lab/output/ (not committed, regenerate any time):
- dyno_stock.png           torque and power over speed, stock tune
- ignition_comparison.png  stock vs ignition -5 deg vs +3 deg (torque, power, EGT)
- lambda_comparison.png    lambda 0.85 vs 1.0 vs stock, plus a VE table that is 10 % too low
"""

from dataclasses import dataclass, replace
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # render to files, no window needed

import matplotlib.pyplot as plt
from matplotlib.axes import Axes

from tuning_lab import units
from tuning_lab.dyno import SweepResult, run_sweep
from tuning_lab.ecu import Tune
from tuning_lab.engine_definition import EngineDefinition, load_engine
from tuning_lab.paths import LAB_OUTPUT_DIR, engine_path

SWEEP_FROM_RPM = 1000.0
SWEEP_TO_RPM = 7000.0
FIGURE_SIZE_IN = (11.0, 7.0)
DPI = 120


@dataclass(frozen=True)
class Variant:
    label: str
    tune: Tune
    style: str


def stock_tune(definition: EngineDefinition) -> Tune:
    return Tune.from_tables(definition.tables)


def sweep(definition: EngineDefinition, tune: Tune) -> SweepResult:
    return run_sweep(definition, SWEEP_FROM_RPM, SWEEP_TO_RPM, tune=tune)


def speed_rpm(result: SweepResult) -> list[float]:
    return list(units.rad_s_to_rpm(result.speed_rad_s))


def plot_stock(definition: EngineDefinition, output_dir: Path) -> Path:
    result = sweep(definition, stock_tune(definition))
    rpm = speed_rpm(result)
    figure, torque_axis = plt.subplots(figsize=FIGURE_SIZE_IN)
    power_axis = torque_axis.twinx()  # second y-axis sharing the speed axis
    torque_axis.plot(rpm, result.measured_torque_nm, color="tab:blue", label="torque")
    power_axis.plot(rpm, units.w_to_kw(result.power_w), color="tab:red", label="power")
    torque_axis.set_xlabel("engine speed [1/min]")
    torque_axis.set_ylabel("torque [Nm]", color="tab:blue")
    power_axis.set_ylabel("power [kW]", color="tab:red")
    peak_torque = float(result.measured_torque_nm.max())
    peak_power_w = float(result.power_w.max())
    torque_axis.set_title(
        f"{definition.name}\nstock tune: peak torque {peak_torque:.0f} Nm, "
        f"peak power {units.w_to_kw(peak_power_w):.0f} kW "
        f"({units.w_to_ps(peak_power_w):.0f} PS)"
    )
    torque_axis.grid(alpha=0.3)
    return save(figure, output_dir / "dyno_stock.png")


def plot_comparison(
    definition: EngineDefinition,
    variants: list[Variant],
    title: str,
    file_name: str,
    output_dir: Path,
) -> Path:
    figure, axes = plt.subplots(3, 1, figsize=(FIGURE_SIZE_IN[0], 10.0), sharex=True)
    torque_axis, power_axis, third_axis = axes
    for variant in variants:
        result = sweep(definition, variant.tune)
        rpm = speed_rpm(result)
        torque_axis.plot(rpm, result.measured_torque_nm, variant.style, label=variant.label)
        power_axis.plot(rpm, units.w_to_kw(result.power_w), variant.style, label=variant.label)
        third_axis.plot(rpm, list(units.kelvin_to_celsius(result.exhaust_temperature_k)),
                        variant.style, label=variant.label)  # fmt: skip
    torque_axis.set_ylabel("torque [Nm]")
    power_axis.set_ylabel("power [kW]")
    third_axis.set_ylabel("exhaust gas temperature [°C]")
    third_axis.set_xlabel("engine speed [1/min]")
    torque_axis.set_title(title)
    decorate(axes)
    return save(figure, output_dir / file_name)


def plot_lambda(definition: EngineDefinition, output_dir: Path) -> Path:
    stock = stock_tune(definition)
    lambda_table = stock.lambda_target
    variants = [
        Variant("stock (λ 0.88 at full load)", stock, "k-"),
        Variant("λ 0.85", replace(stock, lambda_target=lambda_table.filled(0.85)), "g--"),
        Variant("λ 1.00", replace(stock, lambda_target=lambda_table.filled(1.0)), "r--"),
        Variant("ECU VE table 10 % too low", replace(stock, ve=stock.ve.scaled(0.9)), "m:"),
    ]
    figure, axes = plt.subplots(2, 1, figsize=FIGURE_SIZE_IN, sharex=True)
    torque_axis, lambda_axis = axes
    for variant in variants:
        result = sweep(definition, variant.tune)
        rpm = speed_rpm(result)
        torque_axis.plot(rpm, result.measured_torque_nm, variant.style, label=variant.label)
        lambda_axis.plot(rpm, result.lambda_actual, variant.style, label=variant.label)
    torque_axis.set_ylabel("torque [Nm]")
    lambda_axis.set_ylabel("measured lambda [-]")
    lambda_axis.set_xlabel("engine speed [1/min]")
    torque_axis.set_title(
        "Full-load lambda: rich gives the most torque; a wrong VE table shows up on the λ sensor"
    )
    decorate(axes)
    return save(figure, output_dir / "lambda_comparison.png")


def decorate(axes: list[Axes]) -> None:
    for axis in axes:
        axis.grid(alpha=0.3)
        axis.legend(loc="best")


def save(figure: plt.Figure, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.tight_layout()
    figure.savefig(path, dpi=DPI)
    plt.close(figure)
    return path


def main() -> None:
    definition = load_engine(engine_path("i6_30_na"))
    stock = stock_tune(definition)
    ignition_variants = [
        Variant("stock", stock, "k-"),
        Variant("ignition -5°", replace(stock, ignition=stock.ignition.with_offset(-5.0)), "b--"),
        Variant("ignition +3°", replace(stock, ignition=stock.ignition.with_offset(3.0)), "r--"),
    ]
    written = [
        plot_stock(definition, LAB_OUTPUT_DIR),
        plot_comparison(
            definition,
            ignition_variants,
            "Ignition timing: late ignition costs torque and makes the exhaust hotter",
            "ignition_comparison.png",
            LAB_OUTPUT_DIR,
        ),
        plot_lambda(definition, LAB_OUTPUT_DIR),
    ]
    for path in written:
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
