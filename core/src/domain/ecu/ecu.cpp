#include "domain/ecu/ecu.hpp"

#include "domain/common/constants.hpp"
#include "domain/engine/engine_model.hpp"

#include <utility>

namespace simcore {

Ecu::Ecu(Tune tune, double displacement_m3, double revLimit_rad_s, double revLimitHysteresis_rad_s)
    : tune_(std::move(tune)), displacement_m3_(displacement_m3), revLimit_rad_s_(revLimit_rad_s),
      revResume_rad_s_(revLimit_rad_s - revLimitHysteresis_rad_s) {}

EcuCommand Ecu::update(double speed_rad_s, double manifoldPressure_Pa,
                       double intakeTemperature_K) noexcept {
    updateRevLimiter(speed_rad_s);
    const double lambdaTarget = tune_.lambdaTarget.lookup(speed_rad_s, manifoldPressure_Pa);
    const double ignition_deg = tune_.ignition.lookup(speed_rad_s, manifoldPressure_Pa);
    if (fuelCut_) {
        return {0.0, ignition_deg, lambdaTarget, true};
    }
    // §2.2 + §2.4: the ECU's belief of the air mass, then fuel for the target lambda.
    const double believedVe = tune_.ve.lookup(speed_rad_s, manifoldPressure_Pa);
    const double believedAirPerCycle =
        believedVe * airDensity_kg_m3(manifoldPressure_Pa, intakeTemperature_K) * displacement_m3_;
    const double fuelPerCycle =
        believedAirPerCycle / (lambdaTarget * constants::kAfrStoichGasoline);
    return {fuelPerCycle, ignition_deg, lambdaTarget, false};
}

void Ecu::updateRevLimiter(double speed_rad_s) noexcept {
    // Hysteresis: cut above the limit, resume only below limit - hysteresis.
    if (speed_rad_s > revLimit_rad_s_) {
        fuelCut_ = true;
    } else if (speed_rad_s < revResume_rad_s_) {
        fuelCut_ = false;
    }
}

} // namespace simcore
