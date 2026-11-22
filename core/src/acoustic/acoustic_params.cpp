#include "acoustic/acoustic_params.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simcore::acoustic {

namespace {

constexpr double kSecondsPerMinute = 60.0;

double radSToRpm(double speed_rad_s) {
    return speed_rad_s * kSecondsPerMinute / (2.0 * std::numbers::pi);
}

} // namespace

SoundTarget soundTargetFor(const AcousticParams& params) noexcept {
    SoundTarget target;
    target.speed_rpm = std::max(0.0, radSToRpm(params.speed_rad_s));
    if (params.fuelCut || !std::isfinite(params.lambda)) {
        target.load = 0.0F; // PulseSource keeps a minimum amplitude for the pumping pulses
        target.brightness = kFuelCutBrightness;
        return target;
    }
    const double retard = std::max(0.0, params.ignitionRetard_deg);
    const double charge = std::clamp(params.chargeRatio, 0.0, 1.0);
    // research.md §2.7/§6.2: later combustion -> more pressure left at EVO, slower burn.
    target.load = static_cast<float>(charge * (1.0 + kRetardAmplitudeGain_per_deg * retard));
    target.brightness = static_cast<float>(std::exp(-retard / kRetardDullness_deg));
    const double leanness = std::max(0.0, params.lambda - kLeanRoughnessThreshold);
    target.cycleVariation = std::min(PulseSource::kDefaultAmplitudeVariation +
                                         static_cast<float>(kLeanRoughnessGain * leanness),
                                     kMaxCycleVariation);
    return target;
}

} // namespace simcore::acoustic
