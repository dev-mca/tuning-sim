"""One cylinder over crank angle: first law, Wiebe, Woschni, valve flow (docs/research.md §7).

State per cylinder: total mass m, internal energy U = m u(T) and the mass of fresh mixture
(air + fuel vapour, port injection) still in the cylinder. Integrated over crank angle with
the classic fourth-order Runge-Kutta method at a fixed step (research.md §9: RK4 for the 0D
model). Fixed steps keep the result deterministic and make the C++ port straightforward.

One integrated cycle runs from intake valve closing (IVC) to the next IVC, so the trapped
charge and therefore the fuel mass are known before combustion starts. Cycles are repeated
until the trapped mass, the exhaust temperature and the exhaust pressure no longer change
(the residual gas couples one cycle to the next).

Assumptions [assumption] (see also docs/adr/0010):
- Intake and exhaust are reservoirs at constant pressure and temperature (no pulsation).
- Gas flowing back into the intake waits in the port (a per-cylinder buffer without volume
  or heat loss) and returns first; then fresh mixture at the manifold temperature follows.
  Gas flowing back from the exhaust is burned gas at the cycle's mean exhaust temperature.
- One gas for all species (gas.py); fuel enters as vapour with the intake mixture.
"""

import itertools
import math
from collections.abc import Sequence
from dataclasses import dataclass

import numpy as np

from tuning_lab.constants import (
    AFR_STOICH_GASOLINE,
    AMBIENT_PRESSURE_PA,
    AMBIENT_TEMPERATURE_K,
    GAS_CONSTANT_AIR_J_KG_K,
    LOWER_HEATING_VALUE_GASOLINE_J_KG,
    RADIANS_PER_CYCLE_4_STROKE,
)
from tuning_lab.units import FloatArray
from tuning_lab.zero_d import gas
from tuning_lab.zero_d.combustion import burn_duration_deg, burned_fraction
from tuning_lab.zero_d.heat_transfer import woschni_coefficient_w_m2_k
from tuning_lab.zero_d.kinematics import RAD_PER_DEG, CylinderKinematics
from tuning_lab.zero_d.parameters import ZeroDEngine
from tuning_lab.zero_d.valves import (
    DEG_PER_CYCLE,
    orifice_mass_flow_kg_s,
    valve_flow_area_m2,
    valve_lift_m,
)

R = GAS_CONSTANT_AIR_J_KG_K

HIGH_PRESSURE_LOOP_START_DEG = 540.0
"""BDC before firing TDC (= -180 deg). Compression + expansion run from here to 180 deg."""
HIGH_PRESSURE_LOOP_END_DEG = 180.0
"""BDC after firing TDC. Work in this window is the gross work (research.md §2.8, §7.5)."""


@dataclass(frozen=True)
class ZeroDOperatingPoint:
    """Inputs of one steady operating point of the 0D model (SI units)."""

    speed_rad_s: float
    intake_pressure_pa: float
    """Manifold pressure (MAP) in front of the intake valves."""
    ignition_deg: float
    """Spark advance in deg BTDC (as in the ignition table)."""
    lambda_value: float = 1.0
    """Air-fuel ratio of the inducted mixture relative to stoichiometric."""
    intake_temperature_k: float = AMBIENT_TEMPERATURE_K
    exhaust_pressure_pa: float | None = None
    """None: ambient + backpressure from the engine JSON, p = p_amb + k mdot^2 (as the MVM)."""
    fired: bool = True
    """False: motoring, nothing burns (for tests and the polytropic exponent)."""


@dataclass(frozen=True)
class SolverSettings:
    """Numerics (research.md §9). Steps are fixed in crank angle for one operating point."""

    max_step_deg: float = 1.0
    max_time_step_s: float = 2.5e-5
    """Also bounds the step in time: the valve flow near equal pressures is stiff, and at low
    speed one degree lasts long. 25 us gives 1.05 deg at the 7000 1/min rev limit."""
    max_cycles: int = 40
    relative_tolerance: float = 1.0e-6
    initial_exhaust_temperature_k: float = 1000.0


