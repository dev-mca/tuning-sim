"""Data of the 0D model: what the engine JSON provides plus marked assumptions.

The engine definition (shared/engines/<id>.json) has bore, stroke, conrod length, compression
ratio and valve timing, but no valve sizes, lift or flow coefficients. Those are assumed here
(typical four-valve head for an 84 mm bore) and marked [assumption]. A proposal to move them
into the engine JSON is in docs/adr/0010-zero-d-cylinder-model.md.
"""

import json
from dataclasses import dataclass, field
from pathlib import Path

from tuning_lab.engine_definition import EngineDefinition, load_engine
from tuning_lab.paths import engine_path


@dataclass(frozen=True)
class CylinderGeometry:
    """Slider-crank geometry of one cylinder (research.md §1.4)."""

    bore_m: float
    stroke_m: float
    conrod_length_m: float
    compression_ratio: float


@dataclass(frozen=True)
class ValveTiming:
    """Valve events in deg crank angle after the cylinder's own firing TDC (0..720).

    The JSON values are taken as the points of zero lift [assumption: real cam data quote
    them at a small check lift, e.g. 1 mm; then the real opening is somewhat longer].
    """

    exhaust_open_deg: float
    exhaust_close_deg: float
    intake_open_deg: float
    intake_close_deg: float


@dataclass(frozen=True)
class ValveGeometry:
    """One valve group (all intake or all exhaust valves of a cylinder) (research.md §7.4)."""

    count: int
    diameter_m: float
    """Inner seat diameter d_v."""
    max_lift_m: float
    discharge_coefficient: float
    """C_d, research.md §7.4 gives 0.6-0.7; taken as constant over lift and direction."""


# [assumption] Typical four-valve head for B = 84 mm: two intake valves of 32 mm and two exhaust
# valves of 28 mm (intake about 38 % of the bore, exhaust about 33 %), max lift about 0.3 d.
DEFAULT_INTAKE_VALVES = ValveGeometry(
    count=2, diameter_m=0.032, max_lift_m=0.0095, discharge_coefficient=0.65
)
DEFAULT_EXHAUST_VALVES = ValveGeometry(
    count=2, diameter_m=0.028, max_lift_m=0.0090, discharge_coefficient=0.65
)


@dataclass(frozen=True)
class CombustionAssumptions:
    """Wiebe parameters (research.md §7.2) and how the burn duration is chosen."""

    wiebe_efficiency_a: float = 5.0
    """a = 5: 99.3 % burned at the end of the (un-normalised) Wiebe function (§7.2)."""
    wiebe_form_m: float = 2.0
    """m = 2: slow start, fast middle (§7.2)."""
    ignition_delay_deg: float = 5.0
    """[assumption] Crank angle from spark to the start of the Wiebe curve (§7.2 "Zündverzug")."""
    ca50_at_mbt_deg: float = 8.0
    """[assumption] 50 % burned at 8 deg ATDC when the spark is at MBT (common rule of thumb,
    Heywood ch. 9). The burn duration is derived from this and the ignitionMbt table, so the
    0D model's best spark timing stays close to the MBT the MVM uses."""
    min_burn_duration_deg: float = 30.0
    max_burn_duration_deg: float = 90.0
    combustion_efficiency: float = 0.98
    """eta_comb in §7.2 [assumption]: share of the burnable fuel energy that is released."""


@dataclass(frozen=True)
class HeatTransferAssumptions:
    """Simplified Woschni wall heat (research.md §7.3)."""

    enabled: bool = True
    wall_temperature_k: float = 450.0
    """[assumption] Mean of head (~500 K), liner (~400 K) and piston (~550 K)."""
    c1_closed: float = 2.28
    """Woschni C1 for compression, combustion and expansion (Woschni 1967, Heywood ch. 12)."""
    c1_gas_exchange: float = 6.18
    """Woschni C1 while a valve is open (Woschni 1967, Heywood ch. 12)."""
    combustion_term: bool = False
    """Woschni's combustion term C2 V_d T_r / (p_r V_r) (p - p_mot). Off by default because
    research.md §7.3 uses the simplified form; switched on for the comparison in ADR 0010."""
    c2_m_s_k: float = 3.24e-3
    """Woschni C2 for spark ignition engines [m/(s K)] (Woschni 1967, Heywood ch. 12) [prüfen]."""


@dataclass(frozen=True)
class ZeroDEngine:
    """Everything the 0D model needs for one engine."""

    definition: EngineDefinition
    geometry: CylinderGeometry
    valve_timing: ValveTiming
    intake_valves: ValveGeometry = DEFAULT_INTAKE_VALVES
    exhaust_valves: ValveGeometry = DEFAULT_EXHAUST_VALVES
    combustion: CombustionAssumptions = field(default_factory=CombustionAssumptions)
    heat_transfer: HeatTransferAssumptions = field(default_factory=HeatTransferAssumptions)


def load_zero_d_engine(path: Path | None = None) -> ZeroDEngine:
    """Loads the engine JSON (default: the reference engine i6_30_na) for the 0D model.

    The existing loader (`engine_definition.load_engine`) does not read conrod length and valve
    timing because the MVM does not need them; they are read here from the same file.
    """
    path = path if path is not None else engine_path("i6_30_na")
    definition = load_engine(path)
    with path.open(encoding="utf-8") as file:
        data = json.load(file)
    timing = data["valveTiming_deg"]
    return ZeroDEngine(
        definition=definition,
        geometry=CylinderGeometry(
            bore_m=definition.bore_m,
            stroke_m=definition.stroke_m,
            conrod_length_m=float(data["conrodLength_m"]),
            compression_ratio=definition.compression_ratio,
        ),
        valve_timing=ValveTiming(
            exhaust_open_deg=float(timing["evo"]),
            exhaust_close_deg=float(timing["evc"]),
            intake_open_deg=float(timing["ivo"]),
            intake_close_deg=float(timing["ivc"]),
        ),
    )
