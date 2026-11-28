"""Minimal speed-density ECU (docs/research.md §2.2, §2.4, §3).

Concepts as in Speeduino/rusEFI (no code taken from them, they are GPL):
load = MAP, read VE / lambda target / ignition from tables, compute the fuel mass.
Rev limiter: hard fuel cut with hysteresis.
Overrun (deceleration) fuel cut: throttle closed above a speed threshold (research.md §3.3).
With the cut switched off the ECU keeps fuelling in overrun, optionally with retarded ignition
and extra fuel: the "pops & bangs" map (research.md §3.3, §2.10a).
Launch control (2-step, research.md §3.4): with the launch switch held and the throttle open the
ECU retards the ignition and holds a lower speed limit by cutting the spark while it keeps
injecting, so the unburnt mixture can bang in the exhaust.
"""

from dataclasses import dataclass

from tuning_lab import units
from tuning_lab.constants import AFR_STOICH_GASOLINE
from tuning_lab.engine_definition import EngineTables, OverrunCutParameters
from tuning_lab.mvm import air_density_kg_m3
from tuning_lab.table import Table2D


@dataclass(frozen=True)
class OverrunSettings:
    """What the ECU does when the driver lifts off at speed (research.md §3.3)."""

    fuel_cut: bool = True
    """True: no fuel in overrun (stock). False: keep fuelling (needed for pops)."""
    retard_deg: float = 0.0
    """Ignition taken back from the map while fuelling in overrun, deg."""
    enrichment: float = 0.0
    """Extra fuel while fuelling in overrun, as a fraction (0.1 = +10 %)."""


DEFAULT_LAUNCH_RPM = 4000.0
"""Typical 2-step launch speed of a naturally aspirated engine (research.md §3.4)."""


@dataclass(frozen=True)
class LaunchSettings:
    """Launch control / 2-step while standing (research.md §3.4)."""

    enabled: bool = False
    """Stock ECUs have no launch control."""
    launch_speed_rad_s: float = units.rpm_to_rad_s(DEFAULT_LAUNCH_RPM)
    """Speed the ECU holds by cutting the spark while the launch switch is held."""
    retard_deg: float = 0.0
    """Ignition taken back from the map while launch control is active, deg."""


@dataclass(frozen=True)
class Tune:
    """The tables and settings an ECU works with; this is what the user tunes."""

    ve: Table2D
    lambda_target: Table2D
    ignition: Table2D
    overrun: OverrunSettings = OverrunSettings()
    launch: LaunchSettings = LaunchSettings()

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
    overrun_fueling: bool = False
    """True while the ECU fuels in overrun (fuel cut switched off): pops are possible."""
    launch_active: bool = False
    """True while launch control works (enabled, switch held, throttle open)."""
    spark_cut: bool = False
    """True while launch control cuts the spark but keeps injecting."""


class Ecu:
    def __init__(
        self,
        tune: Tune,
        displacement_m3: float,
        rev_limit_rad_s: float,
        rev_limit_hysteresis_rad_s: float,
        overrun_cut: OverrunCutParameters,
    ) -> None:
        self._tune = tune
        self._displacement_m3 = displacement_m3
        self._rev_limit_rad_s = rev_limit_rad_s
        self._rev_resume_rad_s = rev_limit_rad_s - rev_limit_hysteresis_rad_s
        self._overrun = overrun_cut
        self._rev_limit_hysteresis_rad_s = rev_limit_hysteresis_rad_s
        self._limiter_cut = False
        self._in_overrun = False
        self._launch_switch = False
        self._launch_cut = False

    def set_launch_switch(self, held: bool) -> None:
        """The driver's launch button (later: clutch pedal while standing, iteration 10)."""
        self._launch_switch = held

    def update(
        self,
        speed_rad_s: float,
        manifold_pressure_pa: float,
        intake_temperature_k: float,
        throttle: float,
    ) -> EcuCommand:
        self._update_rev_limiter(speed_rad_s)
        self._update_overrun(speed_rad_s, throttle)
        launch_active = self._update_launch(speed_rad_s, throttle)
        overrun = self._tune.overrun
        lambda_target = self._tune.lambda_target.lookup(speed_rad_s, manifold_pressure_pa)
        ignition_deg = self._tune.ignition.lookup(speed_rad_s, manifold_pressure_pa)
        if self._limiter_cut or (self._in_overrun and overrun.fuel_cut):
            return EcuCommand(0.0, ignition_deg, lambda_target, fuel_cut=True)
        if self._in_overrun:
            ignition_deg = ignition_deg - overrun.retard_deg
        if launch_active:
            ignition_deg = ignition_deg - self._tune.launch.retard_deg

        # §2.2 + §2.4: the ECU's belief of the air mass, then fuel for the target lambda.
        believed_ve = self._tune.ve.lookup(speed_rad_s, manifold_pressure_pa)
        believed_air_per_cycle = (
            believed_ve
            * air_density_kg_m3(manifold_pressure_pa, intake_temperature_k)
            * self._displacement_m3
        )
        fuel_per_cycle = believed_air_per_cycle / (lambda_target * AFR_STOICH_GASOLINE)
        if self._in_overrun:
            fuel_per_cycle = fuel_per_cycle * (1.0 + overrun.enrichment)
        return EcuCommand(
            fuel_per_cycle,
            ignition_deg,
            lambda_target,
            fuel_cut=False,
            overrun_fueling=self._in_overrun,
            launch_active=launch_active,
            spark_cut=launch_active and self._launch_cut,
        )

    def _update_launch(self, speed_rad_s: float, throttle: float) -> bool:
        """Launch limiter: spark cut with the same hysteresis as the rev limiter."""
        launch = self._tune.launch
        active = launch.enabled and self._launch_switch and throttle > self._overrun.closed_throttle
        if not active or speed_rad_s < launch.launch_speed_rad_s - self._rev_limit_hysteresis_rad_s:
            self._launch_cut = False
        elif speed_rad_s > launch.launch_speed_rad_s:
            self._launch_cut = True
        return active

    def _update_rev_limiter(self, speed_rad_s: float) -> None:
        # Hysteresis: cut above the limit, resume only after dropping below limit - hysteresis.
        if speed_rad_s > self._rev_limit_rad_s:
            self._limiter_cut = True
        elif speed_rad_s < self._rev_resume_rad_s:
            self._limiter_cut = False

    def _update_overrun(self, speed_rad_s: float, throttle: float) -> None:
        # Overrun = coasting with closed throttle above enter speed; it ends below the exit
        # speed (before idle, so the engine catches itself) or when the throttle opens.
        closed = throttle <= self._overrun.closed_throttle
        if not closed or speed_rad_s < self._overrun.exit_rad_s:
            self._in_overrun = False
        elif speed_rad_s > self._overrun.enter_rad_s:
            self._in_overrun = True