@dataclass(frozen=True)
class CycleTrace:
    """Values at the step nodes of the last cycle, from IVC to the next IVC.

    crank_angle_deg is relative to this cylinder's firing TDC and runs from IVC - 720 to IVC,
    so firing TDC is at 0 and the trace contains the whole closed phase without a wrap.
    Mass flows are positive into the cylinder (intake) and out of the cylinder (exhaust).
    """

    crank_angle_deg: FloatArray
    volume_m3: FloatArray
    pressure_pa: FloatArray
    temperature_k: FloatArray
    mass_kg: FloatArray
    burned_fraction: FloatArray
    intake_mass_flow_kg_s: FloatArray
    exhaust_mass_flow_kg_s: FloatArray


@dataclass(frozen=True)
class CycleEnergy:
    """Integrals over one cycle of one cylinder [J], first law (research.md §7.1)."""

    net_work_j: float
    """Closed integral of p dV over 720 deg (indicated work incl. gas exchange)."""
    gross_work_j: float
    """p dV over compression and expansion only (BDC to BDC around firing TDC)."""
    heat_release_j: float
    wall_heat_j: float
    intake_enthalpy_j: float
    """Net enthalpy entering through the intake valves."""
    exhaust_enthalpy_j: float
    """Net enthalpy leaving through the exhaust valves."""
    internal_energy_change_j: float

    @property
    def balance_residual_j(self) -> float:
        """dU - (Q_comb - Q_wall - W + H_in - H_out); zero for exact bookkeeping (§7.1)."""
        return self.internal_energy_change_j - (
            self.heat_release_j
            - self.wall_heat_j
            - self.net_work_j
            + self.intake_enthalpy_j
            - self.exhaust_enthalpy_j
        )


@dataclass(frozen=True)
class CycleResult:
    """Converged cycle of one cylinder."""

    point: ZeroDOperatingPoint
    trace: CycleTrace
    energy: CycleEnergy
    step_deg: float
    cycles: int
    converged: bool
    trapped_mass_kg: float
    """Total mass in the cylinder at IVC (fresh mixture + residual gas)."""
    trapped_air_kg: float
    """Air in the trapped fresh mixture at IVC."""
    inducted_air_kg: float
    """Air drawn from the manifold per cycle (what an air-mass meter would see)."""
    fuel_kg: float
    """Fuel in the trapped fresh mixture (port injection: fuel arrives with the air)."""
    burned_fuel_kg: float
    """min(fuel, air / AFR_st): with lambda < 1 oxygen is missing (research.md §2.6)."""
    residual_gas_fraction: float
    exhaust_mass_kg: float
    """Net mass through the exhaust valves per cycle."""
    exhaust_temperature_k: float
    """Enthalpy-averaged temperature of the gas leaving through the exhaust valves."""
    exhaust_pressure_pa: float
    burn_start_deg: float
    burn_duration_deg: float


def steps_per_cycle(speed_rad_s: float, settings: SolverSettings) -> int:
    """Fixed number of RK4 steps per 720 deg from the angle and time limits."""
    cycle_time_s = RADIANS_PER_CYCLE_4_STROKE / speed_rad_s
    by_angle = math.ceil(DEG_PER_CYCLE / settings.max_step_deg - 1.0e-9)
    by_time = math.ceil(cycle_time_s / settings.max_time_step_s - 1.0e-9)
    return max(by_angle, by_time)


