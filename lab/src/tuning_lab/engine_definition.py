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
    )
