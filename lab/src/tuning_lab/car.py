"""Engine + drivetrain on the roller dyno, one fixed time step at a time (iteration 10).

Order of a step (research.md §8.3, §9):
1. gearbox and clutch controls (shift phases, pedal), driver aids for the ECU (launch armed by
   the clutch, shift cut for flat shift),
2. ECU and engine torque at the current speed (EngineSimulation.prepare),
3. clutch torque from the drivetrain = load on the crank,
4. crank integration (EngineSimulation.finish), then the car; a locking clutch sets the common
   speed,
5. starter: a stalled engine restarts once the drivetrain is decoupled.
"""

from dataclasses import dataclass

from tuning_lab.drivetrain import Drivetrain, DrivetrainState
from tuning_lab.ecu import Tune
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.simulation import EngineSimulation, SimulationState


@dataclass(frozen=True)
class CarState:
    engine: SimulationState
    drivetrain: DrivetrainState


class CarSimulation:
    def __init__(self, definition: EngineDefinition, tune: Tune | None = None) -> None:
        self._definition = definition
        self._engine = EngineSimulation(definition, tune)
        self._drivetrain = Drivetrain(definition.drivetrain, definition.inertia_kg_m2)
        self._launch_button = False

    @property
    def engine(self) -> EngineSimulation:
        return self._engine

    @property
    def drivetrain(self) -> Drivetrain:
        return self._drivetrain

    def set_launch_button(self, held: bool) -> None:
        """The launch button of iteration 7c; the clutch pedal arms the 2-step too."""
        self._launch_button = held

    def step(self, throttle: float) -> CarState:
        dt = self._engine.time_step_s
        drivetrain = self._drivetrain
        drivetrain.update_controls(dt)
        self._engine.set_launch_switch(self._launch_button or drivetrain.launch_armed())
        self._engine.set_shift_cut_request(drivetrain.shifting)
        outputs = self._engine.prepare(throttle)
        load = drivetrain.clutch_torque(outputs.brake_torque_nm, self._engine.speed_rad_s)
        engine_state = self._engine.finish(load)
        locked_speed = drivetrain.integrate(dt, engine_state.speed_rad_s)
        if locked_speed is not None:
            self._engine.set_speed_rad_s(locked_speed)
        self._restart_if_stalled()
        return CarState(engine_state, drivetrain.state())

    def _restart_if_stalled(self) -> None:
        # A stalled engine (e.g. clutch released at idle in gear) cannot start itself in gear;
        # with the drivetrain decoupled the "starter" brings it back to idle.
        stalled = self._engine.speed_rad_s < self._definition.drivetrain.stall_speed_rad_s
        if stalled and self._drivetrain.decoupled:
            self._engine.set_speed_rad_s(self._definition.idle_speed_rad_s)
