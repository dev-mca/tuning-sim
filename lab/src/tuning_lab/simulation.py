"""Fixed-step engine simulation: ECU + engine model + manifold + crank dynamics.

docs/plan.md §2.3 and research.md §9: fixed time step (1 ms), deterministic.
- Manifold pressure: explicit Euler with substeps (stiff near wide-open throttle).
- Crank speed: semi-implicit Euler, J d(omega)/dt = T_e - T_load (research.md §2.11).
"""

from dataclasses import dataclass

from tuning_lab.constants import AMBIENT_PRESSURE_PA, AMBIENT_TEMPERATURE_K
from tuning_lab.ecu import Ecu, EcuCommand, Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.intake import (
    manifold_pressure_derivative_pa_s,
    orifice_mass_flow_kg_s,
    throttle_area_m2,
)
from tuning_lab.mvm import EngineModel, EngineOutputs, OperatingPoint
from tuning_lab.table import Table2D

DEFAULT_TIME_STEP_S = 1.0e-3
MANIFOLD_SUBSTEPS = 10
"""Manifold substeps per time step. Near wide-open throttle the manifold time constant drops
below 1 ms (steep throttle flow function); 10 substeps of 0.1 ms keep explicit Euler stable."""

MIN_MANIFOLD_PRESSURE_PA = 1.0e3
"""Lower bound so that the model never sees a vacuum (protects divisions)."""


@dataclass(frozen=True)
class SimulationState:
    """Snapshot after a time step (SI units)."""

    time_s: float
    speed_rad_s: float
    manifold_pressure_pa: float
    throttle: float
    load_torque_nm: float
    command: EcuCommand
    engine: EngineOutputs
    catalyst_temperature_k: float


class EngineSimulation:
    def __init__(
        self,
        definition: EngineDefinition,
        tune: Tune | None = None,
        true_ve: Table2D | None = None,
        initial_speed_rad_s: float | None = None,
        time_step_s: float = DEFAULT_TIME_STEP_S,
    ) -> None:
        tables = definition.tables
        self._definition = definition
        self._model = EngineModel(definition, true_ve)
        self._ecu = Ecu(
            tune if tune is not None else Tune.from_tables(tables),
            definition.displacement_m3,
            definition.rev_limit_rad_s,
            definition.rev_limit_hysteresis_rad_s,
            definition.overrun_cut,
        )
        self._time_step_s = time_step_s
        self._time_s = 0.0
        self._speed_rad_s = (
            initial_speed_rad_s if initial_speed_rad_s is not None else definition.idle_speed_rad_s
        )
        self._manifold_pressure_pa = AMBIENT_PRESSURE_PA
        self._manifold_temperature_k = AMBIENT_TEMPERATURE_K
        self._catalyst_temperature_k = definition.afterburn.catalyst_initial_k

    @property
    def time_step_s(self) -> float:
        return self._time_step_s

    @property
    def speed_rad_s(self) -> float:
        return self._speed_rad_s

    @property
    def definition(self) -> EngineDefinition:
        return self._definition

    def set_launch_switch(self, held: bool) -> None:
        """Launch button held (2-step, research.md §3.4)."""
        self._ecu.set_launch_switch(held)

    def step(self, throttle: float, load_torque_nm: float) -> SimulationState:
        """Advances the simulation by one time step."""
        dt = self._time_step_s
        command = self._ecu.update(
            self._speed_rad_s, self._manifold_pressure_pa, self._manifold_temperature_k, throttle
        )
        self._integrate_manifold(throttle, dt)
        outputs = self._model.evaluate(
            OperatingPoint(
                speed_rad_s=self._speed_rad_s,
                manifold_pressure_pa=self._manifold_pressure_pa,
                manifold_temperature_k=self._manifold_temperature_k,
                fuel_mass_per_cycle_kg=command.fuel_mass_per_cycle_kg,
                ignition_deg=command.ignition_deg,
                spark_cut=command.spark_cut,
            )
        )
        # Semi-implicit Euler for the crank: new speed from the current torque balance.
        acceleration = (outputs.brake_torque_nm - load_torque_nm) / self._definition.inertia_kg_m2
        self._speed_rad_s = max(0.0, self._speed_rad_s + dt * acceleration)
        self._update_catalyst(outputs, dt)
        self._time_s += dt
        return SimulationState(
            time_s=self._time_s,
            speed_rad_s=self._speed_rad_s,
            manifold_pressure_pa=self._manifold_pressure_pa,
            throttle=throttle,
            load_torque_nm=load_torque_nm,
            command=command,
            engine=outputs,
            catalyst_temperature_k=self._catalyst_temperature_k,
        )

    def _update_catalyst(self, outputs: EngineOutputs, dt: float) -> None:
        """First-order lag towards exhaust temperature plus afterburn (research.md §2.10a)."""
        target = outputs.exhaust_temperature_k + outputs.afterburn_temperature_rise_k
        time_constant = self._definition.afterburn.catalyst_time_constant_s
        self._catalyst_temperature_k = (
            self._catalyst_temperature_k
            + dt * (target - self._catalyst_temperature_k) / time_constant
        )

    def _integrate_manifold(self, throttle: float, dt: float) -> None:
        intake = self._definition.intake
        area = throttle_area_m2(throttle, intake)
        sub_dt = dt / MANIFOLD_SUBSTEPS
        pressure = self._manifold_pressure_pa
        for _ in range(MANIFOLD_SUBSTEPS):
            throttle_flow = orifice_mass_flow_kg_s(
                area,
                intake.throttle_discharge_coefficient,
                AMBIENT_PRESSURE_PA,
                AMBIENT_TEMPERATURE_K,
                pressure,
            )
            cylinder_flow = self._model.air_mass_flow_kg_s(
                self._speed_rad_s, pressure, self._manifold_temperature_k
            )
            derivative = manifold_pressure_derivative_pa_s(
                throttle_flow,
                cylinder_flow,
                self._manifold_temperature_k,
                intake.manifold_volume_m3,
            )
            pressure = max(MIN_MANIFOLD_PRESSURE_PA, pressure + sub_dt * derivative)
        self._manifold_pressure_pa = pressure
