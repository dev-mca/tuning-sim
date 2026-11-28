"""Clutch, gearbox and the car on a road-load roller dyno (docs/research.md §8, iteration 10).

The engine drives the car through clutch -> gearbox (i_g) -> final drive (i_f) -> wheels. The
roller dyno simulates the car's mass and road load, so on the rollers the car behaves as on a
flat road (§8.4). The clutch is a state machine (§8.3):

- open (neutral, pedal pressed, shifting): no torque, engine and car run on their own,
- slipping: the clutch passes its capacity T_cap = engagement * T_max in the direction of the
  slip; engine and car are integrated separately,
- locked: engine and car turn together as one inertia; the clutch passes what that needs, as
  long as it stays within T_cap.

A step is split around the engine: clutch_torque() before the crank integration (the engine
needs its load torque), integrate() after it.
"""

from dataclasses import dataclass
from enum import Enum

from tuning_lab.constants import AMBIENT_PRESSURE_PA, AMBIENT_TEMPERATURE_K, STANDARD_GRAVITY_M_S2
from tuning_lab.engine_definition import DrivetrainParameters
from tuning_lab.mvm import air_density_kg_m3

NEUTRAL = 0
LOCK_SLIP_RAD_S = 1.0
"""Slip below which a slipping clutch may lock (also when the slip changes sign in a step)."""


class ClutchState(Enum):
    OPEN = "open"
    SLIPPING = "slipping"
    LOCKED = "locked"


class ShiftPhase(Enum):
    NONE = "none"
    INTERRUPT = "interrupt"
    """Clutch open, torque interrupted; the gear changes at the end of this phase."""
    ENGAGE = "engage"
    """Clutch closes again after the shift."""


@dataclass(frozen=True)
class DrivetrainState:
    gear: int
    clutch_engagement: float
    """0 = open .. 1 = closed."""
    clutch_state: ClutchState
    shifting: bool
    """True during the torque interruption of a shift."""
    vehicle_speed_m_s: float
    clutch_torque_nm: float
    """Torque the clutch passes from the engine (negative = engine braking)."""
    wheel_torque_nm: float
    """Sum of the driven wheels: T_clutch * i * eta (§8.1)."""
    wheel_force_n: float
    """Drive force at the tyres = what the roller dyno measures."""
    brake: float
    """Brake pedal 0..1."""


def road_load_n(parameters: DrivetrainParameters, speed_m_s: float) -> float:
    """F_air + F_roll at speed v >= 0 (research.md §8.2); the rollers simulate both."""
    if speed_m_s <= 0.0:
        return 0.0
    air_density = air_density_kg_m3(AMBIENT_PRESSURE_PA, AMBIENT_TEMPERATURE_K)
    aero = 0.5 * air_density * parameters.drag_area_m2 * speed_m_s * speed_m_s
    rolling = parameters.rolling_resistance * parameters.vehicle_mass_kg * STANDARD_GRAVITY_M_S2
    return aero + rolling