class _CycleTables:
    """Everything that depends only on crank angle, at every half step (RK4 stage angles)."""

    def __init__(
        self,
        engine: ZeroDEngine,
        kinematics: CylinderKinematics,
        start_deg: float,
        step_deg: float,
        steps: int,
        burn_start_deg: float,
        burn_duration: float,
    ) -> None:
        angles = start_deg + 0.5 * step_deg * np.arange(2 * steps + 1)
        timing = engine.valve_timing
        intake = engine.intake_valves
        exhaust = engine.exhaust_valves
        heat = engine.heat_transfer
        self.angle_deg = angles
        self.volume_m3 = kinematics.volume_m3(angles).tolist()
        self.volume_derivative_m3_deg = kinematics.volume_derivative_m3_per_deg(angles).tolist()
        self.wall_area_m2 = kinematics.wall_area_m2(angles).tolist()
        self.intake_cd_area_m2: list[float] = []
        self.exhaust_cd_area_m2: list[float] = []
        self.woschni_c1: list[float] = []
        self.closed: list[bool] = []
        for theta in angles.tolist():
            lift_in = valve_lift_m(
                theta, timing.intake_open_deg, timing.intake_close_deg, intake.max_lift_m
            )
            lift_ex = valve_lift_m(
                theta, timing.exhaust_open_deg, timing.exhaust_close_deg, exhaust.max_lift_m
            )
            area_in = intake.discharge_coefficient * valve_flow_area_m2(lift_in, intake)
            area_ex = exhaust.discharge_coefficient * valve_flow_area_m2(lift_ex, exhaust)
            self.intake_cd_area_m2.append(area_in)
            self.exhaust_cd_area_m2.append(area_ex)
            # §7.3: Woschni uses a larger C1 during gas exchange (valves open).
            open_valve = area_in > 0.0 or area_ex > 0.0
            self.closed.append(not open_valve)
            self.woschni_c1.append(heat.c1_gas_exchange if open_valve else heat.c1_closed)

        # §7.2: burn rate as the exact mean over each step, (x_b(theta_i+1) - x_b(theta_i)) / h,
        # used by all four RK4 stages. RK4 would otherwise integrate the kink of the Wiebe rate
        # at the end of combustion with an error of ~1e-4; this way the released energy is exact.
        burned = [
            burned_fraction(theta, burn_start_deg, burn_duration, engine.combustion)
            for theta in angles[::2].tolist()
        ]
        self.burn_rate_per_deg_by_step = [
            (after - before) / step_deg for before, after in itertools.pairwise(burned)
        ]


class _Boundaries:
    """Reservoir states and per-cycle constants used by the right-hand side."""

    def __init__(self) -> None:
        self.intake_pressure_pa = 0.0
        self.intake_temperature_k = 0.0
        self.intake_enthalpy_j_kg = 0.0
        self.intake_gamma = 0.0
        self.exhaust_pressure_pa = 0.0
        self.exhaust_temperature_k = 0.0
        self.exhaust_enthalpy_j_kg = 0.0
        self.exhaust_gamma = 0.0
        self.heat_release_total_j = 0.0
        self.fresh_at_ivc_kg = 0.0
        self.deg_per_s = 0.0
        self.mean_piston_speed_m_s = 0.0
        self.bore_m = 0.0
        self.wall_temperature_k = 0.0
        self.wall_heat = True
        self.woschni_combustion_term = False
        self.woschni_c2_m_s_k = 0.0
        self.swept_volume_m3 = 0.0
        self.reference_pressure_pa = 0.0
        self.reference_volume_m3 = 0.0
        self.reference_temperature_k = 0.0
        self.reference_gamma = 0.0
        self.buffer_blend_kg = 0.0


# Dynamic state (feeds back into _rhs): cylinder mass, internal energy and fresh mass, then the
# intake-port backflow buffer (mass, enthalpy, fresh mass) of this cylinder.
_MASS, _ENERGY, _FRESH, _BUFFER_MASS, _BUFFER_ENTHALPY, _BUFFER_FRESH = 0, 1, 2, 3, 4, 5
_DYNAMIC = 6
# Cycle integrals (same RK4 weights, so the bookkeeping is exact).
_WORK, _HEAT_RELEASE, _WALL_HEAT, _H_IN, _H_EX = 6, 7, 8, 9, 10
_EX_NET_MASS, _EX_OUT_MASS, _EX_OUT_ENTHALPY, _FRESH_FROM_MANIFOLD = 11, 12, 13, 14
_STATE_SIZE = 15

BUFFER_BLEND_SHARE = 0.01
"""[assumption] The port buffer's last 1 % of a cylinder charge mixes with fresh mixture."""


@dataclass(frozen=True, slots=True)
class _Gas:
    """Cylinder gas state at one RK4 stage."""

    pressure_pa: float
    temperature_k: float
    enthalpy_j_kg: float
    fresh_share: float


@dataclass(frozen=True, slots=True)
class _ValveFlow:
    """Flow through one valve group: mass [kg/s], enthalpy [W] and fresh mass [kg/s]."""

    mass_kg_s: float = 0.0
    enthalpy_w: float = 0.0
    fresh_kg_s: float = 0.0


_NO_FLOW = _ValveFlow()


