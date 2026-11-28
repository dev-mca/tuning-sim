"""Engine-level results of the 0D model: IMEP, torque, VE, EGT and the exhaust pulses.

All cylinders of an evenly firing engine with constant-pressure intake and exhaust run the
same cycle, only shifted by their firing angle (research.md §1.2). So one cylinder is
simulated and the others are copies shifted by `firingAngles_deg` of the engine JSON.
"""

import math
from dataclasses import dataclass

import numpy as np

from tuning_lab.constants import GAS_CONSTANT_AIR_J_KG_K, RADIANS_PER_CYCLE_4_STROKE
from tuning_lab.mvm import mean_effective_pressure_to_torque_nm
from tuning_lab.units import FloatArray
from tuning_lab.zero_d.combustion import progress_at_fraction
from tuning_lab.zero_d.cylinder import (
    CycleResult,
    SolverSettings,
    ZeroDOperatingPoint,
    simulate_cycle,
)
from tuning_lab.zero_d.kinematics import CylinderKinematics
from tuning_lab.zero_d.parameters import ZeroDEngine
from tuning_lab.zero_d.valves import DEG_PER_CYCLE

EXHAUST_PULSE_RESOLUTION_DEG = 1.0
"""Grid of the engine-level exhaust mass flow over 0..720 deg (resampled from the trace)."""


@dataclass(frozen=True)
class ZeroDEngineResult:
    """One operating point of the whole engine (SI units)."""

    cylinder: CycleResult
    """The simulated cylinder (cylinder 1); the others are shifted copies."""
    swept_volume_m3: float
    """Per cylinder."""
    imep_net_pa: float
    """Net indicated mean effective pressure, all 720 deg (research.md §7.5)."""
    imep_gross_pa: float
    """Compression and expansion only; comparable with the MVM's indicated torque."""
    pmep_pa: float
    """Pumping mean effective pressure = gross - net (research.md §2.8: loss if positive)."""
    indicated_torque_net_nm: float
    indicated_torque_gross_nm: float
    friction_torque_nm: float
    """From the MVM's friction correlation of the engine JSON (research.md §2.8)."""
    accessory_torque_nm: float
    brake_torque_nm: float
    """T_e = T_i,net - T_f - T_aux (research.md §2.9); pumping is already in T_i,net."""
    air_mass_per_cycle_kg: float
    """Inducted air of the whole engine per cycle, comparable with the MVM's speed density."""
    trapped_air_per_cycle_kg: float
    volumetric_efficiency: float
    """Inducted air / (rho_m V_d), the same definition as the VE table (research.md §2.2)."""
    air_mass_flow_kg_s: float
    exhaust_temperature_k: float
    peak_pressure_pa: float
    peak_pressure_angle_deg: float
    """deg ATDC of the firing TDC."""
    ca50_deg: float
    """Crank angle of 50 % burned, deg ATDC."""
    exhaust_angle_deg: FloatArray
    """0..720 deg crank angle of cylinder 1 (0 = its firing TDC), EXHAUST_PULSE_RESOLUTION_DEG."""
    exhaust_mass_flow_by_cylinder_kg_s: FloatArray
    """Shape (cylinders, angles): mass flow out of each cylinder's exhaust valves.
    This is the source signal for the waveguide in a later sound coupling (research.md §7.5)."""

    @property
    def total_exhaust_mass_flow_kg_s(self) -> FloatArray:
        return self.exhaust_mass_flow_by_cylinder_kg_s.sum(axis=0)


def run_operating_point(
    engine: ZeroDEngine,
    point: ZeroDOperatingPoint,
    settings: SolverSettings | None = None,
) -> ZeroDEngineResult:
    definition = engine.definition
    cylinders = definition.cylinders
    kinematics = CylinderKinematics(engine.geometry)
    swept = kinematics.swept_volume_m3
    displacement = cylinders * swept

    result = simulate_cycle(engine, point, settings)
    energy = result.energy
    # IMEP = W / V_h; engine torque T = imep V_d / (4 pi) (research.md §2.8, §7.5).
    imep_net = energy.net_work_j / swept
    imep_gross = energy.gross_work_j / swept
    torque_net = mean_effective_pressure_to_torque_nm(imep_net, displacement)
    torque_gross = mean_effective_pressure_to_torque_nm(imep_gross, displacement)
    friction = mean_effective_pressure_to_torque_nm(
        definition.friction.fmep_pa(point.speed_rad_s), displacement
    )
    brake = torque_net - friction - definition.accessory_torque_nm

    air_per_cycle = cylinders * result.inducted_air_kg
    manifold_density = point.intake_pressure_pa / (
        GAS_CONSTANT_AIR_J_KG_K * point.intake_temperature_k
    )
    trace = result.trace
    peak_index = int(np.argmax(trace.pressure_pa))
    angles, flows = _exhaust_pulses(engine, result)
    return ZeroDEngineResult(
        cylinder=result,
        swept_volume_m3=swept,
        imep_net_pa=imep_net,
        imep_gross_pa=imep_gross,
        pmep_pa=imep_gross - imep_net,
        indicated_torque_net_nm=torque_net,
        indicated_torque_gross_nm=torque_gross,
        friction_torque_nm=friction,
        accessory_torque_nm=definition.accessory_torque_nm,
        brake_torque_nm=brake,
        air_mass_per_cycle_kg=air_per_cycle,
        trapped_air_per_cycle_kg=cylinders * result.trapped_air_kg,
        volumetric_efficiency=air_per_cycle / (manifold_density * displacement),
        air_mass_flow_kg_s=air_per_cycle * point.speed_rad_s / RADIANS_PER_CYCLE_4_STROKE,
        exhaust_temperature_k=result.exhaust_temperature_k,
        peak_pressure_pa=float(trace.pressure_pa[peak_index]),
        peak_pressure_angle_deg=float(trace.crank_angle_deg[peak_index]),
        ca50_deg=_ca50_deg(engine, result),
        exhaust_angle_deg=angles,
        exhaust_mass_flow_by_cylinder_kg_s=flows,
    )


def _ca50_deg(engine: ZeroDEngine, result: CycleResult) -> float:
    """theta_50 = theta_0 + p50 * delta_theta_b of the Wiebe function (research.md §7.2)."""
    if not result.point.fired:
        return math.nan
    p50 = progress_at_fraction(0.5, engine.combustion)
    return result.burn_start_deg + p50 * result.burn_duration_deg


def _exhaust_pulses(engine: ZeroDEngine, result: CycleResult) -> tuple[FloatArray, FloatArray]:
    """Exhaust mass flow of every cylinder over the crank angle of cylinder 1.

    Cylinder k fires firingAngles_deg[k] later, so its flow at theta is cylinder 1's flow at
    theta - phi_k (research.md §1.2). The trace is periodic over 720 deg.
    """
    trace = result.trace
    period_angles = trace.crank_angle_deg[:-1] % DEG_PER_CYCLE
    order = np.argsort(period_angles)
    base_angles = period_angles[order]
    base_flow = trace.exhaust_mass_flow_kg_s[:-1][order]
    grid = np.arange(0.0, DEG_PER_CYCLE, EXHAUST_PULSE_RESOLUTION_DEG)
    flows = np.array(
        [
            np.interp((grid - phase) % DEG_PER_CYCLE, base_angles, base_flow, period=DEG_PER_CYCLE)
            for phase in engine.definition.firing_angles_deg
        ]
    )
    return grid, flows