class Drivetrain:
    def __init__(self, parameters: DrivetrainParameters, engine_inertia_kg_m2: float) -> None:
        self._p = parameters
        self._engine_inertia = engine_inertia_kg_m2
        # m_v: car plus wheels as an equivalent mass at the tyres (§8.2).
        self._vehicle_mass = parameters.vehicle_mass_kg + parameters.wheel_inertia_kg_m2 / (
            parameters.wheel_radius_m**2
        )
        self._gear = NEUTRAL
        self._target_gear = NEUTRAL
        self._pedal_pressed = False
        self._engagement = 1.0
        self._state = ClutchState.OPEN
        self._shift_phase = ShiftPhase.NONE
        self._shift_elapsed_s = 0.0
        self._speed_m_s = 0.0
        self._clutch_torque = 0.0
        self._brake = 0.0

    # --- driver inputs ---

    def set_clutch_pedal(self, pressed: bool) -> None:
        self._pedal_pressed = pressed

    def set_brake(self, pedal: float) -> None:
        """Brake pedal 0..1 (clamped)."""
        self._brake = min(max(pedal, 0.0), 1.0)

    def shift(self, gear: int) -> bool:
        """Starts a shift to `gear` (0 = neutral); False if invalid or already shifting."""
        if not NEUTRAL <= gear <= self._p.gear_count or gear == self._gear:
            return False
        if self._shift_phase is not ShiftPhase.NONE:
            return False
        self._target_gear = gear
        self._shift_phase = ShiftPhase.INTERRUPT
        self._shift_elapsed_s = 0.0
        return True

    def place_in_gear(self, gear: int, engine_speed_rad_s: float) -> None:
        """Puts the car in `gear` with a closed clutch at the speed that matches the engine.

        Used to start a roller run directly in gear (no driver).
        """
        self._gear = gear
        self._target_gear = gear
        self._shift_phase = ShiftPhase.NONE
        self._pedal_pressed = False
        self._engagement = 1.0
        self._state = ClutchState.LOCKED
        self._speed_m_s = engine_speed_rad_s * self._p.wheel_radius_m / self._ratio()

    # --- queries ---

    @property
    def gear(self) -> int:
        return self._gear

    @property
    def shifting(self) -> bool:
        return self._shift_phase is ShiftPhase.INTERRUPT

    @property
    def vehicle_speed_m_s(self) -> float:
        return self._speed_m_s

    @property
    def decoupled(self) -> bool:
        """True when no torque can pass: neutral or clutch fully open."""
        return self._gear == NEUTRAL or self._engagement <= 0.0

    def launch_armed(self) -> bool:
        """Clutch pedal pressed with a standing car: arms the 2-step (research.md §3.4)."""
        return self._pedal_pressed and self._speed_m_s < self._p.launch_max_speed_m_s

    # --- one time step ---

    def update_controls(self, dt: float) -> None:
        """Shift phases and clutch engagement; call once per step before clutch_torque()."""
        if self._shift_phase is ShiftPhase.INTERRUPT:
            self._shift_elapsed_s += dt
            if self._shift_elapsed_s >= self._p.shift_time_s:
                self._gear = self._target_gear
                self._shift_phase = ShiftPhase.ENGAGE
                self._shift_elapsed_s = 0.0
        if self._pedal_pressed or self._shift_phase is ShiftPhase.INTERRUPT:
            self._engagement = 0.0  # pressing the pedal opens the clutch at once
            return
        closing_time = (
            self._p.shift_engage_time_s
            if self._shift_phase is ShiftPhase.ENGAGE
            else self._p.clutch_release_time_s
        )
        self._engagement = min(1.0, self._engagement + dt / closing_time)
        if self._shift_phase is ShiftPhase.ENGAGE and self._engagement >= 1.0:
            self._shift_phase = ShiftPhase.NONE

    def clutch_torque(self, engine_torque_nm: float, engine_speed_rad_s: float) -> float:
        """Load torque on the crank for this step (research.md §8.3)."""
        if self.decoupled:
            self._state = ClutchState.OPEN
            self._clutch_torque = 0.0
            return 0.0
        capacity = self._engagement * self._p.clutch_max_torque_nm
        if self._state is ClutchState.LOCKED:
            locked_torque = self._locked_clutch_torque(engine_torque_nm)
            if abs(locked_torque) <= capacity:
                self._clutch_torque = locked_torque
                return locked_torque
            self._state = ClutchState.SLIPPING  # too much torque: the clutch breaks loose
        slip = engine_speed_rad_s - self._input_speed_rad_s()
        if self._state is ClutchState.OPEN:
            self._state = ClutchState.SLIPPING
        if slip == 0.0:
            # Same speed on both sides (e.g. both standing): lock if the clutch can hold it.
            locked_torque = self._locked_clutch_torque(engine_torque_nm)
            if abs(locked_torque) <= capacity:
                self._state = ClutchState.LOCKED
                self._clutch_torque = locked_torque
                return locked_torque
            slip = locked_torque  # the sign of the needed torque decides the direction
        self._clutch_torque = capacity if slip > 0.0 else -capacity
        return self._clutch_torque

    def integrate(self, dt: float, engine_speed_rad_s: float) -> float | None:
        """Moves the car on; returns a new engine speed when the clutch has just locked."""
        if self._state is ClutchState.LOCKED:
            # Kinematic coupling: the car follows the crank exactly (no drift).
            self._speed_m_s = engine_speed_rad_s * self._p.wheel_radius_m / self._ratio()
            return None
        drive_force = self._wheel_force(self._clutch_torque)
        acceleration = (drive_force - self._resistance_n(drive_force)) / self._vehicle_mass
        self._speed_m_s = max(0.0, self._speed_m_s + dt * acceleration)
        if self._state is not ClutchState.SLIPPING:
            return None
        # The clutch torque points in the direction of the slip at the start of the step.
        slip = engine_speed_rad_s - self._input_speed_rad_s()
        if abs(slip) < LOCK_SLIP_RAD_S or (slip > 0.0) != (self._clutch_torque > 0.0):
            return self._lock(engine_speed_rad_s)
        return None

    def state(self) -> DrivetrainState:
        wheel_force = self._wheel_force(self._clutch_torque)
        return DrivetrainState(
            gear=self._gear,
            clutch_engagement=self._engagement,
            clutch_state=self._state,
            shifting=self.shifting,
            vehicle_speed_m_s=self._speed_m_s,
            clutch_torque_nm=self._clutch_torque,
            wheel_torque_nm=wheel_force * self._p.wheel_radius_m,
            wheel_force_n=wheel_force,
            brake=self._brake,
        )

    # --- helpers ---

    def _ratio(self) -> float:
        return self._p.overall_ratio(self._gear)

    def _input_speed_rad_s(self) -> float:
        """Gearbox input speed that matches the car: omega_in = v / r * i (§8.1)."""
        return self._speed_m_s / self._p.wheel_radius_m * self._ratio()

    def _wheel_force(self, clutch_torque_nm: float) -> float:
        if self.decoupled:
            return 0.0
        return clutch_torque_nm * self._ratio() * self._p.efficiency / self._p.wheel_radius_m

    def _locked_clutch_torque(self, engine_torque_nm: float) -> float:
        """Clutch torque when engine and car accelerate together (§8.2 with m_eff).

        m_v dv/dt = T_c i eta / r - F_res,  T_c = T_e - J_e (i/r) dv/dt
        -> dv/dt = (T_e i eta / r - F_res) / (m_v + J_e i^2 eta / r^2)
        """
        ratio = self._ratio()
        radius = self._p.wheel_radius_m
        efficiency = self._p.efficiency
        effective_mass = self._vehicle_mass + self._engine_inertia * ratio * ratio * efficiency / (
            radius * radius
        )
        drive_force = engine_torque_nm * ratio * efficiency / radius
        acceleration = (drive_force - self._resistance_n(drive_force)) / effective_mass
        return engine_torque_nm - self._engine_inertia * ratio / radius * acceleration

    def _resistance_n(self, drive_force_n: float) -> float:
        """Road load plus brake force against the drive force (research.md §8.5).

        Moving, the brake takes its full force (tyre grip). Standing, it holds the car: it
        takes what would otherwise push the car forward, up to its holding force (brake torque
        on the driven wheels), so it never pulls backwards.
        """
        road = road_load_n(self._p, self._speed_m_s)
        full_brake = self._brake * self._p.vehicle_mass_kg * self._p.brake_max_deceleration_m_s2
        if self._speed_m_s > 0.0:
            return road + full_brake
        holding = full_brake * self._p.brake_hold_factor
        return road + min(holding, max(drive_force_n - road, 0.0))

    def _lock(self, engine_speed_rad_s: float) -> float:
        """Locks the clutch at the momentum-conserving common speed; returns it for the crank."""
        ratio = self._ratio()
        radius = self._p.wheel_radius_m
        # Car inertia seen at the gearbox input: m_v r^2 / i^2.
        input_inertia = self._vehicle_mass * radius * radius / (ratio * ratio)
        common_speed = (
            self._engine_inertia * engine_speed_rad_s + input_inertia * self._input_speed_rad_s()
        ) / (self._engine_inertia + input_inertia)
        if self._speed_m_s <= 0.0 and self._brake > 0.0:
            common_speed = 0.0  # a braked standing car takes up the rest: engine stalls
        self._state = ClutchState.LOCKED
        self._speed_m_s = common_speed * radius / ratio
        return common_speed