def _rhs(
    k: int, y: Sequence[float], burn_rate: float, tab: _CycleTables, bc: _Boundaries
) -> tuple[list[float], float, float]:
    """Derivatives per deg crank angle at half-step index k with burn rate dx_b/dtheta.
    Returns (d/dtheta, mdot_in, mdot_ex) with the valve mass flows in kg/s for the trace."""
    m = y[_MASS]
    energy = y[_ENERGY]
    if not (m > 0.0 and energy > 0.0):
        # Explicit RK4 left its stability region (step too long for the valve flow, §9).
        raise FloatingPointError("0D cylinder state became invalid; reduce the step size")
    temperature = gas.temperature_from_internal_energy_k(energy / m)
    volume = tab.volume_m3[k]
    cylinder = _Gas(
        pressure_pa=m * R * temperature / volume,  # ideal gas
        temperature_k=temperature,
        enthalpy_j_kg=gas.enthalpy_j_kg(temperature),
        fresh_share=y[_FRESH] / m,
    )
    derivatives = [0.0] * _STATE_SIZE
    per_s = 1.0 / bc.deg_per_s  # converts rates per second into rates per deg

    intake = _intake_flow(tab.intake_cd_area_m2[k], cylinder, y, bc, derivatives, per_s)
    exhaust = _exhaust_flow(tab.exhaust_cd_area_m2[k], cylinder, bc, derivatives, per_s)

    # §7.2: dQ_comb/dtheta = Q_total * dx_b/dtheta; the fresh charge is consumed alike.
    heat_release = bc.heat_release_total_j * burn_rate
    fresh_burned = bc.fresh_at_ivc_kg * burn_rate

    # §7.3: dQ_wall/dt = h_c A_wall (T - T_wall), w = C1 v_p.
    wall_heat = 0.0
    if bc.wall_heat:
        gas_velocity = tab.woschni_c1[k] * bc.mean_piston_speed_m_s
        if bc.woschni_combustion_term and tab.closed[k]:
            gas_velocity += _woschni_combustion_velocity_m_s(cylinder.pressure_pa, volume, bc)
        h_c = woschni_coefficient_w_m2_k(bc.bore_m, cylinder.pressure_pa, temperature, gas_velocity)
        wall_heat = h_c * tab.wall_area_m2[k] * (temperature - bc.wall_temperature_k) * per_s

    # §7.1: dU = dQ_comb - dQ_wall - p dV + sum(mdot_in h_in) - sum(mdot_out h).
    work = cylinder.pressure_pa * tab.volume_derivative_m3_deg[k]
    d_enthalpy_in = intake.enthalpy_w * per_s
    d_enthalpy_ex = exhaust.enthalpy_w * per_s
    derivatives[_MASS] = (intake.mass_kg_s - exhaust.mass_kg_s) * per_s
    derivatives[_ENERGY] = heat_release - wall_heat - work + d_enthalpy_in - d_enthalpy_ex
    derivatives[_FRESH] = (intake.fresh_kg_s - exhaust.fresh_kg_s) * per_s - fresh_burned
    derivatives[_WORK] = work
    derivatives[_HEAT_RELEASE] = heat_release
    derivatives[_WALL_HEAT] = wall_heat
    derivatives[_H_IN] = d_enthalpy_in
    derivatives[_H_EX] = d_enthalpy_ex
    derivatives[_EX_NET_MASS] = exhaust.mass_kg_s * per_s
    return derivatives, intake.mass_kg_s, exhaust.mass_kg_s


