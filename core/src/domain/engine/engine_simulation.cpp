#include "domain/engine/engine_simulation.hpp"

#include "domain/common/constants.hpp"
#include "domain/engine/intake.hpp"

#include <algorithm>
#include <utility>

namespace simcore {

EngineSimulation::EngineSimulation(const EngineDefinition& definition, std::optional<Tune> tune,
                                   const Table2D* trueVe, std::optional<double> initialSpeed_rad_s,
                                   double timeStep_s)
    : definition_(definition), model_(definition, trueVe),
      ecu_(tune ? std::move(*tune) : Tune::fromTables(definition.tables),
           definition.displacement_m3(), definition.revLimit_rad_s,
           definition.revLimitHysteresis_rad_s, definition.overrunCut),
      timeStep_s_(timeStep_s),
      speed_rad_s_(initialSpeed_rad_s.value_or(definition.idleSpeed_rad_s)),
      manifoldPressure_Pa_(constants::kAmbientPressure_Pa),
      manifoldTemperature_K_(constants::kAmbientTemperature_K),
      catalystTemperature_K_(definition.afterburn.catalystInitial_K) {}

SimulationState EngineSimulation::step(double throttle, double loadTorque_Nm) noexcept {
    const double dt = timeStep_s_;
    const EcuCommand command =
        ecu_.update(speed_rad_s_, manifoldPressure_Pa_, manifoldTemperature_K_, throttle);
    integrateManifold(throttle, dt);
    const EngineOutputs outputs =
        model_.evaluate({speed_rad_s_, manifoldPressure_Pa_, manifoldTemperature_K_,
                         command.fuelMassPerCycle_kg, command.ignition_deg});
    // Semi-implicit Euler: new speed from the current torque balance.
    const double acceleration = (outputs.brakeTorque_Nm - loadTorque_Nm) / definition_.inertia_kgm2;
    speed_rad_s_ = std::max(0.0, speed_rad_s_ + dt * acceleration);
    updateCatalyst(outputs, dt);
    time_s_ += dt;
    return {time_s_, speed_rad_s_, manifoldPressure_Pa_,  throttle, loadTorque_Nm,
            command, outputs,      catalystTemperature_K_};
}

void EngineSimulation::updateCatalyst(const EngineOutputs& outputs, double dt) noexcept {
    // First-order lag towards exhaust temperature plus afterburn heat (research.md §2.10a).
    const double target = outputs.exhaustTemperature_K + outputs.afterburnTemperatureRise_K;
    const double timeConstant = definition_.afterburn.catalystTimeConstant_s;
    catalystTemperature_K_ =
        catalystTemperature_K_ + dt * (target - catalystTemperature_K_) / timeConstant;
}

void EngineSimulation::integrateManifold(double throttle, double dt) noexcept {
    const IntakeParameters& intakeParameters = definition_.intake;
    const double area = intake::throttleArea_m2(throttle, intakeParameters);
    const double subDt = dt / kManifoldSubsteps;
    double pressure = manifoldPressure_Pa_;
    for (int substep = 0; substep < kManifoldSubsteps; ++substep) {
        const double throttleFlow = intake::orificeMassFlow_kg_s(
            area, intakeParameters.throttleDischargeCoefficient, constants::kAmbientPressure_Pa,
            constants::kAmbientTemperature_K, pressure);
        const double cylinderFlow =
            model_.airMassFlow_kg_s(speed_rad_s_, pressure, manifoldTemperature_K_);
        const double derivative = intake::manifoldPressureDerivative_Pa_s(
            throttleFlow, cylinderFlow, manifoldTemperature_K_, intakeParameters.manifoldVolume_m3);
        pressure = std::max(kMinManifoldPressure_Pa, pressure + subDt * derivative);
    }
    manifoldPressure_Pa_ = pressure;
}

} // namespace simcore
