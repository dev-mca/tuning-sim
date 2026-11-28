"""Plots and the comparison table of the 0D model (docs/plan.md iteration 11).

Run from lab/:  .venv/bin/python -m tuning_lab.zero_d.report
Writes PNG files to lab/output/zero_d/ (not committed, regenerate any time) and prints the
MVM comparison as Markdown tables:
- torque_vs_mvm.png      full-load brake and indicated torque, 0D vs MVM
- air_vs_mvm.png         full-load air mass per cycle, 0D vs MVM
- p_theta.png            cylinder pressure over crank angle at a few operating points
- p_v.png                log p - log V diagram at the same points
- exhaust_flow.png       exhaust mass flow of cylinder 1 and of all six cylinders, 3000 1/min
"""

import time

import matplotlib

matplotlib.use("Agg")  # render to files, no window needed

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.axes import Axes

from tuning_lab import units
from tuning_lab.paths import LAB_OUTPUT_DIR
from tuning_lab.zero_d.engine import ZeroDEngineResult
from tuning_lab.zero_d.parameters import load_zero_d_engine
from tuning_lab.zero_d.validation import (
    ComparisonPoint,
    compare_point,
    format_table,
    full_load_comparison,
    full_load_manifold_pressure_pa,
    part_load_comparison,
)

OUTPUT_DIR = LAB_OUTPUT_DIR / "zero_d"
FIGURE_SIZE_IN = (10.0, 6.0)
DPI = 120
LINE_WIDTH = 2.0
# Categorical slots 1-3 of the dataviz reference palette (fixed order, never cycled).
COLOR_ZERO_D = "#257de0"
COLOR_MVM = "#f47230"
COLOR_THIRD = "#00b894"
FULL_LOAD_SWEEP_RPM = tuple(float(rpm) for rpm in range(1000, 7001, 500))
TRACE_POINTS_RPM_KPA = ((2000.0, 50.0), (4000.0, None), (7000.0, None))
"""None = full load (MAP from the MVM's throttle balance)."""


def _style(ax: Axes, xlabel: str, ylabel: str) -> None:
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, color="#e0e0e0", linewidth=0.8)
    ax.spines[["top", "right"]].set_visible(False)


def _plot_full_load(points: list[ComparisonPoint]) -> None:
    rpm = [p.speed_rpm for p in points]
    fig, ax = plt.subplots(figsize=FIGURE_SIZE_IN)
    ax.plot(rpm, [p.zero_d.brake_torque_nm for p in points], color=COLOR_ZERO_D,
            linewidth=LINE_WIDTH, label="0D brake")  # fmt: skip
    ax.plot(rpm, [p.mvm.brake_torque_nm for p in points], color=COLOR_MVM,
            linewidth=LINE_WIDTH, label="MVM brake")  # fmt: skip
    ax.plot(rpm, [p.zero_d.indicated_torque_gross_nm for p in points], color=COLOR_ZERO_D,
            linewidth=LINE_WIDTH, linestyle="--", label="0D indicated (gross)")  # fmt: skip
    ax.plot(rpm, [p.mvm.indicated_torque_nm for p in points], color=COLOR_MVM,
            linewidth=LINE_WIDTH, linestyle="--", label="MVM indicated")  # fmt: skip
    _style(ax, "Speed [1/min]", "Torque [Nm]")
    ax.set_title("Full load: 0D model vs mean-value model")
    ax.legend(frameon=False)
    fig.savefig(OUTPUT_DIR / "torque_vs_mvm.png", dpi=DPI, bbox_inches="tight")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=FIGURE_SIZE_IN)
    ax.plot(rpm, [1e6 * p.zero_d.air_mass_per_cycle_kg for p in points], color=COLOR_ZERO_D,
            linewidth=LINE_WIDTH, label="0D (valve flow)")  # fmt: skip
    ax.plot(rpm, [1e6 * p.mvm.air_mass_per_cycle_kg for p in points], color=COLOR_MVM,
            linewidth=LINE_WIDTH, label="MVM (VE table)")  # fmt: skip
    _style(ax, "Speed [1/min]", "Air mass per cycle, whole engine [mg]")
    ax.set_title("Full load: air mass per cycle")
    ax.legend(frameon=False)
    fig.savefig(OUTPUT_DIR / "air_vs_mvm.png", dpi=DPI, bbox_inches="tight")
    plt.close(fig)