def _intake_flow(
    cd_area_m2: float,
    cylinder: _Gas,
    y: Sequence[float],
    bc: _Boundaries,
    derivatives: list[float],
    per_s: float,
) -> _ValveFlow:
    """Orifice flow through the intake valves, both directions (§7.4 + §2.5), into the cylinder
    positive. Also writes the port-buffer and manifold-draw derivatives.

    [assumption] Gas pushed back into the intake port waits there (adiabatic, at manifold
    pressure) and is the first to return; only then fresh mixture follows from the manifold.
    Without this buffer the backflow during overlap would be replaced by fresh mixture, and the
    residual gas at part load would be far too small.
    """
    if cd_area_m2 <= 0.0:
        return _NO_FLOW
    if bc.intake_pressure_pa < cylinder.pressure_pa:
        backflow = orifice_mass_flow_kg_s(
            cd_area_m2,
            cylinder.pressure_pa,
            cylinder.temperature_k,
            bc.intake_pressure_pa,
            gas.gamma(cylinder.temperature_k),
        )
        derivatives[_BUFFER_MASS] = backflow * per_s
        derivatives[_BUFFER_ENTHALPY] = backflow * cylinder.enthalpy_j_kg * per_s
        derivatives[_BUFFER_FRESH] = backflow * cylinder.fresh_share * per_s
        return _ValveFlow(
            -backflow, -backflow * cylinder.enthalpy_j_kg, -backflow * cylinder.fresh_share
        )

    # Share from the port buffer fades out over its last BUFFER_BLEND_SHARE of a charge;
    # a hard switch inside an RK4 step would leave a small limit cycle.
    buffer_mass = max(y[_BUFFER_MASS], 0.0)
    from_buffer = min(1.0, buffer_mass / bc.buffer_blend_kg)
    mdot_manifold = (1.0 - from_buffer) * orifice_mass_flow_kg_s(
        cd_area_m2,
        bc.intake_pressure_pa,
        bc.intake_temperature_k,
        cylinder.pressure_pa,
        bc.intake_gamma,
    )
    derivatives[_FRESH_FROM_MANIFOLD] = mdot_manifold * per_s
    manifold = _ValveFlow(mdot_manifold, mdot_manifold * bc.intake_enthalpy_j_kg, mdot_manifold)
    if from_buffer <= 0.0:
        return manifold
    h_up = y[_BUFFER_ENTHALPY] / buffer_mass
    t_up = gas.temperature_from_enthalpy_k(h_up)
    mdot_buffer = from_buffer * orifice_mass_flow_kg_s(
        cd_area_m2, bc.intake_pressure_pa, t_up, cylinder.pressure_pa, gas.gamma(t_up)
    )
    fresh_buffer = mdot_buffer * y[_BUFFER_FRESH] / buffer_mass
    derivatives[_BUFFER_MASS] = -mdot_buffer * per_s
    derivatives[_BUFFER_ENTHALPY] = -mdot_buffer * h_up * per_s
    derivatives[_BUFFER_FRESH] = -fresh_buffer * per_s
    return _ValveFlow(
        manifold.mass_kg_s + mdot_buffer,
        manifold.enthalpy_w + mdot_buffer * h_up,
        manifold.fresh_kg_s + fresh_buffer,
    )


def _exhaust_flow(
    cd_area_m2: float, cylinder: _Gas, bc: _Boundaries, derivatives: list[float], per_s: float
) -> _ValveFlow:
    """Orifice flow through the exhaust valves (§7.4 + §2.5), out of the cylinder positive.
    Backflow brings burned gas from the exhaust reservoir."""
    if cd_area_m2 <= 0.0:
        return _NO_FLOW
    if cylinder.pressure_pa >= bc.exhaust_pressure_pa:
        mdot = orifice_mass_flow_kg_s(
            cd_area_m2,
            cylinder.pressure_pa,
            cylinder.temperature_k,
            bc.exhaust_pressure_pa,
            gas.gamma(cylinder.temperature_k),
        )
        derivatives[_EX_OUT_MASS] = mdot * per_s
        derivatives[_EX_OUT_ENTHALPY] = mdot * cylinder.enthalpy_j_kg * per_s
        return _ValveFlow(mdot, mdot * cylinder.enthalpy_j_kg, mdot * cylinder.fresh_share)
    mdot = -orifice_mass_flow_kg_s(
        cd_area_m2,
        bc.exhaust_pressure_pa,
        bc.exhaust_temperature_k,
        cylinder.pressure_pa,
        bc.exhaust_gamma,
    )
    return _ValveFlow(mdot, mdot * bc.exhaust_enthalpy_j_kg, 0.0)


def _woschni_combustion_velocity_m_s(pressure: float, volume: float, bc: _Boundaries) -> float:
    """Woschni's combustion term C2 V_d T_r / (p_r V_r) (p - p_mot) (§7.3, optional).

    Reference state r = IVC; p_mot is the motored pressure, isentropic from IVC [prüfen].
    """
    motored = bc.reference_pressure_pa * (bc.reference_volume_m3 / volume) ** bc.reference_gamma
    return max(
        0.0,
        bc.woschni_c2_m_s_k
        * bc.swept_volume_m3
        * bc.reference_temperature_k
        / (bc.reference_pressure_pa * bc.reference_volume_m3)
        * (pressure - motored),
    )


