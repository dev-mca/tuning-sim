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
    updateOverrun(speed_rad_s, throttle);
    const OverrunSettings& overrun = tune_.overrun;
    const double lambdaTarget = tune_.lambdaTarget.lookup(speed_rad_s, manifoldPressure_Pa);
    double ignition_deg = tune_.ignition.lookup(speed_rad_s, manifoldPressure_Pa);
    if (limiterCut_ || (inOverrun_ && overrun.fuelCut)) {
        return {0.0, ignition_deg, lambdaTarget, true, false};
    }
    if (inOverrun_) {
        ignition_deg = ignition_deg - overrun.retard_deg;
    }
    // §2.2 + §2.4: the ECU's belief of the air mass, then fuel for the target lambda.
    const double believedVe = tune_.ve.lookup(speed_rad_s, manifoldPressure_Pa);
    const double believedAirPerCycle =
        believedVe * airDensity_kg_m3(manifoldPressure_Pa, intakeTemperature_K) * displacement_m3_;
    double fuelPerCycle = believedAirPerCycle / (lambdaTarget * constants::kAfrStoichGasoline);
    if (inOverrun_) {
        fuelPerCycle = fuelPerCycle * (1.0 + overrun.enrichment);
    }
    return {fuelPerCycle, ignition_deg, lambdaTarget, false, inOverrun_};
}

void Ecu::updateRevLimiter(double speed_rad_s) noexcept {
    // Hysteresis: cut above the limit, resume only below limit - hysteresis.
    if (speed_rad_s > revLimit_rad_s_) {
        limiterCut_ = true;
    } else if (speed_rad_s < revResume_rad_s_) {
        limiterCut_ = false;
    }
}

void Ecu::updateOverrun(double speed_rad_s, double throttle) noexcept {
    // Overrun = coasting with closed throttle above enter speed; it ends below the exit speed
    // (before idle, so the engine catches itself) or when the throttle opens.
    const bool closed = throttle <= overrun_.closedThrottle;
    if (!closed || speed_rad_s < overrun_.exit_rad_s) {
        inOverrun_ = false;
    } else if (speed_rad_s > overrun_.enter_rad_s) {
        inOverrun_ = true;
    }
}

} // namespace simcore
