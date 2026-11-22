#include "acoustic/pulse_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace simcore::acoustic {

namespace {

constexpr double kCycleDeg = 720.0;
constexpr double kDegPerRevolution = 360.0;
constexpr double kSecondsPerMinute = 60.0;
constexpr double kRearmDistance_deg = 180.0;

double wrapAngle(double angle_deg) {
    angle_deg = std::fmod(angle_deg, kCycleDeg);
    return angle_deg < 0.0 ? angle_deg + kCycleDeg : angle_deg;
}

/// Fraction in (0, 1] of the step [previous, previous + step] at which `target` is passed,
/// or a negative value if it is not passed. Handles the wrap at 720 deg.
double crossingFraction(double previous_deg, double step_deg, double target_deg) {
    if (step_deg <= 0.0) {
        return -1.0;
    }
    double target = target_deg;
    if (target <= previous_deg) {
        target += kCycleDeg;
    }
    const double distance = target - previous_deg;
    return distance <= step_deg ? distance / step_deg : -1.0;
}

} // namespace

PulseSource::PulseSource(const std::vector<CylinderSpec>& cylinders, float sampleRate,
                         std::uint32_t seed)
    : sampleRate_(static_cast<double>(sampleRate)), seed_(seed), random_(seed) {
    cylinders_.reserve(cylinders.size());
    for (const CylinderSpec& spec : cylinders) {
        const double evo = wrapAngle(spec.firingAngle_deg + kEvoAfterTdc_deg);
        cylinders_.push_back({evo, evo, 1.0F});
    }
    reset();
}

void PulseSource::reset() noexcept {
    random_ = Random(seed_);
    crankAngle_deg_ = 0.0;
    sampleIndex_ = 0;
    for (Cylinder& cylinder : cylinders_) {
        cylinder.imbalance = 1.0F + kCylinderImbalance * random_.uniform();
        cylinder.nextTrigger_deg = jitteredTrigger(cylinder.evoAngle_deg);
        cylinder.armed = true;
        cylinder.active = false;
    }
}

double PulseSource::jitteredTrigger(double evoAngle_deg) noexcept {
    return wrapAngle(evoAngle_deg + kTimingJitter_deg * static_cast<double>(random_.uniform()));
}

float PulseSource::pulseShape(double phase, double riseFraction) noexcept {
    // Smooth (continuous slope) raised-cosine pulse with a fast rise and a slower decay.
    // Smoothness keeps the spectrum band-limited; hard rectangles would alias (§6.2 step 4).
    if (phase < 0.0 || phase >= 1.0) {
        return 0.0F;
    }
    const double pi = std::numbers::pi;
    const double value =
        phase < riseFraction
            ? 0.5 * (1.0 - std::cos(pi * phase / riseFraction))
            : 0.5 * (1.0 + std::cos(pi * (phase - riseFraction) / (1.0 - riseFraction)));
    return static_cast<float>(value);
}

std::uint32_t PulseSource::process(double speed_rpm, float load, std::span<float> out) noexcept {
    const double step_deg = speed_rpm / kSecondsPerMinute * kDegPerRevolution / sampleRate_;
    const double previous = crankAngle_deg_;
    const double now = static_cast<double>(sampleIndex_);
    const double degPerSecond = std::max(step_deg * sampleRate_, 1.0e-9);
    const double softness = static_cast<double>(softness_);
    const double durationSamples =
        std::clamp(kPulseCrankAngle_deg / degPerSecond, kMinPulseDuration_s, kMaxPulseDuration_s) *
        (1.0 + kSoftWidening * softness) * sampleRate_;
    const double riseFraction = kRiseFraction + (kSoftRiseFraction - kRiseFraction) * softness;
    const float loadAmplitude = kMinLoadAmplitude + (1.0F - kMinLoadAmplitude) * load;
    std::uint32_t opened = 0;

    for (std::size_t k = 0; k < cylinders_.size(); ++k) {
        Cylinder& cylinder = cylinders_[k];
        // Re-arm half a cycle away from EVO: the jitter of the next cycle may move the trigger
        // slightly behind the one that just fired, which must not fire again immediately.
        const double fromEvo = wrapAngle(previous - cylinder.evoAngle_deg);
        if (fromEvo > kRearmDistance_deg && fromEvo < kCycleDeg - kRearmDistance_deg) {
            cylinder.armed = true;
        }
        const double fraction = crossingFraction(previous, step_deg, cylinder.nextTrigger_deg);
        if (cylinder.armed && fraction > 0.0) {
            cylinder.armed = false;
            cylinder.active = true;
            if (k < kMaxCylinders) {
                opened |= 1U << k;
            }
            cylinder.pulseStart = now - 1.0 + fraction; // between the previous and this sample
            cylinder.pulseDuration = durationSamples;
            cylinder.riseFraction = riseFraction;
            cylinder.amplitude = loadAmplitude * cylinder.imbalance *
                                 (1.0F + amplitudeVariation_ * random_.gaussian());
            cylinder.nextTrigger_deg = jitteredTrigger(cylinder.evoAngle_deg);
        }
        float value = 0.0F;
        if (cylinder.active) {
            const double phase = (now - cylinder.pulseStart) / cylinder.pulseDuration;
            value = cylinder.amplitude * pulseShape(phase, cylinder.riseFraction);
            cylinder.active = phase < 1.0;
        }
        out[k] = value;
    }
    crankAngle_deg_ = wrapAngle(previous + step_deg);
    ++sampleIndex_;
    return opened;
}

} // namespace simcore::acoustic