@dataclass
class _CycleOutcome:
    state: list[float]
    totals: list[float]
    gross_work_j: float
    trace: CycleTrace


def _integrate_cycle(
    initial: Sequence[float], tab: _CycleTables, bc: _Boundaries, steps: int, step_deg: float
) -> _CycleOutcome:
    """Classic RK4 over one cycle at a fixed step (research.md §9)."""
    y = [*initial[:_DYNAMIC], *([0.0] * (_STATE_SIZE - _DYNAMIC))]
    h = step_deg
    gross_work = 0.0
    burned_so_far = 0.0
    n_nodes = steps + 1
    pressure = np.empty(n_nodes)
    temperature = np.empty(n_nodes)
    mass = np.empty(n_nodes)
    mdot_in_trace = np.empty(n_nodes)
    mdot_ex_trace = np.empty(n_nodes)
    burned = np.empty(n_nodes)
    for i in range(steps):
        k = 2 * i
        burn_rate = tab.burn_rate_per_deg_by_step[i]
        k1, mdot_in, mdot_ex = _rhs(k, y, burn_rate, tab, bc)
        _record(i, y, tab, pressure, temperature, mass)
        mdot_in_trace[i] = mdot_in
        mdot_ex_trace[i] = mdot_ex
        burned[i] = burned_so_far
        k2, _, _ = _rhs(k + 1, _advance(y, k1, 0.5 * h), burn_rate, tab, bc)
        k3, _, _ = _rhs(k + 1, _advance(y, k2, 0.5 * h), burn_rate, tab, bc)
        k4, _, _ = _rhs(k + 2, _advance(y, k3, h), burn_rate, tab, bc)
        increments = [
            h / 6.0 * (a + 2.0 * b + 2.0 * c + d) for a, b, c, d in zip(k1, k2, k3, k4, strict=True)
        ]
        y = [value + inc for value, inc in zip(y, increments, strict=True)]
        # Gross work: p dV between the BDCs around firing TDC; the step is assigned by its
        # midpoint (dV/dtheta is ~0 at BDC, so the split error is negligible).
        if _in_high_pressure_loop(float(tab.angle_deg[k + 1])):
            gross_work += increments[_WORK]
        if bc.heat_release_total_j > 0.0:
            burned_so_far += increments[_HEAT_RELEASE] / bc.heat_release_total_j
    _, mdot_in, mdot_ex = _rhs(2 * steps, y, 0.0, tab, bc)
    _record(steps, y, tab, pressure, temperature, mass)
    mdot_in_trace[steps] = mdot_in
    mdot_ex_trace[steps] = mdot_ex
    burned[steps] = burned_so_far
    trace = CycleTrace(
        crank_angle_deg=tab.angle_deg[::2].copy(),
        volume_m3=np.asarray(tab.volume_m3[::2]),
        pressure_pa=pressure,
        temperature_k=temperature,
        mass_kg=mass,
        burned_fraction=burned,
        intake_mass_flow_kg_s=mdot_in_trace,
        exhaust_mass_flow_kg_s=mdot_ex_trace,
    )
    return _CycleOutcome(y[:_DYNAMIC], y[_DYNAMIC:], gross_work, trace)


def _advance(y: list[float], slope: list[float], h: float) -> list[float]:
    """Intermediate RK4 state; only the dynamic entries are needed by _rhs."""
    return [y[i] + h * slope[i] for i in range(_DYNAMIC)]


def _record(
    i: int,
    y: list[float],
    tab: _CycleTables,
    pressure: FloatArray,
    temperature: FloatArray,
    mass: FloatArray,
) -> None:
    t = gas.temperature_from_internal_energy_k(y[_ENERGY] / y[_MASS])
    temperature[i] = t
    mass[i] = y[_MASS]
    pressure[i] = y[_MASS] * R * t / tab.volume_m3[2 * i]


def _in_high_pressure_loop(theta_deg: float) -> bool:
    """True between BDC before and BDC after firing TDC (-180..180 deg)."""
    theta = theta_deg % DEG_PER_CYCLE
    return theta >= HIGH_PRESSURE_LOOP_START_DEG or theta < HIGH_PRESSURE_LOOP_END_DEG


