#include "domain/dyno/roller.hpp"

#include "domain/common/units.hpp"
#include "domain/drivetrain/car_simulation.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace simcore {

std::vector<RollerSample> runRoller(const EngineDefinition& definition,
                                    const RollerParameters& parameters, std::optional<Tune> tune) {
    if (parameters.gear < 1 || parameters.gear > definition.drivetrain.gearCount()) {
        throw std::invalid_argument("gear must be a forward gear");
    }
    if (parameters.speedTo_rpm <= parameters.speedFrom_rpm || parameters.sampleStep_rpm <= 0.0) {
        throw std::invalid_argument("need speedTo_rpm > speedFrom_rpm and a positive step");
    }
    CarSimulation car(definition, std::move(tune));
    const double start = units::rpmToRadS(parameters.speedFrom_rpm - kRollerRunUp_rpm);
    car.engine().setSpeed_rad_s(start);
    car.drivetrain().placeInGear(parameters.gear, start);
    // Same sample speeds as numpy.arange(from, to + step / 2, step) in the lab.
    std::vector<double> sampleRpm;
    for (int i = 0;; ++i) {
        const double rpm = parameters.speedFrom_rpm + i * parameters.sampleStep_rpm;
        if (rpm >= parameters.speedTo_rpm + 0.5 * parameters.sampleStep_rpm) {
            break;
        }
        sampleRpm.push_back(rpm);
    }
    std::vector<RollerSample> samples;
    const auto steps = std::lround(kRollerMaxRunTime_s / car.engine().timeStep_s());
    for (long step = 0; step < steps && samples.size() < sampleRpm.size(); ++step) {
        const CarState state = car.step(1.0);
        const double rpm = units::radSToRpm(state.engine.speed_rad_s);
        while (samples.size() < sampleRpm.size() && rpm >= sampleRpm[samples.size()]) {
            samples.push_back({state.engine.speed_rad_s, state.drivetrain.vehicleSpeed_m_s,
                               state.drivetrain.wheelForce_N, state.drivetrain.wheelTorque_Nm,
                               state.engine.engine.brakeTorque_Nm, state.engine.time_s});
        }
    }
    return samples;
}

} // namespace simcore
