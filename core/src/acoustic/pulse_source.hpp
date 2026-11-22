#pragma once

#include "acoustic/layout.hpp"
#include "dsp/random.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace simcore::acoustic {

/// Exhaust pulse sources, one per cylinder (research.md §6.2, steps 1-4).
///
/// The crank angle is integrated once per audio sample. When it passes a cylinder's exhaust
/// valve opening (EVO), that cylinder emits a short, smooth (band-limited) pressure pulse.
/// The start time is computed with sub-sample precision, so the pitch stays clean even at
/// high speed where a pulse would otherwise snap to the sample grid.
class PulseSource {
public:
    PulseSource(const std::vector<CylinderSpec>& cylinders, float sampleRate, std::uint32_t seed);

    void reset() noexcept;

    /// Advances one sample and writes one pressure value per cylinder into `out`.
    /// `load` in [0, 1] scales the pulse amplitude (cylinder pressure at EVO).
    /// Returns a bit mask of the cylinders whose exhaust valve opened in this sample.
    std::uint32_t process(double speed_rpm, float load, std::span<float> out) noexcept;

    /// Bit masks returned by process() cover at most this many cylinders.
    static constexpr std::size_t kMaxCylinders = 32;

    /// Standard deviation of the cycle-to-cycle amplitude variation, applied from the next pulse
    /// on. Lean mixtures burn less evenly and sound rougher (research.md §6.2 step 3).
    void setCycleVariation(float standardDeviation) noexcept {
        amplitudeVariation_ = standardDeviation;
    }

    /// 0 = normal sharp blow-down, 1 = soft: longer pulse with a slower rise. Retarded ignition
    /// burns later and slower, so the pressure at EVO rises less abruptly (fewer harmonics,
    /// duller sound). Applied from the next pulse on, so running pulses keep their shape.
    void setSoftness(float softness) noexcept { softness_ = std::clamp(softness, 0.0F, 1.0F); }

    [[nodiscard]] double crankAngleDeg() const noexcept { return crankAngle_deg_; }

    // research.md §6.2: EVO about 130-140 deg after firing TDC.
    static constexpr double kEvoAfterTdc_deg = 130.0;
    // Blow-down lasts roughly this crank angle; its duration in time shrinks with speed.
    static constexpr double kPulseCrankAngle_deg = 70.0;
    static constexpr double kMinPulseDuration_s = 0.6e-3; // limits bandwidth (no aliasing)
    static constexpr double kMaxPulseDuration_s = 5.0e-3;
    static constexpr double kRiseFraction = 0.3;     // fast rise, slower decay
    static constexpr double kSoftRiseFraction = 0.5; // symmetric pulse at full softness
    static constexpr double kSoftWidening = 0.5;     // full softness: 1.5 x the duration
    static constexpr double kTimingJitter_deg = 0.5;
    static constexpr float kDefaultAmplitudeVariation = 0.05F; // cycle-to-cycle, std. dev.
    static constexpr float kCylinderImbalance = 0.03F;         // fixed per cylinder
    static constexpr float kMinLoadAmplitude = 0.2F;           // pulses remain even without fuel

private:
    struct Cylinder {
        double evoAngle_deg;    // nominal
        double nextTrigger_deg; // nominal + jitter of the coming cycle
        float imbalance;        // fixed amplitude factor
        bool armed = true;      // false right after a pulse until half a cycle has passed
        bool active = false;
        double pulseStart = 0.0; // in samples (fractional)
        double pulseDuration = 1.0;
        double riseFraction = kRiseFraction;
        float amplitude = 0.0F;
    };

    [[nodiscard]] double jitteredTrigger(double evoAngle_deg) noexcept;
    [[nodiscard]] static float pulseShape(double phase, double riseFraction) noexcept;

    std::vector<Cylinder> cylinders_;
    double sampleRate_;
    std::uint32_t seed_;
    Random random_;
    float amplitudeVariation_ = kDefaultAmplitudeVariation;
    float softness_ = 0.0F;
    double crankAngle_deg_ = 0.0;
    std::int64_t sampleIndex_ = 0;
};

} // namespace simcore::acoustic
