"""Engine definition (shared/engines/<id>.json), converted to SI on load (docs/plan.md §5.2)."""

import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from tuning_lab import units
from tuning_lab.table import Table2D, load_table_si

RPM_PER_KRPM = 1.0e3


@dataclass(frozen=True)
class IntakeParameters:
    manifold_volume_m3: float
    throttle_diameter_m: float
    throttle_closed_angle_deg: float
    throttle_discharge_coefficient: float
    throttle_leak_area_m2: float


@dataclass(frozen=True)
class CombustionParameters:
    indicated_efficiency_base: float
    lambda_optimum: float
    lambda_curvature: float
    ignition_curvature_per_deg2: float
    cooling_fraction: float


@dataclass(frozen=True)
class FrictionParameters:
    """fmep = c0 + c1 * (N/1000) + c2 * (N/1000)^2 with N in 1/min (research.md §2.8)."""

    c0_pa: float
    c1_pa_per_krpm: float
    c2_pa_per_krpm2: float

    def fmep_pa(self, speed_rad_s: float) -> float:
        krpm = units.rad_s_to_rpm(speed_rad_s) / RPM_PER_KRPM
        return self.c0_pa + self.c1_pa_per_krpm * krpm + self.c2_pa_per_krpm2 * krpm * krpm


@dataclass(frozen=True)
class EngineTables:
    """ECU tune (ve, lambda_target, ignition) and model property ignition_mbt."""

    ve: Table2D
    lambda_target: Table2D
    ignition: Table2D
    ignition_mbt: Table2D


@dataclass(frozen=True)
class OverrunCutParameters:
    """Deceleration fuel cut (Schubabschaltung, research.md §3.3)."""

    enter_rad_s: float
    exit_rad_s: float
    closed_throttle: float
    """Throttle positions at or below this count as closed."""


@dataclass(frozen=True)
class AfterburnParameters:
    """Late combustion, afterburn and catalyst temperature (research.md §2.10a)."""

    late_combustion_start_deg: float
    late_combustion_span_deg: float
    late_combustion_max: float
    afterburn_efficiency: float
    catalyst_time_constant_s: float
    catalyst_initial_k: float


@dataclass(frozen=True)
class DrivetrainParameters:
    """Gearbox, clutch and the car on the roller dyno (research.md §8)."""

    gear_ratios: tuple[float, ...]
    """i_g of the forward gears, index 0 = 1st gear."""
    final_drive_ratio: float
    efficiency: float
    """eta_dl of gearbox and differential, applied to the torque at the wheels (§8.1)."""
    wheel_radius_m: float
    vehicle_mass_kg: float
    wheel_inertia_kg_m2: float
    """All wheels, axles and the propshaft, at wheel speed."""
    drag_area_m2: float
    """c_w * A."""
    rolling_resistance: float
    """c_r."""
    clutch_max_torque_nm: float
    clutch_release_time_s: float
    """Time from pedal up to a fully closed clutch."""
    shift_time_s: float
    """Torque interruption of a shift (clutch open, gear changes at its end)."""
    shift_engage_time_s: float
    """Clutch closing after a shift."""
    launch_max_speed_m_s: float
    """Launch control is armed by the clutch only below this speed (standing car)."""
    stall_speed_rad_s: float
    """Below this the engine has stalled; it restarts once the drivetrain is decoupled."""

    @property
    def gear_count(self) -> int:
        return len(self.gear_ratios)

    def overall_ratio(self, gear: int) -> float:
        """i = i_g * i_f from engine to wheels (research.md §8.1); gear 1 = first gear."""
        return self.gear_ratios[gear - 1] * self.final_drive_ratio


@dataclass(frozen=True)
class EngineDefinition:
    id: str
    name: str
    cylinders: int
    bore_m: float
    stroke_m: float
    compression_ratio: float
    firing_order: tuple[int, ...]
    firing_angles_deg: tuple[float, ...]
    idle_speed_rad_s: float
    rev_limit_rad_s: float
    rev_limit_hysteresis_rad_s: float
    inertia_kg_m2: float
    intake: IntakeParameters
    combustion: CombustionParameters
    friction: FrictionParameters
    accessory_torque_nm: float
    exhaust_backpressure_coefficient: float
    tables: EngineTables
    overrun_cut: OverrunCutParameters
    afterburn: AfterburnParameters
    drivetrain: DrivetrainParameters

    @property
    def displacement_m3(self) -> float:
        """Total displaced volume V_d = z * pi/4 * B^2 * S (research.md §1.4)."""
        return self.cylinders * math.pi / 4.0 * self.bore_m**2 * self.stroke_m


def load_engine(path: Path) -> EngineDefinition:
    """Loads shared/engines/<id>.json; table paths are resolved relative to shared/."""
    with path.open(encoding="utf-8") as file:
        data: dict[str, Any] = json.load(file)
    shared_dir = path.parent.parent
    return _engine_from_dict(data, shared_dir)


