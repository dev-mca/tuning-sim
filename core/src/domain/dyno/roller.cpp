#include "domain/dyno/roller.hpp"

#include "domain/common/units.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace simcore {

RollerRun::RollerRun(const RollerParameters& parameters, CarSimulation& car)
    : parameters_(parameters),
      maxSteps_(std::lround(kRollerMaxRunTime_s / car.engine().timeStep_s())),
      rpm_(parameters.speedFrom_rpm - kRollerRunUp_rpm) {
    const EngineDefinition& definition = car.engine().definition();
    if (parameters.gear < 1 || parameters.gear > definition.drivetrain.gearCount()) {
        throw std::invalid_argument("gear must be a forward gear");
    }
    if (parameters.speedTo_rpm <= parameters.speedFrom_rpm || parameters.sampleStep_rpm <= 0.0 ||
        parameters.speedFrom_rpm - kRollerRunUp_rpm <= 0.0) {
        throw std::invalid_argument("need 300 < speedFrom_rpm < speedTo_rpm and a positive step");
    }
    const double start = units::rpmToRadS(parameters.speedFrom_rpm - kRollerRunUp_rpm);
    car.engine().setSpeed_rad_s(start);
    car.drivetrain().placeInGear(parameters.gear, start);
    // Same sample speeds as numpy.arange(from, to + step / 2, step) in the lab.
    for (int i = 0;; ++i) {
        const double rpm = parameters.speedFrom_rpm + i * parameters.sampleStep_rpm;
        if (rpm >= parameters.speedTo_rpm + 0.5 * parameters.sampleStep_rpm) {
            break;
        }
        sampleRpm_.push_back(rpm);
    }
}

CarState RollerRun::step(CarSimulation& car) {
    const CarState state = car.step(1.0);
    ++steps_;
    rpm_ = units::radSToRpm(state.engine.speed_rad_s);
    while (samples_.size() < sampleRpm_.size() && rpm_ >= sampleRpm_[samples_.size()]) {
        const EngineOutputs& engine = state.engine.engine;
        samples_.push_back({state.engine.speed_rad_s, state.drivetrain.vehicleSpeed_m_s,
                            state.drivetrain.wheelForce_N, state.drivetrain.wheelTorque_Nm,
                            engine.brakeTorque_Nm, state.engine.time_s,
                            state.engine.manifoldPressure_Pa, engine.lambdaActual,
                            state.engine.command.ignition_deg, engine.exhaustTemperature_K,
                            engine.airMassFlow_kg_s});
    }
    return state;
}

bool RollerRun::finished() const noexcept {
    return samples_.size() == sampleRpm_.size() || steps_ >= maxSteps_;
}

double RollerRun::progress() const noexcept {
    const double span = parameters_.speedTo_rpm - parameters_.speedFrom_rpm;
    return std::clamp((rpm_ - parameters_.speedFrom_rpm) / span, 0.0, 1.0);
}

std::vector<RollerSample> runRoller(const EngineDefinition& definition,
                                    const RollerParameters& parameters, std::optional<Tune> tune) {
    CarSimulation car(definition, std::move(tune));
    RollerRun run(parameters, car);
    while (!run.finished()) {
        static_cast<void>(run.step(car));
    }
    return run.samples();
}

} // namespace simcore