def _air_share_of_mixture(lambda_value: float) -> float:
    """Mixture of air and fuel vapour: air / (air + fuel) = lambda AFR / (lambda AFR + 1)."""
    air_per_fuel = lambda_value * AFR_STOICH_GASOLINE
    return air_per_fuel / (air_per_fuel + 1.0)


def _burned_fuel_kg(fresh_mixture_kg: float, air_share: float) -> float:
    """m_fuel,burned = min(m_fuel, m_air / AFR_st) (research.md §2.6)."""
    air = fresh_mixture_kg * air_share
    fuel = fresh_mixture_kg - air
    return min(fuel, air / AFR_STOICH_GASOLINE)


def _boundaries(
    engine: ZeroDEngine, kinematics: CylinderKinematics, point: ZeroDOperatingPoint
) -> _Boundaries:
    heat = engine.heat_transfer
    bc = _Boundaries()
    bc.intake_pressure_pa = point.intake_pressure_pa
    bc.intake_temperature_k = point.intake_temperature_k
    bc.intake_enthalpy_j_kg = gas.enthalpy_j_kg(point.intake_temperature_k)
    bc.intake_gamma = gas.gamma(point.intake_temperature_k)
    bc.deg_per_s = point.speed_rad_s / RAD_PER_DEG
    bc.mean_piston_speed_m_s = kinematics.mean_piston_speed_m_s(point.speed_rad_s)
    bc.bore_m = engine.geometry.bore_m
    bc.wall_temperature_k = heat.wall_temperature_k
    bc.wall_heat = heat.enabled
    bc.woschni_combustion_term = heat.combustion_term
    bc.woschni_c2_m_s_k = heat.c2_m_s_k
    bc.swept_volume_m3 = kinematics.swept_volume_m3
    return bc


def simulate_cycle(
    engine: ZeroDEngine,
    point: ZeroDOperatingPoint,
    settings: SolverSettings | None = None,
) -> CycleResult:
    """Runs cycles of one cylinder until they repeat and returns the last one."""
    settings = settings if settings is not None else SolverSettings()
    definition = engine.definition
    kinematics = CylinderKinematics(engine.geometry)
    steps = steps_per_cycle(point.speed_rad_s, settings)
    step_deg = DEG_PER_CYCLE / steps
    start_deg = engine.valve_timing.intake_close_deg - DEG_PER_CYCLE

    # §7.2: the spark sets the Wiebe start (plus ignition delay); duration from MBT.
    mbt_deg = definition.tables.ignition_mbt.lookup(point.speed_rad_s, point.intake_pressure_pa)
    duration = burn_duration_deg(mbt_deg, engine.combustion)
    burn_start = -point.ignition_deg + engine.combustion.ignition_delay_deg
    tab = _CycleTables(engine, kinematics, start_deg, step_deg, steps, burn_start, duration)
    bc = _boundaries(engine, kinematics, point)
    volume_ivc = kinematics.volume_m3(start_deg)
    bc.reference_volume_m3 = volume_ivc

    air_share = _air_share_of_mixture(point.lambda_value)
    cycles_per_s = point.speed_rad_s / RADIANS_PER_CYCLE_4_STROKE
    exhaust_temperature = settings.initial_exhaust_temperature_k
    exhaust_pressure = (
        point.exhaust_pressure_pa if point.exhaust_pressure_pa is not None else AMBIENT_PRESSURE_PA
    )

    # Start at IVC with the cylinder full of fresh mixture at manifold state, port empty.
    m0 = point.intake_pressure_pa * volume_ivc / (R * point.intake_temperature_k)
    bc.buffer_blend_kg = BUFFER_BLEND_SHARE * m0
    state = [m0, m0 * gas.internal_energy_j_kg(point.intake_temperature_k), m0, 0.0, 0.0, 0.0]
    initial_state = state

    outcome: _CycleOutcome | None = None
    converged = False
    cycles = 0
    previous: tuple[float, float, float] | None = None
    while cycles < settings.max_cycles:
        cycles += 1
        bc.exhaust_pressure_pa = exhaust_pressure
        bc.exhaust_temperature_k = exhaust_temperature
        bc.exhaust_enthalpy_j_kg = gas.enthalpy_j_kg(exhaust_temperature)
        bc.exhaust_gamma = gas.gamma(exhaust_temperature)
        fresh_at_ivc = state[_FRESH]
        bc.fresh_at_ivc_kg = fresh_at_ivc if point.fired else 0.0
        bc.heat_release_total_j = (
            _burned_fuel_kg(fresh_at_ivc, air_share)
            * LOWER_HEATING_VALUE_GASOLINE_J_KG
            * engine.combustion.combustion_efficiency
            if point.fired
            else 0.0
        )
        ivc_temperature = gas.temperature_from_internal_energy_k(state[_ENERGY] / state[_MASS])
        bc.reference_temperature_k = ivc_temperature
        bc.reference_pressure_pa = state[_MASS] * R * ivc_temperature / volume_ivc
        bc.reference_gamma = gas.gamma(ivc_temperature)

        initial_state = state
        outcome = _integrate_cycle(state, tab, bc, steps, step_deg)
        state = outcome.state

        totals = outcome.totals
        out_mass = totals[_EX_OUT_MASS - _DYNAMIC]
        if out_mass > 0.0:
            exhaust_temperature = gas.temperature_from_enthalpy_k(
                totals[_EX_OUT_ENTHALPY - _DYNAMIC] / out_mass
            )
        if point.exhaust_pressure_pa is None:
            # Same backpressure law as the MVM (engine JSON), with this cycle's mass flow.
            engine_flow = definition.cylinders * totals[_EX_NET_MASS - _DYNAMIC] * cycles_per_s
            exhaust_pressure = (
                AMBIENT_PRESSURE_PA
                + definition.exhaust_backpressure_coefficient * engine_flow * engine_flow
            )
        current = (initial_state[_FRESH], exhaust_temperature, exhaust_pressure)
        if previous is not None and all(
            abs(c - p) <= settings.relative_tolerance * abs(p)
            for c, p in zip(current, previous, strict=True)
        ):
            converged = True
            break
        previous = current

    assert outcome is not None
    return _make_result(
        point,
        outcome,
        initial_state,
        bc,
        air_share,
        step_deg,
        cycles,
        converged,
        exhaust_temperature,
        burn_start,
        duration,
    )


