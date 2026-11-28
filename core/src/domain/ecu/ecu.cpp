#include "domain/ecu/ecu.hpp"

#include "domain/common/constants.hpp"
#include "domain/engine/engine_model.hpp"

#include <utility>

namespace simcore {

Ecu::Ecu(Tune tune, double displacement_m3, double revLimit_rad_s, double revLimitHysteresis_rad_s,
         OverrunCutParameters overrunCut)
    : tune_(std::move(tune)), displacement_m3_(displacement_m3), revLimit_rad_s_(revLimit_rad_s),
      revResume_rad_s_(revLimit_rad_s - revLimitHysteresis_rad_s),
      revLimitHysteresis_rad_s_(revLimitHysteresis_rad_s), overrun_(overrunCut) {}

std::string validateOverrunSettings(const OverrunSettings& settings) {
    if (!(settings.retard_deg >= 0.0 && settings.retard_deg <= kMaxOverrunRetard_deg)) {
        return "overrun retard must be 0..60 deg";
    }
    if (!(settings.enrichment >= 0.0 && settings.enrichment <= kMaxOverrunEnrichment)) {
        return "overrun enrichment must be 0..50 %";
    }
    return {};
}

std::string validateLaunchSettings(const LaunchSettings& settings) {
    const double launch_rpm = units::radSToRpm(settings.launchSpeed_rad_s);
    if (!(launch_rpm >= kMinLaunch_rpm && launch_rpm <= kMaxLaunch_rpm)) {
        return "launch speed must be 2000..6500 1/min";
    }
    if (!(settings.retard_deg >= 0.0 && settings.retard_deg <= kMaxLaunchRetard_deg)) {
        return "launch retard must be 0..30 deg";
    }
    return {};
}

EcuCommand Ecu::update(double speed_rad_s, double manifoldPressure_Pa, double intakeTemperature_K,
                       double throttle) noexcept {
    updateRevLimiter(speed_rad_s);
    updateOverrun(speed_rad_s, throttle);
    const bool launchActive = updateLaunch(speed_rad_s, throttle);
    const OverrunSettings& overrun = tune_.overrun;
    const double lambdaTarget = tune_.lambdaTarget.lookup(speed_rad_s, manifoldPressure_Pa);
    double ignition_deg = tune_.ignition.lookup(speed_rad_s, manifoldPressure_Pa);
    if (limiterCut_ || (inOverrun_ && overrun.fuelCut)) {
        return {0.0, ignition_deg, lambdaTarget, true, false, false, false};
    }
    if (inOverrun_) {
        ignition_deg = ignition_deg - overrun.retard_deg;
    }
    if (launchActive) {
        ignition_deg = ignition_deg - tune_.launch.retard_deg;
    }
    // §2.2 + §2.4: the ECU's belief of the air mass, then fuel for the target lambda.
    const double believedVe = tune_.ve.lookup(speed_rad_s, manifoldPressure_Pa);
    const double believedAirPerCycle =
        believedVe * airDensity_kg_m3(manifoldPressure_Pa, intakeTemperature_K) * displacement_m3_;
    double fuelPerCycle = believedAirPerCycle / (lambdaTarget * constants::kAfrStoichGasoline);
    if (inOverrun_) {
        fuelPerCycle = fuelPerCycle * (1.0 + overrun.enrichment);
    }
    return {fuelPerCycle,
            ignition_deg,
            lambdaTarget,
            false,
            inOverrun_,
            launchActive,
            launchActive && launchCut_};
}

bool Ecu::updateLaunch(double speed_rad_s, double throttle) noexcept {
    // Launch limiter: spark cut with the same hysteresis as the rev limiter.
    const LaunchSettings& launch = tune_.launch;
    const bool active = launch.enabled && launchSwitch_ && throttle > overrun_.closedThrottle;
    if (!active || speed_rad_s < launch.launchSpeed_rad_s - revLimitHysteresis_rad_s_) {
        launchCut_ = false;
    } else if (speed_rad_s > launch.launchSpeed_rad_s) {
        launchCut_ = true;
    }
    return active;
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
