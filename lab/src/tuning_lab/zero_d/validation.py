"""Comparison of the 0D model with the mean-value model (docs/plan.md iteration 11).

Both models get the same operating point: speed, manifold pressure, the stock ECU's ignition
and lambda target. The MVM's air mass comes from its VE table; the 0D model computes its own
air mass from the valve flow. Uses the existing MVM, ECU and intake code read-only.
"""

from dataclasses import dataclass

from tuning_lab import units
from tuning_lab.constants import AMBIENT_PRESSURE_PA, AMBIENT_TEMPERATURE_K
from tuning_lab.ecu import Ecu, Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.intake import orifice_mass_flow_kg_s, throttle_area_m2
from tuning_lab.mvm import EngineModel, EngineOutputs, OperatingPoint
from tuning_lab.zero_d.cylinder import SolverSettings, ZeroDOperatingPoint
from tuning_lab.zero_d.engine import ZeroDEngineResult, run_operating_point
from tuning_lab.zero_d.parameters import ZeroDEngine

WIDE_OPEN_THROTTLE = 1.0
BISECTION_ITERATIONS = 60
LOWEST_MANIFOLD_PRESSURE_PA = 10_000.0

FULL_LOAD_SPEEDS_RPM = (1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0)
PART_LOAD_POINTS_RPM_KPA = ((800.0, 35.0), (2000.0, 50.0), (3000.0, 60.0), (4000.0, 40.0))


@dataclass(frozen=True)
class ComparisonPoint:
    speed_rpm: float
    manifold_pressure_pa: float
    ignition_deg: float
    lambda_target: float
    mvm: EngineOutputs
    zero_d: ZeroDEngineResult

    @property
    def mvm_net_indicated_torque_nm(self) -> float:
        """MVM indicated torque minus its pumping torque (research.md §2.8)."""
        return self.mvm.indicated_torque_nm - self.mvm.pumping_torque_nm

    @property
    def brake_torque_deviation(self) -> float:
        """(0D - MVM) / MVM of the brake torque."""
        return self.zero_d.brake_torque_nm / self.mvm.brake_torque_nm - 1.0

    @property
    def indicated_torque_deviation(self) -> float:
        """(0D gross - MVM) / MVM of the indicated torque before pumping losses."""
        return self.zero_d.indicated_torque_gross_nm / self.mvm.indicated_torque_nm - 1.0

    @property
    def air_mass_deviation(self) -> float:
        return self.zero_d.air_mass_per_cycle_kg / self.mvm.air_mass_per_cycle_kg - 1.0


def full_load_manifold_pressure_pa(definition: EngineDefinition, speed_rad_s: float) -> float:
    """Steady MAP at wide-open throttle in the MVM: throttle flow = engine air flow (§2.5).

    Bisection on a monotonic function, so the result is deterministic and exact enough.
    """
    model = EngineModel(definition)
    area = throttle_area_m2(WIDE_OPEN_THROTTLE, definition.intake)
    low, high = LOWEST_MANIFOLD_PRESSURE_PA, AMBIENT_PRESSURE_PA
    for _ in range(BISECTION_ITERATIONS):
        middle = 0.5 * (low + high)
        throttle_flow = orifice_mass_flow_kg_s(
            area,
            definition.intake.throttle_discharge_coefficient,
            AMBIENT_PRESSURE_PA,
            AMBIENT_TEMPERATURE_K,
            middle,
        )
        engine_flow = model.air_mass_flow_kg_s(speed_rad_s, middle, AMBIENT_TEMPERATURE_K)
        if throttle_flow > engine_flow:
            low = middle
        else:
            high = middle
    return 0.5 * (low + high)


