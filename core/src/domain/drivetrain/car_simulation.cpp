#include "domain/drivetrain/car_simulation.hpp"

#include <utility>

namespace simcore {

CarSimulation::CarSimulation(const EngineDefinition& definition, std::optional<Tune> tune)
    : engine_(definition, std::move(tune)),
      drivetrain_(definition.drivetrain, definition.inertia_kgm2) {}

CarState CarSimulation::step(double throttle) noexcept {
    const double dt = engine_.timeStep_s();
    drivetrain_.updateControls(dt);
    engine_.ecu().setLaunchSwitch(launchButton_ || drivetrain_.launchArmed());
    engine_.ecu().setShiftCutRequest(drivetrain_.shifting());
    const EngineOutputs& outputs = engine_.prepare(throttle);
    const double load = drivetrain_.clutchTorque(outputs.brakeTorque_Nm, engine_.speed_rad_s());
    const SimulationState engineState = engine_.finish(load);
    if (const std::optional<double> locked = drivetrain_.integrate(dt, engineState.speed_rad_s)) {
        engine_.setSpeed_rad_s(*locked);
    }
    restartIfStalled();
    return {engineState, drivetrain_.state()};
}

void CarSimulation::restartIfStalled() noexcept {
    // A stalled engine (e.g. clutch released at idle in gear) cannot start itself in gear; with
    // the drivetrain decoupled the "starter" brings it back to idle.
    const EngineDefinition& definition = engine_.definition();
    const bool stalled = engine_.speed_rad_s() < definition.drivetrain.stallSpeed_rad_s;
    if (stalled && drivetrain_.decoupled()) {
        engine_.setSpeed_rad_s(definition.idleSpeed_rad_s);
    }
}

} // namespace simcore