def _plot_traces(results: list[tuple[str, ZeroDEngineResult]]) -> None:
    colors = (COLOR_ZERO_D, COLOR_MVM, COLOR_THIRD)
    fig_t, ax_t = plt.subplots(figsize=FIGURE_SIZE_IN)
    fig_v, ax_v = plt.subplots(figsize=FIGURE_SIZE_IN)
    for (label, result), color in zip(results, colors, strict=True):
        trace = result.cylinder.trace
        pressure_bar = units.pa_to_bar(trace.pressure_pa)
        ax_t.plot(trace.crank_angle_deg, pressure_bar, color=color, linewidth=LINE_WIDTH,
                  label=label)  # fmt: skip
        ax_v.loglog(1e6 * trace.volume_m3, pressure_bar, color=color, linewidth=LINE_WIDTH,
                    label=label)  # fmt: skip
    _style(ax_t, "Crank angle [deg ATDC firing]", "Cylinder pressure [bar]")
    ax_t.set_title("Cylinder pressure over crank angle (cylinder 1)")
    ax_t.legend(frameon=False)
    fig_t.savefig(OUTPUT_DIR / "p_theta.png", dpi=DPI, bbox_inches="tight")
    _style(ax_v, "Cylinder volume [cm³]", "Cylinder pressure [bar]")
    ax_v.set_title("p-V diagram, log-log (cylinder 1)")
    ax_v.legend(frameon=False)
    fig_v.savefig(OUTPUT_DIR / "p_v.png", dpi=DPI, bbox_inches="tight")
    plt.close(fig_t)
    plt.close(fig_v)


def _plot_exhaust(result: ZeroDEngineResult, label: str) -> None:
    fig, ax = plt.subplots(figsize=FIGURE_SIZE_IN)
    ax.plot(result.exhaust_angle_deg, result.total_exhaust_mass_flow_kg_s, color=COLOR_MVM,
            linewidth=LINE_WIDTH, label="all 6 cylinders")  # fmt: skip
    ax.plot(result.exhaust_angle_deg, result.exhaust_mass_flow_by_cylinder_kg_s[0],
            color=COLOR_ZERO_D, linewidth=LINE_WIDTH, label="cylinder 1")  # fmt: skip
    ax.axhline(result.air_mass_flow_kg_s, color="#9c9c9c", linewidth=1.0, linestyle=":",
               label="mean air flow")  # fmt: skip
    _style(ax, "Crank angle of cylinder 1 [deg]", "Exhaust valve mass flow [kg/s]")
    ax.set_title(f"Exhaust mass flow, the future sound source (research.md §7.5), {label}")
    ax.set_xticks(np.arange(0.0, 721.0, 120.0))
    ax.legend(frameon=False, loc="upper left", bbox_to_anchor=(1.0, 1.0))
    fig.savefig(OUTPUT_DIR / "exhaust_flow.png", dpi=DPI, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    engine = load_zero_d_engine()
    definition = engine.definition
    started = time.perf_counter()
    full = full_load_comparison(engine, FULL_LOAD_SWEEP_RPM)
    part = part_load_comparison(engine)
    elapsed = time.perf_counter() - started
    points = len(full) + len(part)
    print(f"{points} operating points in {elapsed:.1f} s ({elapsed / points:.2f} s per point)")
    print("\nFull load\n" + format_table(full))
    print("\nPart load\n" + format_table(part))
    _plot_full_load(full)

    traces: list[tuple[str, ZeroDEngineResult]] = []
    for rpm, kpa in TRACE_POINTS_RPM_KPA:
        speed = units.rpm_to_rad_s(rpm)
        manifold = (
            units.kpa_to_pa(kpa)
            if kpa is not None
            else full_load_manifold_pressure_pa(definition, speed)
        )
        label = f"{rpm:.0f} 1/min, {units.pa_to_kpa(manifold):.0f} kPa"
        traces.append((label, compare_point(engine, rpm, manifold).zero_d))
    _plot_traces(traces)
    exhaust_point = compare_point(engine, 3000.0, full_load_manifold_pressure_pa(
        definition, units.rpm_to_rad_s(3000.0)))  # fmt: skip
    _plot_exhaust(exhaust_point.zero_d, "3000 1/min full load")
    print(f"\nPlots written to {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
