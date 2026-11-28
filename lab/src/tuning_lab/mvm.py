"""Mean-value engine model: air, fuel, torque, losses, exhaust (docs/research.md §2).

Everything is computed per engine cycle (720 deg crank angle). Mass flows follow from
"per cycle" times cycles per second = omega / (4 pi). This keeps torque defined at omega = 0.
"""

import math
from dataclasses import dataclass

from tuning_lab.constants import (
    AFR_STOICH_GASOLINE,
    AMBIENT_PRESSURE_PA,
    CP_EXHAUST_J_KG_K,
    GAS_CONSTANT_AIR_J_KG_K,
    LOWER_HEATING_VALUE_GASOLINE_J_KG,
    RADIANS_PER_CYCLE_4_STROKE,
)
from tuning_lab.engine_definition import CombustionParameters, EngineDefinition
from tuning_lab.table import Table2D


@dataclass(frozen=True)
class OperatingPoint:
    """Inputs of the engine model for one instant."""

    speed_rad_s: float
    manifold_pressure_pa: float
    manifold_temperature_k: float
    fuel_mass_per_cycle_kg: float
    ignition_deg: float
    """Ignition advance in deg BTDC."""
    spark_cut: bool = False
    """True while the ECU cuts the spark but keeps injecting (2-step, research.md §3.4): the
    mixture leaves the cylinder unburnt and can afterburn in the exhaust (§2.10a)."""


@dataclass(frozen=True)
class EngineOutputs:
    """Results of the engine model (SI units)."""

    volumetric_efficiency: float
    air_mass_per_cycle_kg: float
    air_mass_flow_kg_s: float
    fuel_mass_flow_kg_s: float
    lambda_actual: float
    """math.inf when no fuel is injected (fuel cut)."""
    ignition_mbt_deg: float
    ignition_efficiency: float
    lambda_efficiency: float
    indicated_torque_nm: float
    friction_torque_nm: float
    pumping_torque_nm: float
    accessory_torque_nm: float
    brake_torque_nm: float
    exhaust_pressure_pa: float
    exhaust_temperature_k: float
    late_combustion_fraction: float
    """Share of the burned fuel still burning at EVO (research.md §2.10a); 1 with spark cut."""
    afterburn_temperature_rise_k: float
    """Extra heat for the catalyst from fuel burning in the exhaust (research.md §2.10a)."""


def air_density_kg_m3(pressure_pa: float, temperature_k: float) -> float:
    """Ideal gas: rho = p / (R T) (research.md §2.1)."""
    return pressure_pa / (GAS_CONSTANT_AIR_J_KG_K * temperature_k)


def mean_effective_pressure_to_torque_nm(mep_pa: float, displacement_m3: float) -> float:
    """T = mep * V_d / (4 pi) for a four-stroke engine (research.md §2.8)."""
    return mep_pa * displacement_m3 / RADIANS_PER_CYCLE_4_STROKE


def torque_to_bmep_pa(torque_nm: float, displacement_m3: float) -> float:
    """bmep = 4 pi T / V_d (research.md §2.9), the main plausibility check."""
    return RADIANS_PER_CYCLE_4_STROKE * torque_nm / displacement_m3


def ignition_efficiency(
    ignition_deg: float, ignition_mbt_deg: float, combustion: CombustionParameters
) -> float:
    """eta_ign = max(0, 1 - k (theta - theta_MBT)^2) (research.md §2.7)."""
    deviation = ignition_deg - ignition_mbt_deg
    return max(0.0, 1.0 - combustion.ignition_curvature_per_deg2 * deviation * deviation)


def lambda_efficiency(lambda_value: float, combustion: CombustionParameters) -> float:
    """Smooth factor with maximum 1.0 at lambda_optimum (research.md §2.6). [prüfen]"""
    if math.isinf(lambda_value):
        return 0.0
    deviation = lambda_value - combustion.lambda_optimum
    return max(0.0, 1.0 - combustion.lambda_curvature * deviation * deviation)


