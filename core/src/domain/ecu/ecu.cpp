#include "domain/ecu/ecu.hpp"

#include "domain/common/constants.hpp"
#include "domain/engine/engine_model.hpp"

#include <utility>

namespace simcore {

Ecu::Ecu(Tune tune, double displacement_m3, double revLimit_rad_s, double revLimitHysteresis_rad_s,
         OverrunCutParameters overrunCut)
    : tune_(std::move(tune)), displacement_m3_(displacement_m3), revLimit_rad_s_(revLimit_rad_s),
      revResume_rad_s_(revLimit_rad_s - revLimitHysteresis_rad_s), overrun_(overrunCut) {}

EcuCommand Ecu::update(double speed_rad_s, double manifoldPressure_Pa, double intakeTemperature_K,
                       double throttle) noexcept {
    updateRevLimiter(speed_rad_s);
    updateOverrunCut(speed_rad_s, throttle);
    const double lambdaTarget = tune_.lambdaTarget.lookup(speed_rad_s, manifoldPressure_Pa);
    const double ignition_deg = tune_.ignition.lookup(speed_rad_s, manifoldPressure_Pa);
    if (limiterCut_ || overrunCut_) {
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
        limiterCut_ = true;
    } else if (speed_rad_s < revResume_rad_s_) {
        limiterCut_ = false;
    }
}

void Ecu::updateOverrunCut(double speed_rad_s, double throttle) noexcept {
    // Cut when coasting with closed throttle; resume below the exit speed (before idle, so the
    // engine catches itself) or as soon as the driver opens the throttle.
    const bool closed = throttle <= overrun_.closedThrottle;
    if (!closed || speed_rad_s < overrun_.exit_rad_s) {
        overrunCut_ = false;
    } else if (speed_rad_s > overrun_.enter_rad_s) {
        overrunCut_ = true;
    }
}

} // namespace simcore
