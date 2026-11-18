#pragma once

#include "acoustic/layout.hpp"
#include "dsp/random.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sound_spike {

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
    void process(double speed_rpm, float load, std::span<float> out) noexcept;

    [[nodiscard]] double crankAngleDeg() const noexcept { return crankAngle_deg_; }

    // research.md §6.2: EVO about 130-140 deg after firing TDC.
    static constexpr double kEvoAfterTdc_deg = 130.0;
    // Blow-down lasts roughly this crank angle; its duration in time shrinks with speed.
    static constexpr double kPulseCrankAngle_deg = 70.0;
    static constexpr double kMinPulseDuration_s = 0.6e-3; // limits bandwidth (no aliasing)
    static constexpr double kMaxPulseDuration_s = 5.0e-3;
    static constexpr double kRiseFraction = 0.3; // fast rise, slower decay
    static constexpr double kTimingJitter_deg = 0.5;
    static constexpr float kAmplitudeVariation = 0.05F; // cycle-to-cycle, standard deviation
    static constexpr float kCylinderImbalance = 0.03F;  // fixed per cylinder
    static constexpr float kMinLoadAmplitude = 0.2F;    // pulses remain even without fuel

private:
    struct Cylinder {
        double evoAngle_deg;    // nominal
        double nextTrigger_deg; // nominal + jitter of the coming cycle
        float imbalance;        // fixed amplitude factor
        bool armed = true;      // false right after a pulse until half a cycle has passed
        bool active = false;
        double pulseStart = 0.0; // in samples (fractional)
        double pulseDuration = 1.0;
        float amplitude = 0.0F;
    };

    [[nodiscard]] double jitteredTrigger(double evoAngle_deg) noexcept;
    [[nodiscard]] static float pulseShape(double phase) noexcept;

    std::vector<Cylinder> cylinders_;
    double sampleRate_;
    std::uint32_t seed_;
    Random random_;
    double crankAngle_deg_ = 0.0;
    std::int64_t sampleIndex_ = 0;
};

} // namespace sound_spike