class EngineModel:
    """The "real" engine. Its true VE may differ from the VE table the ECU believes in.

    research.md §2.4: if the ECU's VE table does not match the true VE, the measured lambda
    deviates from the target. That is exactly how a VE table is tuned in real life.
    """

    def __init__(self, definition: EngineDefinition, true_ve: Table2D | None = None) -> None:
        self._definition = definition
        self._true_ve = true_ve if true_ve is not None else definition.tables.ve
        self._displacement_m3 = definition.displacement_m3

    @property
    def definition(self) -> EngineDefinition:
        return self._definition

    def air_mass_per_cycle_kg(
        self, speed_rad_s: float, manifold_pressure_pa: float, manifold_temperature_k: float
    ) -> float:
        """m_air,cyc = eta_v * rho_m * V_d (research.md §2.2)."""
        ve = self._true_ve.lookup(speed_rad_s, manifold_pressure_pa)
        density = air_density_kg_m3(manifold_pressure_pa, manifold_temperature_k)
        return ve * density * self._displacement_m3

    def air_mass_flow_kg_s(
        self, speed_rad_s: float, manifold_pressure_pa: float, manifold_temperature_k: float
    ) -> float:
        """mdot_air = m_air,cyc * omega / (4 pi), equivalent to eta_v rho V_d N/120 (§2.2)."""
        per_cycle = self.air_mass_per_cycle_kg(
            speed_rad_s, manifold_pressure_pa, manifold_temperature_k
        )
        return per_cycle * max(speed_rad_s, 0.0) / RADIANS_PER_CYCLE_4_STROKE

    def evaluate(self, point: OperatingPoint) -> EngineOutputs:
        definition = self._definition
        combustion = definition.combustion
        speed = point.speed_rad_s
        cycles_per_second = max(speed, 0.0) / RADIANS_PER_CYCLE_4_STROKE

        ve = self._true_ve.lookup(speed, point.manifold_pressure_pa)
        air_per_cycle = (
            ve
            * air_density_kg_m3(point.manifold_pressure_pa, point.manifold_temperature_k)
            * self._displacement_m3
        )
        fuel_per_cycle = point.fuel_mass_per_cycle_kg

        # §2.4: actual lambda from true air mass and injected fuel.
        lambda_actual = (
            air_per_cycle / (fuel_per_cycle * AFR_STOICH_GASOLINE)
            if fuel_per_cycle > 0.0
            else math.inf
        )
        # §2.6: with lambda < 1 oxygen is missing; at most air / AFR_st can burn.
        fuel_burned = min(fuel_per_cycle, air_per_cycle / AFR_STOICH_GASOLINE)
        # §2.10a: without a spark the burnable mixture goes to the exhaust instead.
        unburnt_mixture_fuel = fuel_burned if point.spark_cut else 0.0
        if point.spark_cut:
            fuel_burned = 0.0

        mbt_deg = definition.tables.ignition_mbt.lookup(speed, point.manifold_pressure_pa)
        eta_ign = ignition_efficiency(point.ignition_deg, mbt_deg, combustion)
        eta_lambda = lambda_efficiency(lambda_actual, combustion)
        eta_indicated = combustion.indicated_efficiency_base * eta_ign * eta_lambda

        # §2.6: W_i = m_fuel,burned * H_u * eta_i ; T_i = W_i / (4 pi)
        fuel_energy_per_cycle = fuel_burned * LOWER_HEATING_VALUE_GASOLINE_J_KG
        indicated_torque = fuel_energy_per_cycle * eta_indicated / RADIANS_PER_CYCLE_4_STROKE

        # §2.8: friction from fmep correlation; pumping from p_exh - p_m.
        air_flow = air_per_cycle * cycles_per_second
        fuel_flow = fuel_per_cycle * cycles_per_second
        exhaust_flow = air_flow + fuel_flow
        exhaust_pressure = (
            AMBIENT_PRESSURE_PA
            + definition.exhaust_backpressure_coefficient * exhaust_flow * exhaust_flow
        )
        friction_torque = mean_effective_pressure_to_torque_nm(
            definition.friction.fmep_pa(speed), self._displacement_m3
        )
        pumping_torque = mean_effective_pressure_to_torque_nm(
            exhaust_pressure - point.manifold_pressure_pa, self._displacement_m3
        )
        # §2.9: T_e = T_i - T_f - T_p - T_aux
        accessory_torque = definition.accessory_torque_nm
        brake_torque = indicated_torque - friction_torque - pumping_torque - accessory_torque

        # §2.10: energy not turned into work or lost to cooling leaves with the exhaust.
        # Late ignition lowers eta_indicated, so more energy goes into the exhaust (hotter).
        exhaust_heat_per_cycle = fuel_energy_per_cycle * max(
            0.0, 1.0 - eta_indicated - combustion.cooling_fraction
        )
        exhaust_mass_per_cycle = air_per_cycle + fuel_per_cycle
        exhaust_temperature = point.manifold_temperature_k + (
            exhaust_heat_per_cycle / (exhaust_mass_per_cycle * CP_EXHAUST_J_KG_K)
            if exhaust_mass_per_cycle > 0.0
            else 0.0
        )

        # §2.10a: far behind MBT the combustion is not finished at EVO; that mixture can
        # afterburn in the exhaust and heats the catalyst on top of the exhaust temperature.
        afterburn = definition.afterburn
        if point.spark_cut:
            # All burnable mixture reaches the exhaust unburnt (2-step, research.md §3.4).
            late_fraction = 1.0
            afterburn_heat_per_cycle = (
                afterburn.afterburn_efficiency
                * unburnt_mixture_fuel
                * LOWER_HEATING_VALUE_GASOLINE_J_KG
            )
        else:
            retard_deg = mbt_deg - point.ignition_deg
            late_fraction = min(
                max(
                    (retard_deg - afterburn.late_combustion_start_deg)
                    / afterburn.late_combustion_span_deg,
                    0.0,
                ),
                afterburn.late_combustion_max,
            )
            afterburn_heat_per_cycle = (
                afterburn.afterburn_efficiency
                * late_fraction
                * fuel_burned
                * LOWER_HEATING_VALUE_GASOLINE_J_KG
            )
        afterburn_rise = (
            afterburn_heat_per_cycle / (exhaust_mass_per_cycle * CP_EXHAUST_J_KG_K)
            if exhaust_mass_per_cycle > 0.0
            else 0.0
        )

        return EngineOutputs(
            volumetric_efficiency=ve,
            air_mass_per_cycle_kg=air_per_cycle,
            air_mass_flow_kg_s=air_flow,
            fuel_mass_flow_kg_s=fuel_flow,
            lambda_actual=lambda_actual,
            ignition_mbt_deg=mbt_deg,
            ignition_efficiency=eta_ign,
            lambda_efficiency=eta_lambda,
            indicated_torque_nm=indicated_torque,
            friction_torque_nm=friction_torque,
            pumping_torque_nm=pumping_torque,
            accessory_torque_nm=accessory_torque,
            brake_torque_nm=brake_torque,
            exhaust_pressure_pa=exhaust_pressure,
            exhaust_temperature_k=exhaust_temperature,
            late_combustion_fraction=late_fraction,
            afterburn_temperature_rise_k=afterburn_rise,
        )