def compare_point(
    engine: ZeroDEngine,
    speed_rpm: float,
    manifold_pressure_pa: float,
    settings: SolverSettings | None = None,
) -> ComparisonPoint:
    """Evaluates MVM (stock tune) and 0D model at the same speed and MAP."""
    definition = engine.definition
    speed = units.rpm_to_rad_s(speed_rpm)
    ecu = Ecu(
        Tune.from_tables(definition.tables),
        definition.displacement_m3,
        definition.rev_limit_rad_s,
        definition.rev_limit_hysteresis_rad_s,
        definition.overrun_cut,
    )
    command = ecu.update(speed, manifold_pressure_pa, AMBIENT_TEMPERATURE_K, WIDE_OPEN_THROTTLE)
    mvm = EngineModel(definition).evaluate(
        OperatingPoint(
            speed_rad_s=speed,
            manifold_pressure_pa=manifold_pressure_pa,
            manifold_temperature_k=AMBIENT_TEMPERATURE_K,
            fuel_mass_per_cycle_kg=command.fuel_mass_per_cycle_kg,
            ignition_deg=command.ignition_deg,
        )
    )
    # [assumption] The 0D model gets the lambda target, as if a lambda controller had trimmed
    # the fuel to its own (different) air mass. Otherwise a VE mismatch would change lambda.
    zero_d = run_operating_point(
        engine,
        ZeroDOperatingPoint(
            speed_rad_s=speed,
            intake_pressure_pa=manifold_pressure_pa,
            ignition_deg=command.ignition_deg,
            lambda_value=command.lambda_target,
        ),
        settings,
    )
    return ComparisonPoint(
        speed_rpm=speed_rpm,
        manifold_pressure_pa=manifold_pressure_pa,
        ignition_deg=command.ignition_deg,
        lambda_target=command.lambda_target,
        mvm=mvm,
        zero_d=zero_d,
    )


def full_load_comparison(
    engine: ZeroDEngine,
    speeds_rpm: tuple[float, ...] = FULL_LOAD_SPEEDS_RPM,
    settings: SolverSettings | None = None,
) -> list[ComparisonPoint]:
    definition = engine.definition
    return [
        compare_point(
            engine,
            rpm,
            full_load_manifold_pressure_pa(definition, units.rpm_to_rad_s(rpm)),
            settings,
        )
        for rpm in speeds_rpm
    ]


def part_load_comparison(
    engine: ZeroDEngine,
    points_rpm_kpa: tuple[tuple[float, float], ...] = PART_LOAD_POINTS_RPM_KPA,
    settings: SolverSettings | None = None,
) -> list[ComparisonPoint]:
    return [
        compare_point(engine, rpm, units.kpa_to_pa(kpa), settings) for rpm, kpa in points_rpm_kpa
    ]


def format_table(points: list[ComparisonPoint]) -> str:
    """Markdown table for the ADR and the terminal."""
    header = (
        "| rpm | MAP kPa | T_i MVM | T_i,g 0D | T_e MVM | T_e 0D | dT_e | air MVM mg | "
        "air 0D mg | d air | VE 0D | EGT MVM °C | EGT 0D °C | p_max bar |\n"
        "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"
    )
    rows = [
        f"| {p.speed_rpm:.0f} | {units.pa_to_kpa(p.manifold_pressure_pa):.1f} "
        f"| {p.mvm.indicated_torque_nm:.1f} | {p.zero_d.indicated_torque_gross_nm:.1f} "
        f"| {p.mvm.brake_torque_nm:.1f} | {p.zero_d.brake_torque_nm:.1f} "
        f"| {100.0 * p.brake_torque_deviation:+.1f} % "
        f"| {1e6 * p.mvm.air_mass_per_cycle_kg:.0f} | {1e6 * p.zero_d.air_mass_per_cycle_kg:.0f} "
        f"| {100.0 * p.air_mass_deviation:+.1f} % | {p.zero_d.volumetric_efficiency:.3f} "
        f"| {units.kelvin_to_celsius(p.mvm.exhaust_temperature_k):.0f} "
        f"| {units.kelvin_to_celsius(p.zero_d.exhaust_temperature_k):.0f} "
        f"| {units.pa_to_bar(p.zero_d.peak_pressure_pa):.1f} |"
        for p in points
    ]
    return "\n".join([header, *rows])