def _make_result(
    point: ZeroDOperatingPoint,
    outcome: _CycleOutcome,
    initial_state: list[float],
    bc: _Boundaries,
    air_share: float,
    step_deg: float,
    cycles: int,
    converged: bool,
    exhaust_temperature: float,
    burn_start: float,
    duration: float,
) -> CycleResult:
    totals = outcome.totals
    trapped_mass = initial_state[_MASS]
    trapped_fresh = initial_state[_FRESH]
    energy = CycleEnergy(
        net_work_j=totals[_WORK - _DYNAMIC],
        gross_work_j=outcome.gross_work_j,
        heat_release_j=totals[_HEAT_RELEASE - _DYNAMIC],
        wall_heat_j=totals[_WALL_HEAT - _DYNAMIC],
        intake_enthalpy_j=totals[_H_IN - _DYNAMIC],
        exhaust_enthalpy_j=totals[_H_EX - _DYNAMIC],
        internal_energy_change_j=outcome.state[_ENERGY] - initial_state[_ENERGY],
    )
    return CycleResult(
        point=point,
        trace=outcome.trace,
        energy=energy,
        step_deg=step_deg,
        cycles=cycles,
        converged=converged,
        trapped_mass_kg=trapped_mass,
        trapped_air_kg=trapped_fresh * air_share,
        inducted_air_kg=totals[_FRESH_FROM_MANIFOLD - _DYNAMIC] * air_share,
        fuel_kg=trapped_fresh * (1.0 - air_share),
        burned_fuel_kg=_burned_fuel_kg(trapped_fresh, air_share) if point.fired else 0.0,
        residual_gas_fraction=1.0 - trapped_fresh / trapped_mass,
        exhaust_mass_kg=totals[_EX_NET_MASS - _DYNAMIC],
        exhaust_temperature_k=exhaust_temperature,
        exhaust_pressure_pa=bc.exhaust_pressure_pa,
        burn_start_deg=burn_start,
        burn_duration_deg=duration,
    )