def _engine_from_dict(data: dict[str, Any], shared_dir: Path) -> EngineDefinition:
    intake = data["intake"]
    combustion = data["combustion"]
    friction = data["friction"]
    tables = data["tables"]
    cylinders = int(data["cylinders"])
    firing_angles = tuple(float(a) for a in data["firingAngles_deg"])
    if len(firing_angles) != cylinders or len(data["firingOrder"]) != cylinders:
        raise ValueError("firingOrder and firingAngles_deg need one entry per cylinder")
    return EngineDefinition(
        id=data["id"],
        name=data["name"],
        cylinders=cylinders,
        bore_m=float(data["bore_m"]),
        stroke_m=float(data["stroke_m"]),
        compression_ratio=float(data["compressionRatio"]),
        firing_order=tuple(int(c) for c in data["firingOrder"]),
        firing_angles_deg=firing_angles,
        idle_speed_rad_s=units.rpm_to_rad_s(float(data["idle_rpm"])),
        rev_limit_rad_s=units.rpm_to_rad_s(float(data["revLimit_rpm"])),
        rev_limit_hysteresis_rad_s=units.rpm_to_rad_s(float(data["revLimitHysteresis_rpm"])),
        inertia_kg_m2=float(data["inertia_kg_m2"]),
        intake=IntakeParameters(
            manifold_volume_m3=float(intake["manifoldVolume_m3"]),
            throttle_diameter_m=float(intake["throttleDiameter_m"]),
            throttle_closed_angle_deg=float(intake["throttleClosedAngle_deg"]),
            throttle_discharge_coefficient=float(intake["throttleDischargeCoefficient"]),
            throttle_leak_area_m2=float(intake["throttleLeakArea_m2"]),
        ),
        combustion=CombustionParameters(
            indicated_efficiency_base=float(combustion["indicatedEfficiencyBase"]),
            lambda_optimum=float(combustion["lambdaOptimum"]),
            lambda_curvature=float(combustion["lambdaCurvature"]),
            ignition_curvature_per_deg2=float(combustion["ignitionCurvature_per_deg2"]),
            cooling_fraction=float(combustion["coolingFraction"]),
        ),
        friction=FrictionParameters(
            c0_pa=units.kpa_to_pa(float(friction["fmep_c0_kPa"])),
            c1_pa_per_krpm=units.kpa_to_pa(float(friction["fmep_c1_kPa_per_krpm"])),
            c2_pa_per_krpm2=units.kpa_to_pa(float(friction["fmep_c2_kPa_per_krpm2"])),
        ),
        accessory_torque_nm=float(data["accessoryTorque_Nm"]),
        exhaust_backpressure_coefficient=float(
            data["exhaust"]["backpressureCoefficient_Pa_s2_per_kg2"]
        ),
        tables=EngineTables(
            ve=load_table_si(shared_dir / tables["ve"]),
            lambda_target=load_table_si(shared_dir / tables["lambdaTarget"]),
            ignition=load_table_si(shared_dir / tables["ignition"]),
            ignition_mbt=load_table_si(shared_dir / tables["ignitionMbt"]),
        ),
        overrun_cut=OverrunCutParameters(
            enter_rad_s=units.rpm_to_rad_s(float(data["overrunFuelCut"]["enter_rpm"])),
            exit_rad_s=units.rpm_to_rad_s(float(data["overrunFuelCut"]["exit_rpm"])),
            closed_throttle=float(data["overrunFuelCut"]["closedThrottle"]),
        ),
        afterburn=AfterburnParameters(
            late_combustion_start_deg=float(data["afterburn"]["lateCombustionStart_deg"]),
            late_combustion_span_deg=float(data["afterburn"]["lateCombustionSpan_deg"]),
            late_combustion_max=float(data["afterburn"]["lateCombustionMax"]),
            afterburn_efficiency=float(data["afterburn"]["afterburnEfficiency"]),
            catalyst_time_constant_s=float(data["afterburn"]["catalystTimeConstant_s"]),
            catalyst_initial_k=units.celsius_to_kelvin(
                float(data["afterburn"]["catalystInitial_C"])
            ),
        ),
        drivetrain=_drivetrain_from_dict(data["drivetrain"]),
    )


def _drivetrain_from_dict(data: dict[str, Any]) -> DrivetrainParameters:
    ratios = tuple(float(ratio) for ratio in data["gearRatios"])
    if not ratios or any(ratio <= 0.0 for ratio in ratios):
        raise ValueError("gearRatios needs at least one positive ratio")
    return DrivetrainParameters(
        gear_ratios=ratios,
        final_drive_ratio=float(data["finalDriveRatio"]),
        efficiency=float(data["efficiency"]),
        wheel_radius_m=float(data["wheelRadius_m"]),
        vehicle_mass_kg=float(data["vehicleMass_kg"]),
        wheel_inertia_kg_m2=float(data["wheelInertia_kg_m2"]),
        drag_area_m2=float(data["dragArea_m2"]),
        rolling_resistance=float(data["rollingResistance"]),
        clutch_max_torque_nm=float(data["clutchMaxTorque_Nm"]),
        clutch_release_time_s=float(data["clutchReleaseTime_s"]),
        shift_time_s=float(data["shiftTime_s"]),
        shift_engage_time_s=float(data["shiftEngageTime_s"]),
        launch_max_speed_m_s=units.kmh_to_ms(float(data["launchMaxSpeed_kmh"])),
        stall_speed_rad_s=units.rpm_to_rad_s(float(data["stallSpeed_rpm"])),
    )
