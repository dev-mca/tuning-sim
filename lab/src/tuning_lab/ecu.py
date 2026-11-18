"""Minimal speed-density ECU (docs/research.md §2.2, §2.4, §3).

Concepts as in Speeduino/rusEFI (no code taken from them, they are GPL):
load = MAP, read VE / lambda target / ignition from tables, compute the fuel mass.
Rev limiter: hard fuel cut with hysteresis.
"""

from dataclasses import dataclass

from tuning_lab.constants import AFR_STOICH_GASOLINE
from tuning_lab.engine_definition import EngineTables
from tuning_lab.mvm import air_density_kg_m3
from tuning_lab.table import Table2D


@dataclass(frozen=True)
class Tune:
    """The tables an ECU works with; this is what the user tunes."""

    ve: Table2D
    lambda_target: Table2D
    ignition: Table2D

    @classmethod
    def from_tables(cls, tables: EngineTables) -> "Tune":
        """The stock tune shipped with an engine definition.

        Variants: dataclasses.replace(tune, ignition=tune.ignition.with_offset(3.0)).
        """
        return cls(tables.ve, tables.lambda_target, tables.ignition)


@dataclass(frozen=True)
class EcuCommand:
    fuel_mass_per_cycle_kg: float
    ignition_deg: float
    lambda_target: float
    fuel_cut: bool


class Ecu:
    def __init__(
        self,
        tune: Tune,
        displacement_m3: float,
        rev_limit_rad_s: float,
        rev_limit_hysteresis_rad_s: float,
    ) -> None:
        self._tune = tune
        self._displacement_m3 = displacement_m3
        self._rev_limit_rad_s = rev_limit_rad_s
        self._rev_resume_rad_s = rev_limit_rad_s - rev_limit_hysteresis_rad_s
        self._fuel_cut = False

    def update(
        self, speed_rad_s: float, manifold_pressure_pa: float, intake_temperature_k: float
    ) -> EcuCommand:
        self._update_rev_limiter(speed_rad_s)
        lambda_target = self._tune.lambda_target.lookup(speed_rad_s, manifold_pressure_pa)
        ignition_deg = self._tune.ignition.lookup(speed_rad_s, manifold_pressure_pa)
        if self._fuel_cut:
            return EcuCommand(0.0, ignition_deg, lambda_target, fuel_cut=True)

        # §2.2 + §2.4: the ECU's belief of the air mass, then fuel for the target lambda.
        believed_ve = self._tune.ve.lookup(speed_rad_s, manifold_pressure_pa)
        believed_air_per_cycle = (
            believed_ve
            * air_density_kg_m3(manifold_pressure_pa, intake_temperature_k)
            * self._displacement_m3
        )
        fuel_per_cycle = believed_air_per_cycle / (lambda_target * AFR_STOICH_GASOLINE)
        return EcuCommand(fuel_per_cycle, ignition_deg, lambda_target, fuel_cut=False)

    def _update_rev_limiter(self, speed_rad_s: float) -> None:
        # Hysteresis: cut above the limit, resume only after dropping below limit - hysteresis.
        if speed_rad_s > self._rev_limit_rad_s:
            self._fuel_cut = True
        elif speed_rad_s < self._rev_resume_rad_s:
            self._fuel_cut = False
